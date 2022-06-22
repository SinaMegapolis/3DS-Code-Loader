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

qstring parseSingleLineString(linput_t* li, u32 nameAddress) {
	qstring result;
	u32 charindex = nameAddress;
	while (true) {
		u8 character;
		qlseek(li, charindex);
		qlread(li, &character, sizeof(u8));
		if (character == '\0')
			break;
		result += character;
		charindex += sizeof(u8);
	}
	return result;
}

qstring decToHex(u32 dec) {
	qstring hex;
	while (dec > 0) {
		u32 rem = dec % 16;
		if (rem > 9) {
			switch (rem) {
			case 10: hex.insert("A"); break;
			case 11: hex.insert("B"); break;
			case 12: hex.insert("C"); break;
			case 13: hex.insert("D"); break;
			case 14: hex.insert("E"); break;
			case 15: hex.insert("F"); break;
			}
		}
		else {
			hex.insert(std::to_string(rem).c_str());
		}
		dec = dec / 16;
	}
	return hex;
}