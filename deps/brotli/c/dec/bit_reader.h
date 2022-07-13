/* Copyright 2013 Google Inc. All Rights Reserved.

   Distributed under MIT license.
   See file LICENSE for detail or copy at https://opensource.org/licenses/MIT
*/

/* Bit reading helpers */

#ifndef BROTLI_DEC_BIT_READER_H_
#define BROTLI_DEC_BIT_READER_H_

#include <string.h>  /* memcpy */

#include "../common/constants.h"
#include "../common/platform.h"
#include <brotli/types.h>

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

#define BROTLI_SHORT_FILL_BIT_WINDOW_READ (sizeof(brotli_reg_t) >> 1)

BROTLI_INTERNAL extern const uint32_t kBrotliBitMask[33];

static BROTLI_INLINE uint32_t BitMask(uint32_t n) {
  if (BROTLI_IS_CONSTANT(n) || BROTLI_HAS_UBFX) {
    /* Masking with this expression turns to a single
       "Unsigned Bit Field Extract" UBFX instruction on ARM. */
    return ~((0xFFFFFFFFu) << n);
  } else {
    return kBrotliBitMask[n];
  }
}

typedef struct {
  brotli_reg_t val_;       /* pre-fetched bits */
  uint32_t bit_pos_;       /* current bit-reading position in val_ */
  const uint8_t* next_in;  /* the byte we're reading from */
  size_t avail_in;
} BrotliBitReader;

typedef struct {
  brotli_reg_t val_;
  uint32_t bit_pos_;
  const uint8_t* next_in;
  size_t avail_in;
} BrotliBitReaderState;

/* Initializes the BrotliBitReader fields. */
BROTLI_INTERNAL void BrotliInitBitReader(BrotliBitReader* const br);

/* Ensures that accumulator is not empty.
   May consume up to sizeof(brotli_reg_t) - 1 bytes of input.
   Returns BROTLI_FALSE if data is required but there is no input available.
   For BROTLI_ALIGNED_READ this function also prepares bit reader for aligned
   reading. */
BROTLI_INTERNAL BROTLI_BOOL BrotliWarmupBitReader(BrotliBitReader* const br);

/* Fallback for BrotliSafeReadBits32. Extracted as noninlined method to unburden
   the main code-path. Never called for RFC brotli streams, required only for
   "large-window" mode and other extensions. */
BROTLI_INTERNAL BROTLI_NOINLINE BROTLI_BOOL BrotliSafeReadBits32Slow(
    BrotliBitReader* const br, uint32_t n_bits, uint32_t* val);

static BROTLI_INLINE void BrotliBitReaderSaveState(
    BrotliBitReader* const from, BrotliBitReaderState* to) {
  to->val_ = from->val_;
  to->bit_pos_ = from->bit_pos_;
  to->next_in = from->next_in;
  to->avail_in = from->avail_in;
}

static BROTLI_INLINE void BrotliBitReaderRestoreState(
    BrotliBitReader* const to, BrotliBitReaderState* from) {
  to->val_ = from->val_;
  to->bit_pos_ = from->bit_pos_;
  to->next_in = from->next_in;
  to->avail_in = from->avail_in;
}

static BROTLI_INLINE uint32_t BrotliGetAvailableBits(
    const BrotliBitReader* br) {
  return (BROTLI_64_BITS ? 64 : 32) - br->bit_pos_;
}

/* Returns amount of unread bytes the bit reader still has buffered from the
   BrotliInput, including whole bytes in br->val_. Result is capped with
   maximal ring-buffer size (larger number won't be utilized anyway). */
static BROTLI_INLINE size_t BrotliGetRemainingBytes(BrotliBitReader* br) {
  static const size_t kCap = (size_t)1 << BROTLI_LARGE_MAX_WBITS;
  if (br->avail_in > kCap) return kCap;
  return br->avail_in + (BrotliGetAvailableBits(br) >> 3);
}

