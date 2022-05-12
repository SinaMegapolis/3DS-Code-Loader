#include "idaldr.h"
#pragma once

typedef unsigned char		u8;
typedef unsigned short		u16;
typedef unsigned int		u32;
typedef unsigned long long	u64;

typedef signed char			s8;
typedef signed short		s16;
typedef signed int			s32;
typedef signed long long	s64;

int idaapi accept_file(qstring* fileformatname, qstring* processor, linput_t* li, const char* filename);

void idaapi load_file(linput_t* li, ushort neflags, const char* fileformatname);

loader_t LDSC = { IDP_INTERFACE_VERSION,
                 0, // loader flags
                 accept_file,
                 load_file,
                 NULL,
                 NULL,
                 NULL };