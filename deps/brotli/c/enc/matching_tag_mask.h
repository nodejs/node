#ifndef THIRD_PARTY_BROTLI_ENC_MATCHING_TAG_MASK_H_
#define THIRD_PARTY_BROTLI_ENC_MATCHING_TAG_MASK_H_

#include "../common/platform.h"

#if defined(__SSE2__) || defined(_M_AMD64) || \
    (defined(_M_IX86) && defined(_M_IX86_FP) && (_M_IX86_FP >= 2))
#define SUPPORTS_SSE_2
#endif

#if defined(SUPPORTS_SSE_2)
#include <immintrin.h>
#endif


static BROTLI_INLINE uint64_t GetMatchingTagMask(
    size_t chunk_count, const uint8_t tag,
    const uint8_t* BROTLI_RESTRICT tag_bucket, const size_t head) {
  uint64_t matches = 0;
#if defined(SUPPORTS_SSE_2)
  const __m128i comparison_mask = _mm_set1_epi8((char)tag);
  size_t i;
  for (i = 0; i < chunk_count && i < 4; i++) {
    const __m128i chunk =
        _mm_loadu_si128((const __m128i*)(const void*)(tag_bucket + 16 * i));
    const __m128i equal_mask = _mm_cmpeq_epi8(chunk, comparison_mask);
    matches |= (uint64_t)_mm_movemask_epi8(equal_mask) << 16 * i;
  }
#else
  const int chunk_size = sizeof(size_t);
  const size_t shift_amount = ((chunk_size * 8) - chunk_size);
  const size_t xFF = ~((size_t)0);
  const size_t x01 = xFF / 0xFF;
  const size_t x80 = x01 << 7;
  const size_t splat_char = tag * x01;
  int i = ((int)chunk_count * 16) - chunk_size;
  BROTLI_DCHECK((sizeof(size_t) == 4) || (sizeof(size_t) == 8));
#if BROTLI_LITTLE_ENDIAN
  const size_t extractMagic = (xFF / 0x7F) >> chunk_size;
  do {
      size_t chunk = BrotliUnalignedReadSizeT(&tag_bucket[i]);
      chunk ^= splat_char;
      chunk = (((chunk | x80) - x01) | chunk) & x80;
      matches <<= chunk_size;
      matches |= (chunk * extractMagic) >> shift_amount;
      i -= chunk_size;
  } while (i >= 0);
#else
  const size_t msb = xFF ^ (xFF >> 1);
  const size_t extractMagic = (msb / 0x1FF) | msb;
  do {
      size_t chunk = BrotliUnalignedReadSizeT(&tag_bucket[i]);
      chunk ^= splat_char;
      chunk = (((chunk | x80) - x01) | chunk) & x80;
      matches <<= chunk_size;
      matches |= ((chunk >> 7) * extractMagic) >> shift_amount;
      i -= chunk_size;
  } while (i >= 0);
#endif
  matches = ~matches;
#endif
  if (chunk_count == 1) return BrotliRotateRight16((uint16_t)matches, head);
  if (chunk_count == 2) return BrotliRotateRight32((uint32_t)matches, head);
  return BrotliRotateRight64(matches, head);
}

#undef SUPPORTS_SSE_2

#endif  // THIRD_PARTY_BROTLI_ENC_MATCHING_TAG_MASK_H_