/* Checks if there is at least |num| bytes left in the input ring-buffer
   (excluding the bits remaining in br->val_). */
static BROTLI_INLINE BROTLI_BOOL BrotliCheckInputAmount(
    BrotliBitReader* const br, size_t num) {
  return TO_BROTLI_BOOL(br->avail_in >= num);
}

/* Guarantees that there are at least |n_bits| + 1 bits in accumulator.
   Precondition: accumulator contains at least 1 bit.
   |n_bits| should be in the range [1..24] for regular build. For portable
   non-64-bit little-endian build only 16 bits are safe to request. */
static BROTLI_INLINE void BrotliFillBitWindow(
    BrotliBitReader* const br, uint32_t n_bits) {
#if (BROTLI_64_BITS)
  if (!BROTLI_ALIGNED_READ && BROTLI_IS_CONSTANT(n_bits) && (n_bits <= 8)) {
    if (br->bit_pos_ >= 56) {
      br->val_ >>= 56;
      br->bit_pos_ ^= 56;  /* here same as -= 56 because of the if condition */
      br->val_ |= BROTLI_UNALIGNED_LOAD64LE(br->next_in) << 8;
      br->avail_in -= 7;
      br->next_in += 7;
    }
  } else if (
      !BROTLI_ALIGNED_READ && BROTLI_IS_CONSTANT(n_bits) && (n_bits <= 16)) {
    if (br->bit_pos_ >= 48) {
      br->val_ >>= 48;
      br->bit_pos_ ^= 48;  /* here same as -= 48 because of the if condition */
      br->val_ |= BROTLI_UNALIGNED_LOAD64LE(br->next_in) << 16;
      br->avail_in -= 6;
      br->next_in += 6;
    }
  } else {
    if (br->bit_pos_ >= 32) {
      br->val_ >>= 32;
      br->bit_pos_ ^= 32;  /* here same as -= 32 because of the if condition */
      br->val_ |= ((uint64_t)BROTLI_UNALIGNED_LOAD32LE(br->next_in)) << 32;
      br->avail_in -= BROTLI_SHORT_FILL_BIT_WINDOW_READ;
      br->next_in += BROTLI_SHORT_FILL_BIT_WINDOW_READ;
    }
  }
#else
  if (!BROTLI_ALIGNED_READ && BROTLI_IS_CONSTANT(n_bits) && (n_bits <= 8)) {
    if (br->bit_pos_ >= 24) {
      br->val_ >>= 24;
      br->bit_pos_ ^= 24;  /* here same as -= 24 because of the if condition */
      br->val_ |= BROTLI_UNALIGNED_LOAD32LE(br->next_in) << 8;
      br->avail_in -= 3;
      br->next_in += 3;
    }
  } else {
    if (br->bit_pos_ >= 16) {
      br->val_ >>= 16;
      br->bit_pos_ ^= 16;  /* here same as -= 16 because of the if condition */
      br->val_ |= ((uint32_t)BROTLI_UNALIGNED_LOAD16LE(br->next_in)) << 16;
      br->avail_in -= BROTLI_SHORT_FILL_BIT_WINDOW_READ;
      br->next_in += BROTLI_SHORT_FILL_BIT_WINDOW_READ;
    }
  }
#endif
}

/* Mostly like BrotliFillBitWindow, but guarantees only 16 bits and reads no
   more than BROTLI_SHORT_FILL_BIT_WINDOW_READ bytes of input. */
static BROTLI_INLINE void BrotliFillBitWindow16(BrotliBitReader* const br) {
  BrotliFillBitWindow(br, 17);
}

/* Tries to pull one byte of input to accumulator.
   Returns BROTLI_FALSE if there is no input available. */
