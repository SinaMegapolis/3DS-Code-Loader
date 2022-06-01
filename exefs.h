#pragma once

#include "types.h"
#include <loader.hpp>
#include "ncch.h"

struct ExeFs_SectionHeader {
    char name[8];
    u32 offset;
    u32 size;
};

struct ExeFs_Header {
    ExeFs_SectionHeader section[8];
    u8 reserved[0x80];
    u8 hashes[8][0x20];
};

struct ExHeader_SystemInfoFlags {
    u8 reserved[5];
    u8 flag;
    u8 remaster_version[2];
};

struct ExHeader_CodeSegmentInfo {
    u32 address;
    u32 num_max_pages;
    u32 code_size;
};

struct ExHeader_CodeSetInfo {
    u8 name[8];
    ExHeader_SystemInfoFlags flags;
    ExHeader_CodeSegmentInfo text;
    u32 stack_size;
    ExHeader_CodeSegmentInfo ro;
    u8 reserved[4];
    ExHeader_CodeSegmentInfo data;
    u32 bss_size;
};

struct ExHeader_DependencyList {
    u8 program_id[0x30][8];
};

struct ExHeader_SystemInfo {
    u64 save_data_size;
    u8 jump_id[8];
    u8 reserved_2[0x30];
};

struct ExHeader_StorageInfo {
    u8 ext_save_data_id[8];
    u8 system_save_data_id[8];
    u8 reserved[8];
    u8 access_info[7];
    u8 other_attributes;
};

struct ExHeader_ARM11_SystemLocalCaps {
    u64 program_id;
    u32 core_version;
    u8 reserved_flags[2];
    u8 flags;
    u8 priority;
    u8 resource_limit_descriptor[0x10][2];
    ExHeader_StorageInfo storage_info;
    u8 service_access_control[0x20][8];
    u8 ex_service_access_control[0x2][8];
    u8 reserved[0xf];
    u8 resource_limit_category;
};

struct ExHeader_ARM11_KernelCaps {
    u32 descriptors[28];
    u8 reserved[0x10];
};

struct ExHeader_ARM9_AccessControl {
    u8 descriptors[15];
    u8 descversion;
};

struct ExHeader_Header {
    ExHeader_CodeSetInfo codeset_info;
    ExHeader_DependencyList dependency_list;
    ExHeader_SystemInfo system_info;
    ExHeader_ARM11_SystemLocalCaps arm11_system_local_caps;
    ExHeader_ARM11_KernelCaps arm11_kernel_caps;
    ExHeader_ARM9_AccessControl arm9_access_control;
    struct {
        u8 signature[0x100];
        u8 ncch_public_key_modulus[0x100];
        ExHeader_ARM11_SystemLocalCaps arm11_system_local_caps;
        ExHeader_ARM11_KernelCaps arm11_kernel_caps;
        ExHeader_ARM9_AccessControl arm9_access_control;
    } access_desc;
};

void load_exefs(linput_t* li, NCCH::NCCH_Header ncch_header, u32 ncch_offset, ExHeader_Header header);
static bool is_encrypted(linput_t* li, NCCH::NCCH_Header ncch_header);