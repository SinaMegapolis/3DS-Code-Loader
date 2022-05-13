#pragma once
#include "types.h"
#include "idaldr.h"

class NCCH {
public:
    struct NCCH_Header {
        u8 signature[0x100];
        u32 magic;
        u32 content_size;
        u8 partition_id[8];
        u16 maker_code;
        u16 version;
        u8 reserved_0[4];
        u8 program_id[8];
        u8 reserved_1[0x10];
        u8 logo_region_hash[0x20];
        u8 product_code[0x10];
        u8 extended_header_hash[0x20];
        u32 extended_header_size;
        u8 reserved_2[4];
        u8 flags[8];
        u32 plain_region_offset;
        u32 plain_region_size;
        u32 logo_region_offset;
        u32 logo_region_size;
        u32 exefs_offset;
        u32 exefs_size;
        u32 exefs_hash_region_size;
        u8 reserved_3[4];
        u32 romfs_offset;
        u32 romfs_size;
        u32 romfs_hash_region_size;
        u8 reserved_4[4];
        u8 exefs_super_block_hash[0x20];
        u8 romfs_super_block_hash[0x20];
    };
private:
    NCCH_Header ncch_header;
    u32 ncch_offset;
public:
    NCCH() : ncch_offset(0) {};
    bool check_header(linput_t* li);
    void load_file(linput_t* li);
};