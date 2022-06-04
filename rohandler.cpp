#include "rohandler.h"
#include "util.h"

#define UNINITIALIZED_FIELD 0xFFFFFFFF

const u32 kBlockSize = 0x200;
std::vector<ROLinker::ModuleInfo> moduleTable;



//-----------------------------------Utility functions below--------------------------------------------



static qstring parseSingleLineString(linput_t* li, u32 nameAddress) {
	qstring result;
	u32 charindex = nameAddress;
	while (true) {
		u8 character;
		qlseek(li, charindex);
		qlread(li, &character, sizeof(u8));
		if (character == '\0')
			break;
		result += character;
		charindex += sizeof(u8);
	}
	return result;
}

static qstring decToHex(u32 dec) {
	qstring hex;
	while (dec > 0) {
		u32 rem = dec % 16;
		if (rem > 9) {
			switch (rem) {
			case 10: hex.insert("A"); break;
			case 11: hex.insert("B"); break;
			case 12: hex.insert("C"); break;
			case 13: hex.insert("D"); break;
			case 14: hex.insert("E"); break;
			case 15: hex.insert("F"); break;
			}
		}
		else {
			hex.insert(std::to_string(rem).c_str());
		}
		dec = dec / 16;
	}
	return hex;
}

static u32 DecodeTag(std::vector<u64> segmentTable, ROLinker::SegmentTag tag) {
	return segmentTable[tag.segment_index] + tag.offset_into_segment;
}

static void do_import_name(u64 address, qstring name, u32 patchedvalue = 0) {
	qstring import_name = "_import_";
	force_name(address, import_name.append(name).c_str());
	if (patchedvalue > 0) {
		patch_dword(address, patchedvalue);
		fixup_data_t f = fixup_data_t();
		f.set_type(FIXUP_OFF32);
		f.off = patchedvalue;
		set_fixup(address, f);
	}
    //guess wrapper
	if (get_dword(address - 4) == 0xE51FF004)
		force_name(address - 4, name.c_str());
}

static void do_import_batch(linput_t* li, std::vector<u64> segmentAddressTable, u32 batchAddress, qstring fullName, u32 addr = -1) {
	while (true) {
		qstring name = fullName;
		qlseek(li, batchAddress);
		ROLinker::RelocationEntry entry;
		qlread(li, &entry, sizeof(ROLinker::RelocationEntry));
		u32 target_offset = DecodeTag(segmentAddressTable, entry.target);
		if (addr == -1)
			do_import_name(target_offset, name);
		else {
			u32 shift_final = entry.addend;
			if (entry.patch_type == 3)
				shift_final -= target_offset;
			do_import_name(target_offset, name.append("_0x").append(decToHex(addr).c_str()), addr );
		}
		if (entry.source != 0)
			break;
		batchAddress = batchAddress + sizeof(ROLinker::RelocationEntry);
	}
}



//-----------------------------------RO Loader functions below--------------------------------------------



void applyCRS(linput_t* li, u32 actual_romfs_address, u32 exefscode_offset){
	loadRelocatableObject(li, exefscode_offset, RomFS::findCRS(li, actual_romfs_address), true);
}

void applyCROs(linput_t* li, u32 actual_romfs_address, u32 offset_to_load_cros) {
	qvector<u64> croAddresses = RomFS::findCROs(li, actual_romfs_address);
	u64 currentAddressToLoadAt = offset_to_load_cros;
	for (u32 i = 0; i < croAddresses.size(); i++) {
		currentAddressToLoadAt = loadRelocatableObject(li, currentAddressToLoadAt, croAddresses[i], false);
	}
	ROLinker::resolveModuleImports(li);
}

