#include "rohandler.h"
#include "util.h"

#define UNINITIALIZED_FIELD 0xFFFFFFFF
const u32 kBlockSize = 0x200;



//-----------------------------------Utility functions below--------------------------------------------



u32 ROHandler::decodeTag(std::vector<u64> segmentTable, SegmentTag tag) {
	return segmentTable[tag.segment_index] + tag.offset_into_segment;
}

void ROHandler::patchImport(u64 address, qstring importname, u32 patchedvalue) {
	qstring import_name = "_import_";
	force_name(address, import_name.append(importname).c_str());
	if (patchedvalue > 0) {
		patch_dword(address, patchedvalue);
		fixup_data_t f = fixup_data_t();
		f.set_type(FIXUP_OFF32);
		f.off = patchedvalue;
		set_fixup(address, f);
	}
    //guess wrapper
	if (get_dword(address - 4) == 0xE51FF004) //0xE51FF004 == "ldr pc, [pc, #-4]"
		force_name(address - 4, importname.c_str());
}

void ROHandler::patchImportBatch(std::vector<u64> segmentAddressTable, u32 batchAddress, qstring fullName, u32 addr) {
	while (true) {
		qstring name = fullName;
		qlseek(m_file_buffer, batchAddress);
		RelocationEntry entry;
		qlread(m_file_buffer, &entry, sizeof(RelocationEntry));
		u32 target_offset = decodeTag(segmentAddressTable, entry.target);
		if (addr == -1)
			patchImport(target_offset, name);
		else {
			u32 shift_final = entry.addend;
			if (entry.patch_type == 3)
				shift_final -= target_offset;
			patchImport(target_offset, name.append("_0x").append(decToHex(addr).c_str()), addr );
		}
		if (entry.source != 0)
			break;
		batchAddress = batchAddress + sizeof(RelocationEntry);
	}
}



//-----------------------------------RO Loader functions below--------------------------------------------



void ROHandler::applyCRS(u32 exefscode_offset){
	u64 CRSAddress = RomFS::findCRS(m_file_buffer, m_index_list);
	if (CRSAddress > 0)
		loadRelocatableObject(exefscode_offset, CRSAddress, true);
	else
		//warning("The CXI file doesn't contain static.crs! CXI Loader will continue to load ExeFS's code.bin regardless.");
}

void ROHandler::applyCROs(u32 offset_to_load_cros) {
	qvector<u64> croAddresses = RomFS::findCROs(m_file_buffer, m_index_list);
	u64 currentAddressToLoadAt = offset_to_load_cros;
	if (croAddresses.size() <= 0) {
		//warning("The CXI file doesn't contain any CROs! CXI Loader will continue to load ExeFS's code.bin regardless.");
		return;
	}
	for (u32 i = 0; i < croAddresses.size(); i++) {
		currentAddressToLoadAt = loadRelocatableObject(currentAddressToLoadAt, croAddresses[i], false);
	}
	resolveModuleImports();
}

