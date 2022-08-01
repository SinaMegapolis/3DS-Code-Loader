#pragma once

#include "types.h"
#include "idaldr.h"
#include <vector>

#ifdef _MSC_VER
#define PACK( __Declaration__ ) __pragma( pack(push, 4) ) __Declaration__ __pragma( pack(pop))
#endif

namespace RomFS {

	PACK(typedef struct { //all offsets are from the start of level 3 data. NOT from the start of romfs.bin
		u32 header_length;

		u32 dir_hashtable_offset;
		u32 dir_hashtable_length;
		u32 dir_metadata_table_offset;
		u32 dir_metadata_table_length;

		u32 file_hashtable_offset;
		u32 file_hashtable_length;
		u32 file_metadata_table_offset;
		u32 file_metadata_table_length;

		u32 filedataoffset;
	}) RomFS_Header;

	PACK(typedef struct {
		u32 parent_dir_offset; //offset for itself if the directory is the RomFS root.
		u32 sibling_dir_offset; //offset for next directory in the parent directory (order is unknown, possibly Alphabetical order?)
		u32 child_dir_offset; //offset for the first subdirectory inside the current one (order is unknown, same as above) 
		u32 child_file_offset; //offset for the first child File 
		u32 next_hashtablebucket_dir_offset;
		u32 name_length;
		//char *name[name_length rounded up to a multiple of 4]; encoding is Unicode.  
	}) Directory_Metadata;

	typedef struct {
		u32 parent_dir_index; //index for parent directory inside directory metadata table
		u32 sibling_file_offset; //offset for next file in the parent directory (order is unknown, possibly Alphabetical order?)
		u64 actual_file_data_offset;
		u64 actual_file_data_length;
		u32 next_hashtablebucket_file_offset;
		u32 name_length;
		//char *name[name_length rounded up to a multiple of 4]; encoding is unicode
	} File_Metadata;

	typedef struct {
		RomFS::File_Metadata metadata;
		u64 fileDataAddress;
		qwstring name;
	} FileInfo;

	u32 findActualRomFSOffset(linput_t* li, u32 ivfc_offset);
	//RomFS::Directory_Metadata findRootDir(linput_t* li, u32 actual_romfs_offset);
	qvector<RomFS::FileInfo> indexRootFiles(linput_t* li, u32 actual_romfs_address);
	u64 findCRS(linput_t* li, qvector<RomFS::FileInfo> indexList);
	qvector<u64> findCROs(linput_t* li, qvector<RomFS::FileInfo> indexList);


	PACK(typedef struct {
		u64 logicaloffset;
		u64 hashdatasize;
		u32 blocksize; // == log2(actual block size)
		u8 reserved[4];
	}) IVFC_LevelHeader;

	typedef struct {
		u32 magic; //== "IVFC"
		u32 magicid; //== 0x10000
		u32 masterhashsize;
		IVFC_LevelHeader level1;
		IVFC_LevelHeader level2;
		IVFC_LevelHeader level3;
		u8 reserved[4];
		u8 optionalinfo[4];
	} RomFS_IVFCHeader;

};

class ROHandler {
private:
	typedef struct {
		u32 Magic;
		u32 NameOffset;
		u32 NextCRO;
		u32 PreviousCRO;
		u32 FileSize;
		u32 BssSize;
		u32 FixedSize;
		u32 UnknownZero;
		u32 UnkSegmentTag;
		u32 OnLoadSegmentTag;
		u32 OnExitSegmentTag;
		u32 OnUnresolvedSegmentTag;

		u32 CodeOffset;
		u32 CodeSize;
		u32 DataOffset;
		u32 DataSize;
		u32 ModuleNameOffset;
		u32 ModuleNameSize;
		u32 SegmentTableOffset;
		u32 SegmentNum;

		u32 ExportNamedSymbolTableOffset;
		u32 ExportNamedSymbolNum;
		u32 ExportIndexedSymbolTableOffset;
		u32 ExportIndexedSymbolNum;
		u32 ExportStringsOffset;
		u32 ExportStringsSize;
		u32 ExportTreeTableOffset;
		u32 ExportTreeNum;

		u32 ImportModuleTableOffset;
		u32 ImportModuleNum;
		u32 ExternalPatchTableOffset;
		u32 ExternalPatchNum;
		u32 ImportNamedSymbolTableOffset;
		u32 ImportNamedSymbolNum;
		u32 ImportIndexedSymbolTableOffset;
		u32 ImportIndexedSymbolNum;
		u32 ImportAnonymousSymbolTableOffset;
		u32 ImportAnonymousSymbolNum;
		u32 ImportStringsOffset;
		u32 ImportStringsSize;

		u32 StaticAnonymousSymbolTableOffset;
		u32 StaticAnonymousSymbolNum;
		u32 InternalPatchTableOffset;
		u32 InternalPatchNum;
		u32 StaticPatchTableOffset;
		u32 StaticPatchNum;
	} CRO_Header;

	typedef struct {
		u32 SegmentOffset;
		u32 SegmentSize;
		u32 SegmentType;
	} SegmentTableEntry;

	typedef struct {
		u32 segment_index : 4;
		u32 offset_into_segment : 28;
	} SegmentTag;

	typedef struct {
		SegmentTag target;
		u8 patch_type;
		u8 source;
		u8 unk6;
		u8 unk7;
		u32 addend;
	} RelocationEntry;

	typedef struct {
		u32 nameOffset;
		u32 batchOffset;
	} NamedImportTableEntry;

	typedef struct {
		u32 moduleNameOffset;
		u32 indexed;
		u32 indexedNum;
		u32 anonymous;
		u32 anonymousNum;
	} ModuleImportTableEntry;

	typedef struct {
		u32 nameOffset;
		SegmentTag target;
	} NamedExportTableEntry;

	typedef struct {
		u32 index;
		u32 batchOffset;
	} IndexedTableEntry;

	typedef struct {
		SegmentTag tag;
		u32 batchOffset;
	} AnonymousImportEntry;

	typedef struct {
		qstring name;
		CRO_Header header;
		u64 ro_file_address;
		std::vector<u64> segmentAddresses; //addresses are from the start of the idb
		std::vector<SegmentTag> indexedExportTagTable;
		std::vector<NamedExportTableEntry> namedExportTable;
	} ModuleInfo;

	linput_t* m_file_buffer; //Buffer for the input file
	u32 m_romfs_address; //Address for RomFS inside CXI
	qvector<RomFS::FileInfo> m_index_list;
	std::vector<ModuleInfo> m_module_table;

	u32 decodeTag(std::vector<u64> segmentTable, SegmentTag tag);
	void patchImport(u64 address, qstring importname, u32 patchedvalue = 0);
	void patchImportBatch(std::vector<u64> segmentAddressTable, u32 batchAddress, qstring fullName, u32 addr = -1);

	void handleInternalRelocations(ModuleInfo info);
	void handleExports(ModuleInfo info);
	void resolveModuleImports();

	u64 loadRelocatableObject(u64 idb_address_to_load_at, u64 ro_file_address, bool isCrs);
public:

	ROHandler(linput_t* input, u32 romfs_address) {
		m_file_buffer = input;
		m_romfs_address = romfs_address;
		m_index_list = RomFS::indexRootFiles(input, romfs_address);
	}

	void applyCRS(u32 exefscode_offset);
	void applyCROs(u32 offset_to_load_cros);
};