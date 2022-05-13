#include "types.h"
#include "ncch.h"
#include "exefs.h"

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
    load_exefs(li, ncch_header, ncch_offset);
}