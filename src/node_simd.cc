#include "node_simd.h"

#include <string_view>

#if NODE_HAS_SIMD_NEON
#include <arm_neon.h>
#endif

namespace node {
namespace simd {

#if NODE_HAS_SIMD_NEON
uint32_t utf8_byte_length(const uint8_t* data, size_t length) {
  uint64_t result{0};

  const int lanes = sizeof(uint8x16_t);
  const int max_sra_count = 256 / lanes;  // Avoid overflowing vaddvq_u8.
  const int unrolls = max_sra_count;
  const int unrolled_lanes = lanes * unrolls;

  const uint8_t* unroll_end = data + (length / unrolled_lanes) * unrolled_lanes;
  uint32_t length_after_unroll = length % unrolled_lanes;
  for (; data < unroll_end;) {
    uint8x16_t acc = {};
    for (int i = 0; i < unrolls; ++i, data += lanes) {
      uint8x16_t chunk = vld1q_u8(data);
      acc = vsraq_n_u8(acc, chunk, 7);
    }
    result += vaddvq_u8(acc);
  }

  const uint8_t* simd_end = data + (length_after_unroll / lanes) * lanes;
  uint32_t length_after_simd = length % lanes;
  uint8x16_t acc = {};
  for (; data < simd_end; data += lanes) {
    uint8x16_t chunk = vld1q_u8(data);
    acc = vsraq_n_u8(acc, chunk, 7);
  }
  result += vaddvq_u8(acc);

  const uint8_t* scalar_end = data + length_after_simd;
  for (; data < scalar_end; data += 1) {
    result += *data >> 7;
  }

  return result + length;
}
#else
uint32_t utf8_byte_length(const uint8_t* data, size_t length) {
  uint32_t result = 0;
  for (uint32_t i = 0; i < length; ++i) {
    result += (data[i] >> 7);
  }
  result += length;
  return result;
}
#endif

}  // namespace simd
}  // namespace node