u64 loadRelocatableObject(linput_t* li, u64 idb_address_to_load_at, u64 ro_file_address, bool isCrs) {
	CRO_Header header;
	ROLinker::ModuleInfo info;
	qlseek(li, ro_file_address + 0x80);
	qlread(li, &header, sizeof(CRO_Header));

	if (header.Magic != MakeMagic('C', 'R', 'O', '0'))
		warning("CRS/CRO file not found properly!");
	info.name = parseSingleLineString(li, ro_file_address + header.NameOffset);
	info.header = header;
	info.ro_file_address = ro_file_address;

	if (!isCrs) {
		file2base(li, ro_file_address, idb_address_to_load_at, idb_address_to_load_at + header.FileSize, 0);

		set_selector(1, 0);
		add_segm(1, idb_address_to_load_at, idb_address_to_load_at + 0x80, (info.name+".hash").c_str(), CLASS_CONST);

		set_selector(2, 0);
		add_segm(2, idb_address_to_load_at + 0x80, idb_address_to_load_at + 0x80 + sizeof(header), (info.name+".header").c_str(), CLASS_CONST);

		set_selector(3, 0);
		add_segm(3, idb_address_to_load_at + header.SegmentTableOffset, idb_address_to_load_at + header.DataOffset, (info.name+".tables").c_str(), CLASS_CONST);
	}

	qstring segmentDic[4][2]{
		{"CODE", ".text"},
		{"CONST", ".rodata"},
		{"DATA", ".data"},
		{"BSS", ".bss"}
	};
	u64 segmentEntryaddress = ro_file_address + header.SegmentTableOffset;
	u32 bssSize = 0;
	for (u32 i = 0; i < header.SegmentNum; i++) {
		qlseek(li, segmentEntryaddress);
		ROLinker::SegmentTableEntry entry;
		qlread(li, &entry, sizeof(ROLinker::SegmentTableEntry));
		if (entry.SegmentType == 3) {
			entry.SegmentOffset = header.DataOffset + header.DataSize;
			bssSize = entry.SegmentSize;
			if (!isCrs)
				enable_flags(idb_address_to_load_at + entry.SegmentOffset, idb_address_to_load_at + entry.SegmentOffset + entry.SegmentSize, STT_VA);
		}
		u64 segAddress = idb_address_to_load_at + entry.SegmentOffset;
		info.segmentAddresses.push_back(segAddress);
		if (entry.SegmentSize > 0 && !isCrs)
			add_segm(0, info.segmentAddresses[i], info.segmentAddresses[i] + entry.SegmentSize, (info.name+segmentDic[entry.SegmentType][1]).c_str(), segmentDic[entry.SegmentType][0].c_str());
		segmentEntryaddress += sizeof(ROLinker::SegmentTableEntry);
	}
	
	ROLinker::handleInternalRelocations(li, info);
	ROLinker::handleExports(li, info);

	moduleTable.push_back(info);
	return idb_address_to_load_at + header.DataOffset + header.DataSize + bssSize;
}



//-----------------------------------RomFS functions below--------------------------------------------



u32 RomFS::findActualRomFSOffset(linput_t* li, u32 ivfc_offset)
{
	RomFS::IVFC::RomFS_IVFCHeader ivfcheader;
	u32 ivfc_off = ivfc_offset * kBlockSize;
	qlseek(li, ivfc_off);
	qlread(li, &ivfcheader, sizeof(RomFS::IVFC::RomFS_IVFCHeader));
	if (ivfcheader.magicid != 0x10000 || MakeMagic('I','V','F','C') != ivfcheader.magic)
		warning("IVFC not found properly!");
	return ivfc_off + align64(align64(sizeof(RomFS::IVFC::RomFS_IVFCHeader), 16) + ivfcheader.masterhashsize, 1 << ivfcheader.level3.blocksize);
}

RomFS::Directory_Metadata RomFS::findRootDir(linput_t* li, u32 actual_romfs_offset)
{
	RomFS::RomFS_Header header;
	qlseek(li, actual_romfs_offset);
	qlread(li, &header, sizeof(RomFS::RomFS_Header));

	u32 metadata_table_address = actual_romfs_offset + header.dir_metadata_table_offset;
	RomFS::Directory_Metadata root_dir;
	qlseek(li, metadata_table_address);
	qlread(li, &root_dir, sizeof(RomFS::Directory_Metadata));

	return root_dir;
}

u64 RomFS::findCRS(linput_t* li, u32 actual_romfs_address)
{
	RomFS::RomFS_Header header;
	qlseek(li, actual_romfs_address);
	qlread(li, &header, sizeof(RomFS::RomFS_Header));

	RomFS::File_Metadata current_file;
	u32 current_file_offset = header.file_metadata_table_offset;
	while (current_file_offset != UNINITIALIZED_FIELD) {
		u32 current_file_address = actual_romfs_address + current_file_offset;
		qlseek(li, current_file_address);
		qlread(li, &current_file, sizeof(RomFS::File_Metadata));
		if (current_file.name_length == 0) {
			current_file_offset = header.file_metadata_table_offset + current_file.sibling_file_offset;
			continue;
		}
		qwstring filename{};
		filename.resize(current_file.name_length);
		qlseek(li, current_file_address + sizeof(RomFS::File_Metadata));
		qlread(li, filename.begin(), filename.size() * sizeof(filename[0]));
		const wchar16_t* crsformat = L".crs";
		const wchar16_t * extension = filename.substr(filename.rfind('.')).c_str();
		if (!filename.empty() && filename.rfind('.') != qwstring::npos && wmemcmp(extension, crsformat, 4) == 0) {
			qlseek(li, actual_romfs_address + current_file.actual_file_data_offset); 
			//info("Static.crs file located at offset %d!", actual_romfs_offset + header.filedataoffset + current_file.actual_file_data_offset);
			return actual_romfs_address + header.filedataoffset + current_file.actual_file_data_offset;
		}
		current_file_offset = header.file_metadata_table_offset + current_file.sibling_file_offset;
	}
 
}

