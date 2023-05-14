#ifndef SRC_NODE_SIMD_H_
#define SRC_NODE_SIMD_H_

#if defined(__aarch64__) || defined(_M_ARM64)
#define NODE_HAS_SIMD_NEON 1
#endif

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include <string_view>

namespace node {
namespace simd {

uint32_t utf8_byte_length(const uint8_t* input, size_t length);

}  // namespace simd
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_NODE_SIMD_H_