static BROTLI_INLINE BROTLI_BOOL BrotliPullByte(BrotliBitReader* const br) {
  if (br->avail_in == 0) {
    return BROTLI_FALSE;
  }
  br->val_ >>= 8;
#if (BROTLI_64_BITS)
  br->val_ |= ((uint64_t)*br->next_in) << 56;
#else
  br->val_ |= ((uint32_t)*br->next_in) << 24;
#endif
  br->bit_pos_ -= 8;
  --br->avail_in;
  ++br->next_in;
  return BROTLI_TRUE;
}

/* Returns currently available bits.
   The number of valid bits could be calculated by BrotliGetAvailableBits. */
static BROTLI_INLINE brotli_reg_t BrotliGetBitsUnmasked(
    BrotliBitReader* const br) {
  return br->val_ >> br->bit_pos_;
}

/* Like BrotliGetBits, but does not mask the result.
   The result contains at least 16 valid bits. */
static BROTLI_INLINE uint32_t BrotliGet16BitsUnmasked(
    BrotliBitReader* const br) {
  BrotliFillBitWindow(br, 16);
  return (uint32_t)BrotliGetBitsUnmasked(br);
}

/* Returns the specified number of bits from |br| without advancing bit
   position. */
static BROTLI_INLINE uint32_t BrotliGetBits(
    BrotliBitReader* const br, uint32_t n_bits) {
  BrotliFillBitWindow(br, n_bits);
  return (uint32_t)BrotliGetBitsUnmasked(br) & BitMask(n_bits);
}

/* Tries to peek the specified amount of bits. Returns BROTLI_FALSE, if there
   is not enough input. */
static BROTLI_INLINE BROTLI_BOOL BrotliSafeGetBits(
    BrotliBitReader* const br, uint32_t n_bits, uint32_t* val) {
  while (BrotliGetAvailableBits(br) < n_bits) {
    if (!BrotliPullByte(br)) {
      return BROTLI_FALSE;
    }
  }
  *val = (uint32_t)BrotliGetBitsUnmasked(br) & BitMask(n_bits);
  return BROTLI_TRUE;
}

/* Advances the bit pos by |n_bits|. */
static BROTLI_INLINE void BrotliDropBits(
    BrotliBitReader* const br, uint32_t n_bits) {
  br->bit_pos_ += n_bits;
}

static BROTLI_INLINE void BrotliBitReaderUnload(BrotliBitReader* br) {
  uint32_t unused_bytes = BrotliGetAvailableBits(br) >> 3;
  uint32_t unused_bits = unused_bytes << 3;
  br->avail_in += unused_bytes;
  br->next_in -= unused_bytes;
  if (unused_bits == sizeof(br->val_) << 3) {
    br->val_ = 0;
  } else {
    br->val_ <<= unused_bits;
  }
  br->bit_pos_ += unused_bits;
}

/* Reads the specified number of bits from |br| and advances the bit pos.
   Precondition: accumulator MUST contain at least |n_bits|. */
static BROTLI_INLINE void BrotliTakeBits(
  BrotliBitReader* const br, uint32_t n_bits, uint32_t* val) {
  *val = (uint32_t)BrotliGetBitsUnmasked(br) & BitMask(n_bits);
  BROTLI_LOG(("[BrotliTakeBits]  %d %d %d val: %6x\n",
      (int)br->avail_in, (int)br->bit_pos_, (int)n_bits, (int)*val));
  BrotliDropBits(br, n_bits);
}

/* Reads the specified number of bits from |br| and advances the bit pos.
   Assumes that there is enough input to perform BrotliFillBitWindow.
   Up to 24 bits are allowed to be requested from this method. */
static BROTLI_INLINE uint32_t BrotliReadBits24(
    BrotliBitReader* const br, uint32_t n_bits) {
  BROTLI_DCHECK(n_bits <= 24);
  if (BROTLI_64_BITS || (n_bits <= 16)) {
    uint32_t val;
    BrotliFillBitWindow(br, n_bits);
    BrotliTakeBits(br, n_bits, &val);
    return val;
  } else {
    uint32_t low_val;
    uint32_t high_val;
    BrotliFillBitWindow(br, 16);
    BrotliTakeBits(br, 16, &low_val);
    BrotliFillBitWindow(br, 8);
    BrotliTakeBits(br, n_bits - 16, &high_val);
    return low_val | (high_val << 16);
  }
}

