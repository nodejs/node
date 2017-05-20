
#ifndef SRC_STRING_UTILS_H_
#define SRC_STRING_UTILS_H_

#include <cstddef>
#include <cstdint>

namespace node {
namespace stringutils {
  inline static bool contains_non_ascii_slow(const char* buf, size_t len) {
    for (size_t i = 0; i < len; ++i) {
      if (buf[i] & 0x80)
        return true;
      }
      return false;
  }

  inline bool ContainsNonAscii(const char* src, size_t len) {
    if (len < 16) {
        return contains_non_ascii_slow(src, len);
    }

    const unsigned bytes_per_word = sizeof(uintptr_t);
    const unsigned align_mask = bytes_per_word - 1;
    const unsigned unaligned = reinterpret_cast<uintptr_t>(src) & align_mask;

    if (unaligned > 0) {
        const unsigned n = bytes_per_word - unaligned;
        if (contains_non_ascii_slow(src, n))
        return true;
        src += n;
        len -= n;
    }


    #if defined(_WIN64) || defined(_LP64)
    const uintptr_t mask = 0x8080808080808080ll;
    #else
    const uintptr_t mask = 0x80808080l;
    #endif

    const uintptr_t* srcw = reinterpret_cast<const uintptr_t*>(src);

    for (size_t i = 0, n = len / bytes_per_word; i < n; ++i) {
        if (srcw[i] & mask)
        return true;
    }

    const unsigned remainder = len & align_mask;
    if (remainder > 0) {
        const size_t offset = len - remainder;
        if (contains_non_ascii_slow(src + offset, remainder))
        return true;
    }

    return false;
  }
}  // namespace stringutils
}  // namespace node

#endif  // SRC_STRING_UTILS_H_
