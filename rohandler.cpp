#include <vector>
#include "rohandler.h"
#include "util.h"

#define UNINITIALIZED_FIELD 0xFFFFFFFF

const u32 kBlockSize = 0x200;

static qstring decToHex(u32 dec) {
	qstring hex;
	while (dec > 0) {
		u32 rem = dec % 16;
		if (rem > 9) {
			switch (rem) {
			case 10: hex.append("A"); break;
			case 11: hex.append("B"); break;
			case 12: hex.append("C"); break;
			case 13: hex.append("D"); break;
			case 14: hex.append("E"); break;
			case 15: hex.append("F"); break;
			}
		}
		else {
			hex.append(std::to_string(rem).c_str());
		}
		dec = dec / 16;
	}
	return hex;
}

static u32 DecodeTag(std::vector<u64> segmentTable, u32 tag) {
	u32 target_segment = tag & 0xF;
	u32 target_rel_offset = tag >> 4;
	return segmentTable[target_segment] + target_rel_offset;
}

static void do_import_name(u64 address, qstring name) {
	qstring import_name = "_import_";
	force_name(address, import_name.append(name).c_str());

    //guess wrapper
	if (get_dword(address - 4) == 0xE51FF004)
		force_name(address - 4, name.c_str());
}

static void do_import_batch(linput_t* li, std::vector<u64> segmentAddressTable, u32 batchAddress, qstring fullName, u32 addr = -1) {
	while (true) {
		qlseek(li, batchAddress);
		RelocationEntry entry;
		qlread(li, &entry, sizeof(RelocationEntry));
		u32 target_offset = DecodeTag(segmentAddressTable, entry.target);
		if (addr == -1)
			do_import_name(target_offset, fullName);
		else {
			u32 shift_final = entry.addend;
			if (entry.patch_type == 3)
				shift_final -= target_offset;
			do_import_name(target_offset, fullName.append("_0x").append(decToHex(addr + shift_final).c_str()));
		}
		if (entry.source != 0)
			break;
		batchAddress = batchAddress + sizeof(RelocationEntry);
	}
}

void applyCRS(linput_t* li, u32 actual_romfs_offset, u32 exefscode_offset){
	loadRelocatableObject(li, exefscode_offset, RomFS::findCRS(li, actual_romfs_offset), true);
}