/* Same as BrotliReadBits24, but allows reading up to 32 bits. */
static BROTLI_INLINE uint32_t BrotliReadBits32(
    BrotliBitReader* const br, uint32_t n_bits) {
  BROTLI_DCHECK(n_bits <= 32);
  if (BROTLI_64_BITS || (n_bits <= 16)) {
    uint32_t val;
    BrotliFillBitWindow(br, n_bits);
    BrotliTakeBits(br, n_bits, &val);
    return val;
  } else {
    uint32_t low_val;
    uint32_t high_val;
    BrotliFillBitWindow(br, 16);
    BrotliTakeBits(br, 16, &low_val);
    BrotliFillBitWindow(br, 16);
    BrotliTakeBits(br, n_bits - 16, &high_val);
    return low_val | (high_val << 16);
  }
}

/* Tries to read the specified amount of bits. Returns BROTLI_FALSE, if there
   is not enough input. |n_bits| MUST be positive.
   Up to 24 bits are allowed to be requested from this method. */
static BROTLI_INLINE BROTLI_BOOL BrotliSafeReadBits(
    BrotliBitReader* const br, uint32_t n_bits, uint32_t* val) {
  BROTLI_DCHECK(n_bits <= 24);
  while (BrotliGetAvailableBits(br) < n_bits) {
    if (!BrotliPullByte(br)) {
      return BROTLI_FALSE;
    }
  }
  BrotliTakeBits(br, n_bits, val);
  return BROTLI_TRUE;
}

/* Same as BrotliSafeReadBits, but allows reading up to 32 bits. */
static BROTLI_INLINE BROTLI_BOOL BrotliSafeReadBits32(
    BrotliBitReader* const br, uint32_t n_bits, uint32_t* val) {
  BROTLI_DCHECK(n_bits <= 32);
  if (BROTLI_64_BITS || (n_bits <= 24)) {
    while (BrotliGetAvailableBits(br) < n_bits) {
      if (!BrotliPullByte(br)) {
        return BROTLI_FALSE;
      }
    }
    BrotliTakeBits(br, n_bits, val);
    return BROTLI_TRUE;
  } else {
    return BrotliSafeReadBits32Slow(br, n_bits, val);
  }
}

/* Advances the bit reader position to the next byte boundary and verifies
   that any skipped bits are set to zero. */
static BROTLI_INLINE BROTLI_BOOL BrotliJumpToByteBoundary(BrotliBitReader* br) {
  uint32_t pad_bits_count = BrotliGetAvailableBits(br) & 0x7;
  uint32_t pad_bits = 0;
  if (pad_bits_count != 0) {
    BrotliTakeBits(br, pad_bits_count, &pad_bits);
  }
  return TO_BROTLI_BOOL(pad_bits == 0);
}

/* Copies remaining input bytes stored in the bit reader to the output. Value
   |num| may not be larger than BrotliGetRemainingBytes. The bit reader must be
   warmed up again after this. */
static BROTLI_INLINE void BrotliCopyBytes(uint8_t* dest,
                                          BrotliBitReader* br, size_t num) {
  while (BrotliGetAvailableBits(br) >= 8 && num > 0) {
    *dest = (uint8_t)BrotliGetBitsUnmasked(br);
    BrotliDropBits(br, 8);
    ++dest;
    --num;
  }
  memcpy(dest, br->next_in, num);
  br->avail_in -= num;
  br->next_in += num;
}

#if defined(__cplusplus) || defined(c_plusplus)
}  /* extern "C" */
#endif

#endif  /* BROTLI_DEC_BIT_READER_H_ */
