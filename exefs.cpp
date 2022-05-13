#include <vector>
#include "exefs.h"

ExHeader_Header exheader_header;
ExeFs_Header exefs_header;
u8 key[16];
u8 exheadercounter[16];
u8 exefscounter[16];

const u32 kBlockSize = 0x200;

static u32 LZSS_GetDecompressedSize(const u8* buffer, u32 size) {
    u32 offset_size = *(u32*)(buffer + size - 4);
    return offset_size + size;
}

static bool LZSS_Decompress(const u8* compressed, u32 compressed_size, u8* decompressed,
    u32 decompressed_size) {
    const u8* footer = compressed + compressed_size - 8;
    u32 buffer_top_and_bottom = *reinterpret_cast<const u32*>(footer);
    u32 out = decompressed_size;
    u32 index = compressed_size - ((buffer_top_and_bottom >> 24) & 0xFF);
    u32 stop_index = compressed_size - (buffer_top_and_bottom & 0xFFFFFF);

    memset(decompressed, 0, decompressed_size);
    memcpy(decompressed, compressed, compressed_size);

    while (index > stop_index) {
        u8 control = compressed[--index];

        for (unsigned i = 0; i < 8; i++) {
            if (index <= stop_index)
                break;
            if (index <= 0)
                break;
            if (out <= 0)
                break;

            if (control & 0x80) {
                // Check if compression is out of bounds
                if (index < 2)
                    return false;
                index -= 2;

                u32 segment_offset = compressed[index] | (compressed[index + 1] << 8);
                u32 segment_size = ((segment_offset >> 12) & 15) + 3;
                segment_offset &= 0x0FFF;
                segment_offset += 2;

                // Check if compression is out of bounds
                if (out < segment_size)
                    return false;

                for (unsigned j = 0; j < segment_size; j++) {
                    // Check if compression is out of bounds
                    if (out + segment_offset >= decompressed_size)
                        return false;

                    u8 data = decompressed[out + segment_offset];
                    decompressed[--out] = data;
                }
            }
            else {
                // Check if compression is out of bounds
                if (out < 1)
                    return false;
                decompressed[--out] = compressed[--index];
            }
            control <<= 1;
        }
    }
    return true;
}

void load_exefs(linput_t* li, NCCH::NCCH_Header ncch_header, u32 ncch_offset)
{
    qlread(li, &exheader_header, sizeof(exheader_header));

    bool is_compressed = (exheader_header.codeset_info.flags.flag & 1) == 1;

    u32 exefs_offset = ncch_header.exefs_offset * kBlockSize;

    qlseek(li, exefs_offset + ncch_offset);

    qlread(li, &exefs_header, sizeof(exefs_header));

    std::vector<u8> code;

    for (u32 i = 0; i < 8; ++i) {
        const ExeFs_SectionHeader& section = exefs_header.section[i];
        if (0 == qstrcmp(section.name, ".code")) {
            u32 offset = section.offset + exefs_offset + sizeof(ExeFs_Header) + ncch_offset;
            qlseek(li, offset);
            if (is_compressed) {
                std::vector<u8> temp;
                temp.resize(section.size);
                qlread(li, &temp[0], section.size);
                u32 decompressed_size = LZSS_GetDecompressedSize(&temp[0], section.size);
                code.resize(decompressed_size);
                bool rc = LZSS_Decompress(&temp[0], section.size, &code[0], decompressed_size);
                if (!rc)
                    qexit(1);
            }
            else {
                code.resize(section.size);
                qlread(li, &code[0], section.size);
            }
            break;
        }
    }

    bool aligned =
        code.size() >= (exheader_header.codeset_info.text.code_size + exheader_header.codeset_info.ro.code_size +
            exheader_header.codeset_info.data.code_size);
    u32 offset = 0;

    set_selector(1, 0);
    if (!add_segm(1, exheader_header.codeset_info.text.address,
        exheader_header.codeset_info.text.address + exheader_header.codeset_info.text.num_max_pages * 0x1000,
        NAME_CODE, CLASS_CODE))
        qexit(1);
    set_segm_addressing(getseg(exheader_header.codeset_info.text.address), 1); // enable 32bit addressing
    mem2base(&code[offset], exheader_header.codeset_info.text.address,
        exheader_header.codeset_info.text.address + exheader_header.codeset_info.text.code_size, -1);

    offset =
        aligned ? exheader_header.codeset_info.text.num_max_pages * 0x1000 : exheader_header.codeset_info.text.code_size;

    set_selector(2, 0);
    if (!add_segm(2, exheader_header.codeset_info.ro.address,
        exheader_header.codeset_info.ro.address + exheader_header.codeset_info.ro.num_max_pages * 0x1000, ".ro",
        CLASS_CONST))
        qexit(1);
    mem2base(&code[offset], exheader_header.codeset_info.ro.address,
        exheader_header.codeset_info.ro.address + exheader_header.codeset_info.ro.code_size, -1);

    set_selector(3, 0);
    if (!add_segm(3, exheader_header.codeset_info.data.address,
        exheader_header.codeset_info.data.address + exheader_header.codeset_info.data.num_max_pages * 0x1000,
        NAME_DATA, CLASS_DATA))
        qexit(1);

    offset =
        aligned ? (exheader_header.codeset_info.text.num_max_pages + exheader_header.codeset_info.ro.num_max_pages) * 0x1000
        : exheader_header.codeset_info.text.code_size + exheader_header.codeset_info.ro.code_size;

    mem2base(&code[offset], exheader_header.codeset_info.data.address,
        exheader_header.codeset_info.data.address + exheader_header.codeset_info.data.code_size, -1);

    set_selector(4, 0);
    add_segm(4, (exheader_header.codeset_info.data.address + exheader_header.codeset_info.data.num_max_pages * 0x1000),
        (exheader_header.codeset_info.data.address + exheader_header.codeset_info.data.num_max_pages * 0x1000) +
        exheader_header.codeset_info.bss_size,
        NAME_BSS, CLASS_BSS);
}

static bool is_encrypted(linput_t* li, NCCH::NCCH_Header ncch_header)
{
    if (memcmp(&exheader_header.arm11_system_local_caps.program_id, &ncch_header.program_id, 8)) {
        // Fixed Crypto Key
        if (ncch_header.flags[7] & 0x1) {
            memset(&exheadercounter, 0, sizeof(exheadercounter));
            memset(&exefscounter, 0, sizeof(exefscounter));
            if (ncch_header.version == 2 || ncch_header.version == 0) {
                for (u8 i = 0; i < 8; i++) {
                    exefscounter[i] = exheadercounter[i] = ncch_header.partition_id[7 - i];
                }
                exheadercounter[8] = 1;
                exefscounter[8] = 2;
                return true;
            }
        }
    }
	return false;
}