u64 ROHandler::loadRelocatableObject(u64 idb_address_to_load_at, u64 ro_file_address, bool isCrs) {
	CRO_Header header;
	ModuleInfo info;
	qlseek(m_file_buffer, ro_file_address + 0x80);
	qlread(m_file_buffer, &header, sizeof(CRO_Header));

	if (header.Magic != MakeMagic('C', 'R', 'O', '0'))
		warning("CRS/CRO file not found properly!");
	info.name = parseSingleLineString(m_file_buffer, ro_file_address + header.NameOffset);
	info.header = header;
	info.ro_file_address = ro_file_address;

	if (!isCrs) {
		file2base(m_file_buffer, ro_file_address, idb_address_to_load_at, idb_address_to_load_at + header.FileSize, 0);

		set_selector(1, 0);
		add_segm(1, idb_address_to_load_at, idb_address_to_load_at + 0x80, (info.name+".hash").c_str(), CLASS_CONST);
		segment_t* hashSeg = getseg(idb_address_to_load_at);
		hashSeg->set_header_segm(true);
		hashSeg->perm = SEGPERM_READ;

		set_selector(2, 0);
		add_segm(2, idb_address_to_load_at + 0x80, idb_address_to_load_at + 0x80 + sizeof(header), (info.name+".header").c_str(), CLASS_CONST);
		segment_t* headerSeg = getseg(idb_address_to_load_at + 0x80);
		headerSeg->set_header_segm(true);
		headerSeg->perm = SEGPERM_READ;

		set_selector(3, 0);
		add_segm(3, idb_address_to_load_at + header.SegmentTableOffset, idb_address_to_load_at + header.DataOffset, (info.name+".tables").c_str(), CLASS_CONST);
		segment_t* tablesSeg = getseg(idb_address_to_load_at + header.SegmentTableOffset);
		tablesSeg->set_header_segm(true);
		tablesSeg->perm = SEGPERM_READ;
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
		qlseek(m_file_buffer, segmentEntryaddress);
		SegmentTableEntry entry;
		qlread(m_file_buffer, &entry, sizeof(SegmentTableEntry));
		if (entry.SegmentType == 3) { //bss
			entry.SegmentOffset = header.DataOffset + header.DataSize;
			bssSize = entry.SegmentSize;
			if (!isCrs)
				enable_flags(idb_address_to_load_at + entry.SegmentOffset, idb_address_to_load_at + entry.SegmentOffset + entry.SegmentSize, STT_VA);
		}
		u64 segAddress = idb_address_to_load_at + entry.SegmentOffset;
		info.segmentAddresses.push_back(segAddress);
		if (entry.SegmentSize > 0 && !isCrs) {
			add_segm(0, info.segmentAddresses[i], info.segmentAddresses[i] + entry.SegmentSize, (info.name + segmentDic[entry.SegmentType][1]).c_str(), segmentDic[entry.SegmentType][0].c_str());
			segment_t* currentSeg = getseg(info.segmentAddresses[i]);
			switch (entry.SegmentType) {
			case 0: //.text segment permissions
				currentSeg->perm = SEGPERM_READ + SEGPERM_EXEC;
				break;
			case 1: //.rodata segment permissions
				currentSeg->perm = SEGPERM_READ;
				break;
			case 2: //.data segment permissions
				currentSeg->perm = SEGPERM_READ + SEGPERM_WRITE;
				break;
			case 3: //.bss segment permissions
				currentSeg->perm = SEGPERM_READ + SEGPERM_WRITE;
				break;
			}
		}
		segmentEntryaddress += sizeof(SegmentTableEntry);
	}
	
	handleInternalRelocations(info);
	handleExports(info);

	m_module_table.push_back(info);
	return idb_address_to_load_at + header.DataOffset + header.DataSize + bssSize;
}



//-----------------------------------RomFS functions below--------------------------------------------



u32 RomFS::findActualRomFSOffset(linput_t* li, u32 ivfc_offset)
{
	RomFS::RomFS_IVFCHeader ivfcheader;
	u32 ivfc_off = ivfc_offset * kBlockSize;
	qlseek(li, ivfc_off);
	qlread(li, &ivfcheader, sizeof(RomFS::RomFS_IVFCHeader));
	if (ivfcheader.magicid != 0x10000 || MakeMagic('I','V','F','C') != ivfcheader.magic)
		warning("IVFC not found properly!");
	return ivfc_off + align64(align64(sizeof(RomFS::RomFS_IVFCHeader), 16) + ivfcheader.masterhashsize, 1 << ivfcheader.level3.blocksize);
}

/*RomFS::Directory_Metadata RomFS::findRootDir(linput_t* li, u32 actual_romfs_offset)
{
	RomFS::RomFS_Header header;
	qlseek(li, actual_romfs_offset);
	qlread(li, &header, sizeof(RomFS::RomFS_Header));

	u32 metadata_table_address = actual_romfs_offset + header.dir_metadata_table_offset;
	RomFS::Directory_Metadata root_dir;
	qlseek(li, metadata_table_address);
	qlread(li, &root_dir, sizeof(RomFS::Directory_Metadata));

	return root_dir;
}*/