qvector<u64> RomFS::findCROs(linput_t* li, u32 actual_romfs_address)
{
	qvector<u64> croAddresses;
	RomFS::RomFS_Header header;
	qlseek(li, actual_romfs_address);
	qlread(li, &header, sizeof(RomFS::RomFS_Header));

	RomFS::File_Metadata current_file;
	u32 current_file_offset = header.file_metadata_table_offset;
	while (current_file_offset != UNINITIALIZED_FIELD) {
		u32 current_file_address = actual_romfs_address + current_file_offset;
		qlseek(li, current_file_address);
		qlread(li, &current_file, sizeof(RomFS::File_Metadata));
		if (current_file.name_length == 0) {
			if (current_file.sibling_file_offset == UNINITIALIZED_FIELD) //doesn't seem to happen, but just in case.
				break;
			current_file_offset = header.file_metadata_table_offset + current_file.sibling_file_offset;
			continue;
		}
		qwstring filename{};
		filename.resize(current_file.name_length);
		qlseek(li, current_file_address + sizeof(RomFS::File_Metadata));
		qlread(li, filename.begin(), filename.size() * sizeof(filename[0]));
		const wchar16_t* croformat = L".cro";
		const wchar16_t* extension = filename.substr(filename.rfind('.')).c_str();
		if (!filename.empty() && filename.rfind('.') != qwstring::npos && wmemcmp(extension, croformat, 4) == 0) {
			qlseek(li, actual_romfs_address + current_file.actual_file_data_offset);
			croAddresses.push_back(actual_romfs_address + header.filedataoffset + current_file.actual_file_data_offset);
		}
		if (current_file.sibling_file_offset == UNINITIALIZED_FIELD)
			break;
		current_file_offset = header.file_metadata_table_offset + current_file.sibling_file_offset;
	}

	return croAddresses;
}

/*static u32 hash(u8* filename, u32 name_length, u32 parentdiroffset) { //hash function directly taken from 3DBrew and translated to C++
	u32 hash = parentdiroffset ^ 123456789;
	for (u32 i = 0; i < name_length; i += 2) {
		hash = (hash >> 5) | (hash << 27);
		hash ^= (u16)((filename[i]) | (filename[i + 1] << 8));
	}
	return hash;
}*/



//-----------------------------------ROLinker functions below--------------------------------------------



void ROLinker::handleInternalRelocations(linput_t* li, ModuleInfo info) {
	u32 tableEntryAddress = info.ro_file_address + info.header.InternalPatchTableOffset;
	for (u32 i = 0; i < info.header.InternalPatchNum; i++) {
		ROLinker::RelocationEntry entry;
		qlseek(li, tableEntryAddress);
		qlread(li, &entry, sizeof(ROLinker::RelocationEntry));
		u32 target_address = DecodeTag(info.segmentAddresses, entry.target);
		u32 source_address = info.segmentAddresses[entry.source] + entry.addend;
		u32 value;
		if (entry.patch_type == 2)
			value = source_address;
		else if (entry.patch_type == 3) {
			u32 rel = source_address - target_address;
			if (rel < 0)
				rel += 0x100000000; //is this the amount of userland memory?
			value = rel;
		}
		patch_dword(target_address, value);
		fixup_data_t f = fixup_data_t();
		f.set_type(FIXUP_OFF32);
		f.off = value;
		set_fixup(target_address, f);
		tableEntryAddress += sizeof(ROLinker::RelocationEntry);
	}
}

void ROLinker::handleExports(linput_t* li, ModuleInfo info) {
	u32 exportNamedEntryAddress = info.ro_file_address + info.header.ExportNamedSymbolTableOffset;
	for (u32 i = 0; i < info.header.ExportNamedSymbolNum; i++) {
		qlseek(li, exportNamedEntryAddress);
		NamedExportTableEntry entry;
		qlread(li, &entry, sizeof(NamedExportTableEntry));
		info.namedExportTable.push_back(entry);
		u32 target_offset = DecodeTag(info.segmentAddresses, entry.target);
		qstring name = parseSingleLineString(li, info.ro_file_address + entry.nameOffset);
		if (segtype(target_offset) == SEG_CODE)
			target_offset &= ~1;
		add_entry(target_offset, target_offset, name.c_str(), segtype(target_offset) == SEG_CODE);
		make_name_public(target_offset);
		exportNamedEntryAddress += sizeof(NamedExportTableEntry);
	}

	u32 exportIndexedEntryAddress = info.ro_file_address + info.header.ExportIndexedSymbolTableOffset;
	for (u32 i = 0; i < info.header.ExportIndexedSymbolNum; i++) {
		qlseek(li, exportIndexedEntryAddress);
		SegmentTag segmentOffsetForExport;
		qlread(li, &segmentOffsetForExport, sizeof(u32));
		info.indexedExportTagTable.push_back(segmentOffsetForExport);
		u32 target_offset = DecodeTag(info.segmentAddresses, segmentOffsetForExport);
		if (segtype(target_offset) == SEG_CODE)
			target_offset &= ~1;
		qstring indexExp = "indexedExport_";
		add_entry(target_offset, target_offset, indexExp.append(std::to_string(i).c_str()).c_str(), segtype(target_offset) == SEG_CODE);
		make_name_public(target_offset);
		exportIndexedEntryAddress += sizeof(u32);
	}
}

