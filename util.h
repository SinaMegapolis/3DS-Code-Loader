#include <intrin.h>
#include "types.h"
#include "idaldr.h"
#pragma once

u32 MakeMagic(char a, char b, char c, char d);

u32 align(u32 offset, u32 alignment);

u64 align64(u64 offset, u32 alignment);

u16 change_to_little_endian(u16 input);

u32 change_to_little_endian(u32 input);

u64 change_to_little_endian(u64 input);

u16 change_to_big_endian(u16 input);

u32 change_to_big_endian(u32 input);

u64 change_to_big_endian(u64 input);

qstring parseSingleLineString(linput_t* li, u32 nameAddress);

qstring decToHex(u32 dec);
