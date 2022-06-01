#include "romfs.h"
#include "idaldr.h"

#define UNUSED 0xFFFFFFFF

void findFile(linput_t* li, u32 romfs_offset, u8* filename, u32 name_length, u32 parentdiroffset)
{
	RomFS_IVFC::RomFS_IVFCHeader ivfcheader;
	qlread(li, &ivfcheader, sizeof(RomFS_IVFC::RomFS_IVFCHeader));
	qlseek(li, romfs_offset + ivfcheader.level3.logicaloffset);
	RomFS_Header romfs_header;
	qlread(li, &romfs_header, sizeof(RomFS_Header));

	u32 romfs_data_offset = romfs_offset + ivfcheader.level3.logicaloffset;
	u32* file_hashtable = {new u32[romfs_header.file_hashtable_length]};
	qlseek(li, romfs_data_offset + romfs_header.file_hashtable_offset);
	qlread(li, file_hashtable, sizeof(file_hashtable));

	u32 hashkey = hash(filename, name_length, parentdiroffset);
	if (file_hashtable[hashkey] == UNUSED)
		return;

	File_Metadata metadata;
	qlseek(li, romfs_data_offset + file_hashtable[hashkey]);
	while (true) {
		qlread(li, &metadata, sizeof(File_Metadata));
		u8* name = { new u8[metadata.name_length] };

		if (qstrcmp(name, filename) == 1)
			return;
		if (metadata.next_hashtablebucket_file_offset == UNUSED)
			return;
		qlseek(li, romfs_data_offset + metadata.next_hashtablebucket_file_offset);
	}
}

static u32 hash(u8* filename, u32 name_length, u32 parentdiroffset) { //hash function directly taken from 3DBrew and translated to C++
	u32 hash = parentdiroffset ^ 123456789;
	for (u32 i = 0; i < name_length; i += 2) {
		hash = (hash >> 5) | (hash << 27);
		hash ^= (u16)((filename[i]) | (filename[i + 1] << 8));
	}
	return hash;
}

//TODO: finish integrity check for IVFC
/*bool RomFS_IVFC::checkRomFSIntegrity(RomFS_IVFCHeader header)
{
	IVFC_LevelInfo levelInfo[3];

	levelInfo[0].hashoffset = 0x60;
	levelInfo[0].hashblocksize = 1 << getle32(header.level1.blocksize);
	levelInfo[1].hashblocksize = 1 << getle32(header.level2.blocksize);
	levelInfo[2].hashblocksize = 1 << getle32(header.level3.blocksize);

	u64 romfsdataoffset = align64(levelInfo[0].hashoffset + getle32(header.masterhashsize), levelInfo[2].hashblocksize); //offset of actual files/directories blob
	u64 romfsdatasize = getle64(header.level3.hashdatasize); //size of all directories and files

	levelInfo[2].dataoffset = romfsdataoffset;
	levelInfo[2].datasize = align64(romfsdatasize, levelInfo[2].hashblocksize);

	levelInfo[1].hashoffset = align64(romfsdataoffset + romfsdatasize, levelInfo[2].hashblocksize);
	levelInfo[2].hashoffset = levelInfo[1].hashoffset + getle64(header.level2.logicaloffset) - getle64(header.level1.logicaloffset);

	levelInfo[1].dataoffset = levelInfo[2].hashoffset;
	levelInfo[1].datasize = align64(getle64(header.level2.hashdatasize), levelInfo[1].hashblocksize);

	levelInfo[0].dataoffset = levelInfo[1].hashoffset;
	levelInfo[0].datasize = align64(getle64(header.level1.hashdatasize), levelInfo[0].hashblocksize);

	u32 i, j;
	u32 blockcount;

	return verifyRomFSHash(header, levelInfo);
}

static bool RomFS_IVFC::verifyRomFSHash(RomFS_IVFCHeader header, IVFC_LevelInfo* infoForLevels)
{
	u32 i, j;
	u32 blockcount;


	for (i = 0; i < 3; i++)
	{
		IVFC_LevelInfo level = infoForLevels[i];

		blockcount = level.datasize / level.hashblocksize;
		if (blockcount * level.hashblocksize != level.datasize)
		{
			fprintf(stderr, "Error, IVFC block size mismatch\n");
			return false;
		}

		for (j = 0; j < blockcount; j++)
		{
			u8 calchash[32];
			u8 testhash[32];


			ivfc_hash(level.dataoffset + level.hashblocksize * j, level.hashblocksize, calchash);
			ivfc_read(level.hashoffset + 0x20 * j, 0x20, testhash);

			if (memcmp(calchash, testhash, 0x20) != 0)
				return false;
		}
	}
	return true;
}

void ivfc_read(linput_t* li, u32 offset, u32 size, u8* buffer)
{
	if ((offset > ctx->size) || (offset + size > ctx->size))
	{
		fprintf(stderr, "Error, IVFC offset out of range (offset=0x%08x, size=0x%08x)\n", offset, size);
		return;
	}

	fseek(ctx->file, ctx->offset + offset, SEEK_SET);
	if (size != fread(buffer, 1, size, ctx->file))
	{
		fprintf(stderr, "Error, IVFC could not read file\n");
		return;
	}
}*/