qvector<RomFS::FileInfo> RomFS::indexRootFiles(linput_t* li, u32 actual_romfs_address)
{
	qvector<RomFS::FileInfo> indexlist = qvector<RomFS::FileInfo>();
	RomFS::RomFS_Header header;
	qlseek(li, actual_romfs_address);
	qlread(li, &header, sizeof(RomFS::RomFS_Header));

	RomFS::File_Metadata current_file;
	u32 current_file_offset = header.file_metadata_table_offset;
	bool isIndexingComplete = false;
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

		RomFS::FileInfo info = RomFS::FileInfo{};
		info.metadata = current_file;
		info.fileDataAddress = actual_romfs_address + header.filedataoffset + current_file.actual_file_data_offset;
		info.name = filename;
		indexlist.add(info);
		if (current_file.sibling_file_offset == UNINITIALIZED_FIELD)
			break;
		current_file_offset = header.file_metadata_table_offset + current_file.sibling_file_offset;
	}
	return indexlist;
}

u64 RomFS::findCRS(linput_t* li, qvector<RomFS::FileInfo> indexList)
{
	for each (RomFS::FileInfo file in indexList)
	{
		const wchar16_t* crsformat = L".crs\x18";
		const wchar16_t* extension = file.name.substr(file.name.rfind('.')).c_str();
		if (!file.name.empty() && file.name.rfind('.') != qwstring::npos && wcscmp(extension, crsformat) == 0) {
			return file.fileDataAddress;
		}
	}
	return 0;
}

