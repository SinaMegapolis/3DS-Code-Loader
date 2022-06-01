#include "types.h"
#include "exefs.h"
#include "rohandler.h"
#include "util.h"

const u32 kBlockSize = 0x200;

bool NCCH::check_header(linput_t* li)
{
    u32 size = qlsize(li);
    if (size < 0x4000)
        return false;
    qlseek(li, 0);
    qlread(li, &ncch_header, sizeof(ncch_header));

    if (MakeMagic('N', 'C', 'S', 'D') == ncch_header.magic) {
        ncch_offset = 0x4000;
        qlseek(li, ncch_offset);
        qlread(li, &ncch_header, sizeof(ncch_header));
    }
    if (MakeMagic('N', 'C', 'C', 'H') == ncch_header.magic)
        return true;
    return false;
}

void NCCH::load_file(linput_t* li) {
    ExHeader_Header extheader;
    qlread(li, &extheader, sizeof(ExHeader_Header));

    load_exefs(li, ncch_header, ncch_offset, extheader);
    u32 romfs_offset = RomFS::findActualRomFSOffset(li, ncch_header.romfs_offset);
    //RomFS::Directory_Metadata root_directory = RomFS::findRootDir(li, romfs_offset);

    applyCRS(li, romfs_offset, 0);
}