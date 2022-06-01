#include "util.h"

u32 MakeMagic(char a, char b, char c, char d) {
	return a | b << 8 | c << 16 | d << 24;
}

u32 align(u32 offset, u32 alignment)
{
	u32 mask = ~(alignment - 1);

	return (offset + (alignment - 1)) & mask;
}

u64 align64(u64 offset, u32 alignment)
{
	u64 mask = ~(alignment - 1);

	return (offset + (alignment - 1)) & mask;
}

u16 change_to_little_endian(u16 input) {
	return _byteswap_ushort(input);
}

u32 change_to_little_endian(u32 input) {
	return _byteswap_ulong(input);
}

u64 change_to_little_endian(u64 input) {
	return _byteswap_uint64(input);
}

u16 change_to_big_endian(u16 input) {
	return _byteswap_ushort(input);
}

u32 change_to_big_endian(u32 input) {
	return _byteswap_ulong(input);
}

u64 change_to_big_endian(u64 input) {
	return _byteswap_uint64(input);
}