void loadRelocatableObject(linput_t* li, u64 idb_address_to_load_at, u64 ro_file_address, bool isCrs) {
	CRO_Header header;
	qlseek(li, ro_file_address + 0x80);
	qlread(li, &header, sizeof(CRO_Header));
	if (header.Magic != MakeMagic('C', 'R', 'O', '0'))
		warning("CRO file not found properly!");
	if (!isCrs) {
		file2base(li, ro_file_address, idb_address_to_load_at, idb_address_to_load_at + header.FileSize, 0);

		//set_selector(1, 0);
		add_segm(0, idb_address_to_load_at, idb_address_to_load_at + 0x80, "CRO.header", "RODATA");

		//set_selector(2, 0);
		add_segm(0, idb_address_to_load_at + header.SegmentTableOffset, idb_address_to_load_at + header.DataOffset, "CRO.tables", "RODATA");
	}
	qstring segmentDic[4][2]{
		{"CODE", ".text"},
		{"CONST DATA", ".rodata"},
		{"DATA", ".data"},
		{"BSS", ".bss"}
	};
	std::vector<u64> segmentAddress;

	u64 segmentEntryaddress = ro_file_address + header.SegmentTableOffset;
	for (u32 i = 0; i < header.SegmentNum; i++) {
		qlseek(li, segmentEntryaddress);
		SegmentTableEntry entry;
		qlread(li, &entry, sizeof(SegmentTableEntry));
		if (entry.SegmentType == 3) {
			entry.SegmentOffset = 0x08000000;
			if (!isCrs)
				enable_flags(idb_address_to_load_at + entry.SegmentOffset, idb_address_to_load_at + entry.SegmentOffset + entry.SegmentSize, STT_VA);
		}
		u64 segAddress = idb_address_to_load_at + entry.SegmentOffset;
		segmentAddress.push_back(segAddress);
		if (entry.SegmentSize > 0 && !isCrs)
			add_segm(0, segmentAddress[i], segmentAddress[i] + entry.SegmentSize, segmentDic[entry.SegmentType][1].c_str(), segmentDic[entry.SegmentType][0].c_str());
		segmentEntryaddress += sizeof(SegmentTableEntry);
	}

	//Do internal relocations
	u32 tableEntryAddress = ro_file_address + header.InternalPatchTableOffset;
	for (u32 i = 0; i < header.InternalPatchNum; i++) {
		RelocationEntry entry;
		qlseek(li, tableEntryAddress);
		qlread(li, &entry, sizeof(RelocationEntry));
		u32 target_offset = DecodeTag(segmentAddress, entry.target);
		u32 source_offset = segmentAddress[entry.source] + entry.addend;
		u32 value;
		if (entry.patch_type == 2)
			value = source_offset;
		else if (entry.patch_type == 3) {
			u32 rel = source_offset - target_offset;
			if (rel < 0)
				rel += 0x100000000;
			value = rel;
		}
		patch_dword(idb_address_to_load_at + target_offset, value);
		fixup_data_t f = fixup_data_t();
		f.set_type(FIXUP_OFF32);
		f.off = value;
		set_fixup(idb_address_to_load_at + target_offset, f);
		tableEntryAddress += sizeof(RelocationEntry);
	}

	//Handle imports
	u32 importEntryAddress = ro_file_address + header.ImportNamedSymbolTableOffset;
	for (u32 i = 0; i < header.ImportNamedSymbolNum; i++) {
		qlseek(li, importEntryAddress);
		NamedImportTableEntry entry;
		qlread(li, &entry, sizeof(NamedImportTableEntry));
		
		qstring name;
		qlseek(li, entry.nameOffset);
		u32 charindex = entry.nameOffset;
		while (true)
		{
			qlseek(li, charindex);
			u8 character;
			qlread(li, &character, sizeof(u8));
			if (character == '\0')
				break;
			name += character;
			charindex = charindex + sizeof(u8);
		}
		do_import_batch(li, segmentAddress, ro_file_address + entry.batchOffset, name);
		importEntryAddress += sizeof(NamedImportTableEntry);
	}

	u32 crsReloc;
	if (isCrs)
		crsReloc = 0x180;

	u32 importModuleAddress = ro_file_address + header.ImportModuleTableOffset;
	for (u32 i = 0; i < header.ImportModuleNum; i++) {
		qlseek(li, importModuleAddress);
		ModuleImportTableEntry entry;
		qlread(li, &entry, sizeof(ModuleImportTableEntry));

		qstring modulename;
		u32 charindex = ro_file_address + entry.moduleNameOffset;
		while (true)
		{
			qlseek(li, charindex);
			u8 character;
			qlread(li, &character, sizeof(u8));
			if (character == '\0')
				break;
			modulename += character;
			charindex += sizeof(u8);
		}

		u32 staticReloc = 0;
		if (modulename == "|static|")
			staticReloc = -0x180;

		u32 indexedAddress = ro_file_address + entry.indexed;
		for (u32 i = 0; i < entry.indexedNum; i++) {
			qlseek(li, indexedAddress);
			IndexedTableEntry indexEntry;
			qlread(li, &indexEntry, sizeof(IndexedTableEntry));
			qstring index = qstring(std::to_string(indexEntry.index).c_str());
			do_import_batch(li, segmentAddress, ro_file_address + indexEntry.batchOffset, modulename.append(index));
			indexedAddress += sizeof(IndexedTableEntry);
		}

		u32 anonymousAddress = ro_file_address + entry.anonymous;
		for (u32 i = 0; i < entry.anonymousNum; i++) {
			qlseek(li, anonymousAddress);
			AnonymousImportEntry anonEntry;
			qlread(li, &anonEntry, sizeof(AnonymousImportEntry));
			do_import_batch(li, segmentAddress, ro_file_address + anonEntry.batchOffset, modulename, DecodeTag(segmentAddress, anonEntry.tag) + crsReloc + staticReloc);
			anonymousAddress += sizeof(AnonymousImportEntry);
		}
		importModuleAddress += sizeof(ModuleImportTableEntry);
	}

	//Handle Exports
	u32 exportNamedEntryAddress = ro_file_address + header.ExportNamedSymbolTableOffset;
	for (u32 i = 0; i < header.ExportNamedSymbolNum; i++) {
		qlseek(li, exportNamedEntryAddress);
		NamedExportTableEntry entry;
		qlread(li, &entry, sizeof(NamedExportTableEntry));
		u32 target_offset = DecodeTag(segmentAddress, entry.target);
		qstring name;
		u32 charindex = ro_file_address + entry.nameOffset;
		while (true)
		{
			qlseek(li, charindex);
			u8 character;
			qlread(li, &character, sizeof(u8));
			if (character == '\0')
				break;
			name += character;
			charindex += sizeof(u8);
		}
		if (segtype(idb_address_to_load_at + target_offset) == SEG_CODE)
			target_offset &= ~1;
		add_entry(target_offset, target_offset, name.c_str(), segtype(target_offset) == SEG_CODE);
		make_name_public(target_offset);
		exportNamedEntryAddress += sizeof(NamedExportTableEntry);
	}

	u32 exportIndexedEntryAddress = ro_file_address + header.ExportIndexedSymbolTableOffset;
	for (u32 i = 0; i < header.ExportIndexedSymbolNum; i++) {
		qlseek(li, exportIndexedEntryAddress);
		u32 segmentOffsetForExport;
		qlread(li, &segmentOffsetForExport, sizeof(u32));
		u32 target_offset = DecodeTag(segmentAddress, segmentOffsetForExport);
		if (segtype(idb_address_to_load_at + target_offset) == SEG_CODE)
			target_offset &= ~1;
		qstring indexExp = "indexedExport_";
		add_entry(target_offset, target_offset, indexExp.append(std::to_string(i).c_str()).c_str(), segtype(target_offset) == SEG_CODE);
		make_name_public(target_offset);
		exportIndexedEntryAddress += sizeof(u32);
	}
}

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

