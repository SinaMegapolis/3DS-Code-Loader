#include "exefs.h"
#include "ctr.h"

ExHeader_Header exheader_header;
ExeFs_Header exefs_header;
bool is_encrypted = false;
ctr_aes_context aes{};
ctr_rsa_context rsa{};
u8 key[16];
u8 exheadercounter[16];
u8 exefscounter[16];

const u32 kBlockSize = 0x200;

void load_exefs(linput_t* li)
{

}
