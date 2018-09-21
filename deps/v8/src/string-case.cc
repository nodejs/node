// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/string-case.h"

#include "src/assert-scope.h"
#include "src/base/logging.h"
#include "src/globals.h"
#include "src/utils.h"

namespace v8 {
namespace internal {

#ifdef DEBUG
bool CheckFastAsciiConvert(char* dst, const char* src, int length, bool changed,
                           bool is_to_lower) {
  bool expected_changed = false;
  for (int i = 0; i < length; i++) {
    if (dst[i] == src[i]) continue;
    expected_changed = true;
    if (is_to_lower) {
      DCHECK('A' <= src[i] && src[i] <= 'Z');
      DCHECK(dst[i] == src[i] + ('a' - 'A'));
    } else {
      DCHECK('a' <= src[i] && src[i] <= 'z');
      DCHECK(dst[i] == src[i] - ('a' - 'A'));
    }
  }
  return (expected_changed == changed);
}
#endif

const uintptr_t kOneInEveryByte = kUintptrAllBitsSet / 0xFF;
const uintptr_t kAsciiMask = kOneInEveryByte << 7;

// Given a word and two range boundaries returns a word with high bit
// set in every byte iff the corresponding input byte was strictly in
// the range (m, n). All the other bits in the result are cleared.
// This function is only useful when it can be inlined and the
// boundaries are statically known.
// Requires: all bytes in the input word and the boundaries must be
// ASCII (less than 0x7F).
static inline uintptr_t AsciiRangeMask(uintptr_t w, char m, char n) {
  // Use strict inequalities since in edge cases the function could be
  // further simplified.
  DCHECK(0 < m && m < n);
  // Has high bit set in every w byte less than n.
  uintptr_t tmp1 = kOneInEveryByte * (0x7F + n) - w;
  // Has high bit set in every w byte greater than m.
  uintptr_t tmp2 = w + kOneInEveryByte * (0x7F - m);
  return (tmp1 & tmp2 & (kOneInEveryByte * 0x80));
}

template <bool is_lower>
int FastAsciiConvert(char* dst, const char* src, int length,
                     bool* changed_out) {
#ifdef DEBUG
  char* saved_dst = dst;
#endif
  const char* saved_src = src;
  DisallowHeapAllocation no_gc;
  // We rely on the distance between upper and lower case letters
  // being a known power of 2.
  DCHECK_EQ('a' - 'A', 1 << 5);
  // Boundaries for the range of input characters than require conversion.
  static const char lo = is_lower ? 'A' - 1 : 'a' - 1;
  static const char hi = is_lower ? 'Z' + 1 : 'z' + 1;
  bool changed = false;
  const char* const limit = src + length;

  // dst is newly allocated and always aligned.
  DCHECK(IsAligned(reinterpret_cast<intptr_t>(dst), sizeof(uintptr_t)));
  // Only attempt processing one word at a time if src is also aligned.
  if (IsAligned(reinterpret_cast<intptr_t>(src), sizeof(uintptr_t))) {
    // Process the prefix of the input that requires no conversion one aligned
    // (machine) word at a time.
    while (src <= limit - sizeof(uintptr_t)) {
      const uintptr_t w = *reinterpret_cast<const uintptr_t*>(src);
      if ((w & kAsciiMask) != 0) return static_cast<int>(src - saved_src);
      if (AsciiRangeMask(w, lo, hi) != 0) {
        changed = true;
        break;
      }
      *reinterpret_cast<uintptr_t*>(dst) = w;
      src += sizeof(uintptr_t);
      dst += sizeof(uintptr_t);
    }
    // Process the remainder of the input performing conversion when
    // required one word at a time.
    while (src <= limit - sizeof(uintptr_t)) {
      const uintptr_t w = *reinterpret_cast<const uintptr_t*>(src);
      if ((w & kAsciiMask) != 0) return static_cast<int>(src - saved_src);
      uintptr_t m = AsciiRangeMask(w, lo, hi);
      // The mask has high (7th) bit set in every byte that needs
      // conversion and we know that the distance between cases is
      // 1 << 5.
      *reinterpret_cast<uintptr_t*>(dst) = w ^ (m >> 2);
      src += sizeof(uintptr_t);
      dst += sizeof(uintptr_t);
    }
  }
  // Process the last few bytes of the input (or the whole input if
  // unaligned access is not supported).
  while (src < limit) {
    char c = *src;
    if ((c & kAsciiMask) != 0) return static_cast<int>(src - saved_src);
    if (lo < c && c < hi) {
      c ^= (1 << 5);
      changed = true;
    }
    *dst = c;
    ++src;
    ++dst;
  }

  DCHECK(
      CheckFastAsciiConvert(saved_dst, saved_src, length, changed, is_lower));

  *changed_out = changed;
  return length;
}

template int FastAsciiConvert<false>(char* dst, const char* src, int length,
                                     bool* changed_out);
template int FastAsciiConvert<true>(char* dst, const char* src, int length,
                                    bool* changed_out);

}  // namespace internal
}  // namespace v8