u64 RomFS::findCRS(linput_t* li, u32 actual_romfs_offset)
{
	RomFS::RomFS_Header header;
	qlseek(li, actual_romfs_offset);
	qlread(li, &header, sizeof(RomFS::RomFS_Header));

	RomFS::File_Metadata current_file;
	u32 current_file_offset = header.file_metadata_table_offset;
	while (true && current_file_offset != UNINITIALIZED_FIELD) {
		u32 current_file_address = actual_romfs_offset + current_file_offset;
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
			qlseek(li, actual_romfs_offset + current_file.actual_file_data_offset); 
			info("Static.crs file located at offset %d!", actual_romfs_offset + header.filedataoffset + current_file.actual_file_data_offset);
			return actual_romfs_offset + header.filedataoffset + current_file.actual_file_data_offset;
		}
		current_file_offset = header.file_metadata_table_offset + current_file.sibling_file_offset;
	}
 
}

void RomFS::findCROs(linput_t* li, u32 first_file_metadata_offset, u32 actual_romfs_offset)
{
}

/*static u32 hash(u8* filename, u32 name_length, u32 parentdiroffset) { //hash function directly taken from 3DBrew and translated to C++
	u32 hash = parentdiroffset ^ 123456789;
	for (u32 i = 0; i < name_length; i += 2) {
		hash = (hash >> 5) | (hash << 27);
		hash ^= (u16)((filename[i]) | (filename[i + 1] << 8));
	}
	return hash;
}*/