/**
 *
 * Copyright 2005, 2006 Nick Galbreath -- nickg [at] modp [dot] com
 * Copyright 2017 Matthieu Darbois
 * All rights reserved.
 *
 * http://modp.com/release/base64
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * - Redistributions of source code must retain the above copyright notice,
 *  this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

/****************************/

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

static uint8_t b64chars[64] = {
	'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M',
	'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
	'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm',
	'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
	'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '+', '/'
};

static uint8_t padchar = '=';

static void printStart(void)
{
	printf("#include <stdint.h>\n");
	printf("#define CHAR62 '%c'\n", b64chars[62]);
	printf("#define CHAR63 '%c'\n", b64chars[63]);
	printf("#define CHARPAD '%c'\n", padchar);
}

static void clearDecodeTable(uint32_t* ary)
{
	int i = 0;
	for (i = 0; i < 256; ++i) {
		ary[i] = 0xFFFFFFFF;
	}
}

/* dump uint32_t as hex digits */
void uint32_array_to_c_hex(const uint32_t* ary, size_t sz, const char* name)
{
	size_t i = 0;

	printf("const uint32_t %s[%d] = {\n", name, (int)sz);
	for (;;) {
		printf("0x%08" PRIx32, ary[i]);
		++i;
		if (i == sz)
			break;
		if (i % 6 == 0) {
			printf(",\n");
		} else {
			printf(", ");
		}
	}
	printf("\n};\n");
}

int main(int argc, char** argv)
{
	uint32_t x;
	uint32_t i = 0;
	uint32_t ary[256];

	/*  over-ride standard alphabet */
	if (argc == 2) {
		uint8_t* replacements = (uint8_t*)argv[1];
		if (strlen((char*)replacements) != 3) {
			fprintf(stderr, "input must be a string of 3 characters '-', '.' or '_'\n");
			exit(1);
		}
		fprintf(stderr, "fusing '%s' as replacements in base64 encoding\n", replacements);
		b64chars[62] = replacements[0];
		b64chars[63] = replacements[1];
		padchar = replacements[2];
	}

	printStart();

	printf("\n\n#if BASE64_LITTLE_ENDIAN\n");

	printf("\n\n/* SPECIAL DECODE TABLES FOR LITTLE ENDIAN (INTEL) CPUS */\n\n");

	clearDecodeTable(ary);
	for (i = 0; i < 64; ++i) {
		x = b64chars[i];
		ary[x] = i << 2;
	}
	uint32_array_to_c_hex(ary, sizeof(ary) / sizeof(uint32_t), "base64_table_dec_32bit_d0");
	printf("\n\n");

	clearDecodeTable(ary);
	for (i = 0; i < 64; ++i) {
		x = b64chars[i];
		ary[x] = ((i & 0x30) >> 4) | ((i & 0x0F) << 12);
	}
	uint32_array_to_c_hex(ary, sizeof(ary) / sizeof(uint32_t), "base64_table_dec_32bit_d1");
	printf("\n\n");

	clearDecodeTable(ary);
	for (i = 0; i < 64; ++i) {
		x = b64chars[i];
		ary[x] = ((i & 0x03) << 22) | ((i & 0x3c) << 6);
	}
	uint32_array_to_c_hex(ary, sizeof(ary) / sizeof(uint32_t), "base64_table_dec_32bit_d2");
	printf("\n\n");

	clearDecodeTable(ary);
	for (i = 0; i < 64; ++i) {
		x = b64chars[i];
		ary[x] = i << 16;
	}
	uint32_array_to_c_hex(ary, sizeof(ary) / sizeof(uint32_t), "base64_table_dec_32bit_d3");
	printf("\n\n");

	printf("#else\n");

	printf("\n\n/* SPECIAL DECODE TABLES FOR BIG ENDIAN (IBM/MOTOROLA/SUN) CPUS */\n\n");

	clearDecodeTable(ary);
	for (i = 0; i < 64; ++i) {
		x = b64chars[i];
		ary[x] = i << 26;
	}
	uint32_array_to_c_hex(ary, sizeof(ary) / sizeof(uint32_t), "base64_table_dec_32bit_d0");
	printf("\n\n");

	clearDecodeTable(ary);
	for (i = 0; i < 64; ++i) {
		x = b64chars[i];
		ary[x] = i << 20;
	}
	uint32_array_to_c_hex(ary, sizeof(ary) / sizeof(uint32_t), "base64_table_dec_32bit_d1");
	printf("\n\n");

	clearDecodeTable(ary);
	for (i = 0; i < 64; ++i) {
		x = b64chars[i];
		ary[x] = i << 14;
	}
	uint32_array_to_c_hex(ary, sizeof(ary) / sizeof(uint32_t), "base64_table_dec_32bit_d2");
	printf("\n\n");

	clearDecodeTable(ary);
	for (i = 0; i < 64; ++i) {
		x = b64chars[i];
		ary[x] = i << 8;
	}
	uint32_array_to_c_hex(ary, sizeof(ary) / sizeof(uint32_t), "base64_table_dec_32bit_d3");
	printf("\n\n");

	printf("#endif\n");

	return 0;
}
