#pragma once

#include "types.h"
#include <loader.hpp>

typedef struct { //all offsets are from the start of level 3 data. NOT from the start of romfs.bin
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
} RomFS_Header;

typedef struct {
	u32 parent_dir_offset; //offset for itself if the directory is the RomFS root.
	u32 sibling_dir_offset; //offset for next directory in the parent directory (order is unknown, possibly Alphabetical order?)
	u32 child_dir_offset; //offset for the first subdirectory inside the current one (order is unknown, same as above) 
	u32 child_file_index; //file metadata table index for the first child File 
	u32 next_hashtablebucket_dir_offset;
	u32 name_length;
	qstring name; //char *name[name_length rounded up to a multiple of 4]; encoding is Unicode.  
} Directory_Metadata;

typedef struct {
	u32 parent_dir_index; //index for parent directory inside directory metadata table
	u32 sibling_file_offset; //offset for next file in the parent directory (order is unknown, possibly Alphabetical order?)
	u64 actual_file_data_offset;
	u64 actual_file_data_length;
	u32 next_hashtablebucket_file_offset;
	u32 name_length;
    //char *name[name_length rounded up to a multiple of 4]; encoding is unicode
} File_Metadata;

void findFile(linput_t* li, u32 romfs_offset, u8* filename, u32 name_length, u32 parentdiroffset);
void findCRS(linput_t* li, u32 romfs_offset);
void findCROs(linput_t* li, u32 romfs_offset);
static u32 hash(u8* filename, u32 name_length, u32 parentdiroffset);



namespace RomFS_IVFC {
	typedef struct {
		u64 logicaloffset;
		u64 hashdatasize;
		u8 blocksize[4]; // == log2(actual block size)
		u8 reserved[4];
	} IVFC_LevelHeader;

	typedef struct {
		u8 magic[4]; //== "IVFC"
		u8 magicid[4]; //== 0x10000
		u8 masterhashsize[4];
		IVFC_LevelHeader level1;
		IVFC_LevelHeader level2;
		IVFC_LevelHeader level3;
		u8 reserved[4];
		u8 optionalinfo[4];
	} RomFS_IVFCHeader;
	//TODO: finish IVFC integrity check
	/*typedef struct
	{
		u64 dataoffset;
		u64 datasize;
		u64 hashoffset; //offset from RomFS.bin base
		u32 hashblocksize;
		int hashcheck;
	} IVFC_LevelInfo;

	bool checkRomFSIntegrity(linput_t* li, RomFS_IVFCHeader header);
	static bool verifyRomFSHash(linput_t* li, RomFS_IVFCHeader header, IVFC_LevelInfo* infoForLevels);
	static void ivfc_read(linput_t* li, u32 offset, u32 size, u8* buffer);*/
}
