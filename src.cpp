/*
 *
 */

#include "idaldr.h"
#include "types.h"
#include "exefs.h"

class NCCH {
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
public:

    bool NCCH::check_header(linput_t* li)
    {
        u32 size = qlsize(li);
        if (size < 0x4000)
            return false;
        qlseek(li, 0);
        qlread(li, &ncch_header, sizeof(ncch_header));
        if (MakeMagic('N', 'C', 'C', 'H') == ncch_header.magic)
            return true;
        return false;
    }

    void load_file(linput_t* li) {

    }
};

//--------------------------------------------------------------------------
//
//      check input file format. if recognized, then return 1
//      and fill 'fileformatname'.
//      otherwise return 0
//
static int idaapi accept_file(
        qstring *fileformatname,
        qstring *processor,
        linput_t *li,
        const char *)
{
    NCCH ncch;
    if (ncch.check_header(li)) {
        *fileformatname = "Nintendo 3DS game dump (NCCH/CXI)";
        *processor = "arm";
        return 1;
    }
    return 0;
}

//--------------------------------------------------------------------------
//
//      load file into the database.
//
static void idaapi load_file(linput_t *li, ushort neflag, const char * /*fileformatname*/)
{

}

//----------------------------------------------------------------------
//
//      LOADER DESCRIPTION BLOCK
//
//----------------------------------------------------------------------
loader_t LDSC =
{
  IDP_INTERFACE_VERSION,
  0,                            // loader flags
//
//      check input file format. if recognized, then return 1
//      and fill 'fileformatname'.
//      otherwise return 0
//
  accept_file,
//
//      load file into the database.
//
  load_file,
//
//      create output file from the database.
//      this function may be absent.
//
  NULL,
//      take care of a moved segment (fix up relocations, for example)
  NULL,
  NULL,
};
