/* ssl/fnv1a64.c */

/* Open sourcing FIXME: include correct copyright header here. */

/* Fowler-Noll-Vo (FNV) hash: http://isthe.com/chongo/tech/comp/fnv/ */

#include "fnv1a64.h"

/* http://www.isthe.com/chongo/tech/comp/fnv/index.html#FNV-param */
static const FNV1A64 FNV1A64_OFFSET_BASIS = 14695981039346656037ull;
static const FNV1A64 FNV1A64_PRIME = 1099511628211ull;

void fnv1a64_init(FNV1A64* ctx)
	{
	*ctx = FNV1A64_OFFSET_BASIS;
	}

void fnv1a64_update(FNV1A64* ctx, const void* voiddata, unsigned int length)
	{
	const unsigned char *data = voiddata;
	unsigned int i;

	for (i = 0; i < length; i++)
		{
		*ctx ^= data[i];
		*ctx *= FNV1A64_PRIME;
		}
	}

void fnv1a64_final(unsigned char out[8], const FNV1A64 *ctx)
	{
	const FNV1A64 native_endian_result = *ctx;
	unsigned int i;

	for (i = 0; i < 8; i++)
		out[i] = (unsigned char) (native_endian_result >> (8 * (sizeof(FNV1A64) - i - 1)));
	}