void ROLinker::resolveModuleImports(linput_t* li)
{
	if (moduleTable.size() == 0)
		return;

	for (ModuleInfo currentModule : moduleTable) {
		ModuleInfo targetModule;
		bool isTargetModuleFound = false;
		
		u32 importEntryAddress = currentModule.ro_file_address + currentModule.header.ImportNamedSymbolTableOffset;
		for (u32 i = 0; i < currentModule.header.ImportNamedSymbolNum; i++) {
			qlseek(li, importEntryAddress);
			ROLinker::NamedImportTableEntry entry;
			qlread(li, &entry, sizeof(ROLinker::NamedImportTableEntry));
			qstring name = parseSingleLineString(li, currentModule.ro_file_address + entry.nameOffset);
			SegmentTag targetTag;
			if (moduleTable.size() > 0) {
				for (u32 i = 0; i < moduleTable.size(); i++) {
					for (u32 b = 0; b < moduleTable[i].namedExportTable.size(); b++) {
						if (parseSingleLineString(li, moduleTable[i].ro_file_address + moduleTable[i].namedExportTable[b].nameOffset) == name)
							targetTag = moduleTable[i].namedExportTable[b].target;
							targetModule = moduleTable[i];
							qlseek(li, entry.nameOffset);
							qstring nI = "namedImport_";
							do_import_batch(li, currentModule.segmentAddresses, currentModule.ro_file_address + entry.batchOffset, nI + name, DecodeTag(targetModule.segmentAddresses, targetTag));
					}
				}
			}
			importEntryAddress += sizeof(ROLinker::NamedImportTableEntry);
		}


		u32 importModuleAddress = currentModule.ro_file_address + currentModule.header.ImportModuleTableOffset;
		for (u32 i = 0; i < currentModule.header.ImportModuleNum; i++) {
			qlseek(li, importModuleAddress);
			ROLinker::ModuleImportTableEntry entry;
			qlread(li, &entry, sizeof(ROLinker::ModuleImportTableEntry));

			qstring modulename = parseSingleLineString(li, currentModule.ro_file_address + entry.moduleNameOffset);

			for (u32 i = 0; i < moduleTable.size(); i++) {
				if (modulename == moduleTable[i].name) {
					targetModule = moduleTable[i];
					isTargetModuleFound = true;
				}
			}

			u32 indexedAddress = currentModule.ro_file_address + entry.indexed;
			for (u32 i = 0; i < entry.indexedNum; i++) {
				qlseek(li, indexedAddress);
				ROLinker::IndexedTableEntry indexEntry;
				qlread(li, &indexEntry, sizeof(ROLinker::IndexedTableEntry));
				qstring index = qstring(std::to_string(indexEntry.index).c_str());
				if(isTargetModuleFound == true)
					do_import_batch(li, currentModule.segmentAddresses, currentModule.ro_file_address + indexEntry.batchOffset, (modulename+"_index").append(index), DecodeTag(targetModule.segmentAddresses, targetModule.indexedExportTagTable[indexEntry.index]));
				indexedAddress += sizeof(ROLinker::IndexedTableEntry);
			}

			u32 anonymousAddress = currentModule.ro_file_address + entry.anonymous;
			for (u32 i = 0; i < entry.anonymousNum; i++) {
				qlseek(li, anonymousAddress);
				ROLinker::AnonymousImportEntry anonEntry;
				qlread(li, &anonEntry, sizeof(ROLinker::AnonymousImportEntry));
				if(isTargetModuleFound == true)
					do_import_batch(li, currentModule.segmentAddresses, currentModule.ro_file_address + anonEntry.batchOffset, modulename, DecodeTag(targetModule.segmentAddresses, anonEntry.tag));
				anonymousAddress += sizeof(ROLinker::AnonymousImportEntry);
			}
			isTargetModuleFound = false;
			importModuleAddress += sizeof(ROLinker::ModuleImportTableEntry);
		}
	}
}
