/* ssl/fnv1a64.h */

/* Open sourcing FIXME: include correct copyright header here. */

/* Fowler-Noll-Vo (FNV) hash: http://isthe.com/chongo/tech/comp/fnv/ */

#ifndef HEADER_FNV1A64_H
#define HEADER_FNV1A64_H

#include <stdint.h>

typedef uint64_t FNV1A64;

void fnv1a64_init(FNV1A64* ctx);
void fnv1a64_update(FNV1A64* ctx, const void* data, unsigned int length);
void fnv1a64_final(unsigned char out[8], const FNV1A64 *ctx);

#endif  // HEADER_FNV1A64_H