qvector<u64> RomFS::findCROs(linput_t* li, qvector<RomFS::FileInfo> indexList)
{
	qvector<u64> croAddresses{};
	for each (RomFS::FileInfo file in indexList)
	{
		const wchar16_t* croformat = L".cro";
		const wchar16_t* extension = file.name.substr(file.name.rfind('.')).c_str();
		if (!file.name.empty() && file.name.rfind('.') != qwstring::npos && wcscmp(extension, croformat) == 0) {
			croAddresses.push_back(file.fileDataAddress);
		}
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



void ROHandler::handleInternalRelocations(ModuleInfo info) {
	u32 tableEntryAddress = info.ro_file_address + info.header.InternalPatchTableOffset;
	for (u32 i = 0; i < info.header.InternalPatchNum; i++) {
		RelocationEntry entry;
		qlseek(m_file_buffer, tableEntryAddress);
		qlread(m_file_buffer, &entry, sizeof(RelocationEntry));
		u32 target_address = decodeTag(info.segmentAddresses, entry.target);
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
		tableEntryAddress += sizeof(RelocationEntry);
	}
}

void ROHandler::handleExports(ModuleInfo info) {
	u32 exportNamedEntryAddress = info.ro_file_address + info.header.ExportNamedSymbolTableOffset;
	for (u32 i = 0; i < info.header.ExportNamedSymbolNum; i++) {
		qlseek(m_file_buffer, exportNamedEntryAddress);
		NamedExportTableEntry entry;
		qlread(m_file_buffer, &entry, sizeof(NamedExportTableEntry));
		info.namedExportTable.push_back(entry);
		u32 target_offset = decodeTag(info.segmentAddresses, entry.target);
		qstring name = parseSingleLineString(m_file_buffer, info.ro_file_address + entry.nameOffset);
		if (segtype(target_offset) == SEG_CODE)
			target_offset &= ~1;
		add_entry(target_offset, target_offset, name.c_str(), segtype(target_offset) == SEG_CODE);
		make_name_public(target_offset);
		exportNamedEntryAddress += sizeof(NamedExportTableEntry);
	}

	u32 exportIndexedEntryAddress = info.ro_file_address + info.header.ExportIndexedSymbolTableOffset;
	for (u32 i = 0; i < info.header.ExportIndexedSymbolNum; i++) {
		qlseek(m_file_buffer, exportIndexedEntryAddress);
		SegmentTag segmentOffsetForExport;
		qlread(m_file_buffer, &segmentOffsetForExport, sizeof(u32));
		info.indexedExportTagTable.push_back(segmentOffsetForExport);
		u32 target_offset = decodeTag(info.segmentAddresses, segmentOffsetForExport);
		if (segtype(target_offset) == SEG_CODE)
			target_offset &= ~1;
		qstring indexExp = "indexedExport_";
		add_entry(target_offset, target_offset, indexExp.append(std::to_string(i).c_str()).c_str(), segtype(target_offset) == SEG_CODE);
		make_name_public(target_offset);
		exportIndexedEntryAddress += sizeof(u32);
	}
}

void ROHandler::resolveModuleImports()
{
	if (m_module_table.size() == 0)
		return;

	for (ModuleInfo currentModule : m_module_table) {
		ModuleInfo targetModule;
		bool isTargetModuleFound = false;
		
		u32 importEntryAddress = currentModule.ro_file_address + currentModule.header.ImportNamedSymbolTableOffset;
		for (u32 i = 0; i < currentModule.header.ImportNamedSymbolNum; i++) {
			qlseek(m_file_buffer, importEntryAddress);
			NamedImportTableEntry entry;
			qlread(m_file_buffer, &entry, sizeof(NamedImportTableEntry));
			qstring name = parseSingleLineString(m_file_buffer, currentModule.ro_file_address + entry.nameOffset);
			SegmentTag targetTag;
			if (m_module_table.size() > 0) {
				for (u32 i = 0; i < m_module_table.size(); i++) {
					for (u32 b = 0; b < m_module_table[i].namedExportTable.size(); b++) {
						if (parseSingleLineString(m_file_buffer, m_module_table[i].ro_file_address + m_module_table[i].namedExportTable[b].nameOffset) == name)
							targetTag = m_module_table[i].namedExportTable[b].target;
							targetModule = m_module_table[i];
							qlseek(m_file_buffer, entry.nameOffset);
							qstring nI = "namedImport_";
							patchImportBatch(currentModule.segmentAddresses, currentModule.ro_file_address + entry.batchOffset, nI + name, decodeTag(targetModule.segmentAddresses, targetTag));
					}
				}
			}
			importEntryAddress += sizeof(NamedImportTableEntry);
		}


		u32 importModuleAddress = currentModule.ro_file_address + currentModule.header.ImportModuleTableOffset;
		for (u32 i = 0; i < currentModule.header.ImportModuleNum; i++) {
			qlseek(m_file_buffer, importModuleAddress);
			ModuleImportTableEntry entry;
			qlread(m_file_buffer, &entry, sizeof(ModuleImportTableEntry));

			qstring modulename = parseSingleLineString(m_file_buffer, currentModule.ro_file_address + entry.moduleNameOffset);

			for (u32 i = 0; i < m_module_table.size(); i++) {
				if (modulename == m_module_table[i].name) {
					targetModule = m_module_table[i];
					isTargetModuleFound = true;
				}
			}

			u32 indexedAddress = currentModule.ro_file_address + entry.indexed;
			for (u32 i = 0; i < entry.indexedNum; i++) {
				qlseek(m_file_buffer, indexedAddress);
				IndexedTableEntry indexEntry;
				qlread(m_file_buffer, &indexEntry, sizeof(IndexedTableEntry));
				qstring index = qstring(std::to_string(indexEntry.index).c_str());
				if(isTargetModuleFound == true)
					patchImportBatch(currentModule.segmentAddresses, currentModule.ro_file_address + indexEntry.batchOffset, (modulename+"_index").append(index), decodeTag(targetModule.segmentAddresses, targetModule.indexedExportTagTable[indexEntry.index]));
				indexedAddress += sizeof(IndexedTableEntry);
			}

			u32 anonymousAddress = currentModule.ro_file_address + entry.anonymous;
			for (u32 i = 0; i < entry.anonymousNum; i++) {
				qlseek(m_file_buffer, anonymousAddress);
				AnonymousImportEntry anonEntry;
				qlread(m_file_buffer, &anonEntry, sizeof(AnonymousImportEntry));
				if(isTargetModuleFound == true)
					patchImportBatch(currentModule.segmentAddresses, currentModule.ro_file_address + anonEntry.batchOffset, modulename, decodeTag(targetModule.segmentAddresses, anonEntry.tag));
				anonymousAddress += sizeof(AnonymousImportEntry);
			}
			isTargetModuleFound = false;
			importModuleAddress += sizeof(ModuleImportTableEntry);
		}
	}
}
