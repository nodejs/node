//===-- Definition of a class for mbstate_t and conversion -----*-- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_STRING_CONVERTER_H
#define LLVM_LIBC_SRC___SUPPORT_STRING_CONVERTER_H

#include "hdr/types/char32_t.h"
#include "hdr/types/char8_t.h"
#include "hdr/types/size_t.h"
#include "src/__support/CPP/type_traits.h"
#include "src/__support/common.h"
#include "src/__support/error_or.h"
#include "src/__support/wchar/character_converter.h"
#include "src/__support/wchar/mbstate.h"

namespace LIBC_NAMESPACE_DECL {
namespace internal {

template <typename T> class StringConverter {
private:
  CharacterConverter cr;
  const T *src;
  size_t src_len;
  size_t src_idx;

  // # of pops we are allowed to perform (essentially size of the dest buffer)
  size_t num_to_write;

  LIBC_INLINE ErrorOr<size_t> pushFullCharacter() {
    size_t num_pushed;
    for (num_pushed = 0; !cr.isFull() && src_idx + num_pushed < src_len;
         ++num_pushed) {
      int err = cr.push(src[src_idx + num_pushed]);
      if (err != 0)
        return Error(err);
    }

    // if we aren't able to read a full character from the source string
    if (src_idx + num_pushed == src_len && !cr.isFull()) {
      src_idx += num_pushed;
      return Error(-1);
    }

    return num_pushed;
  }

public:
  LIBC_INLINE StringConverter(const T *s, mbstate *ps, size_t dstlen,
                              size_t srclen = SIZE_MAX)
      : cr(ps), src(s), src_len(srclen), src_idx(0), num_to_write(dstlen) {}

  template <typename CharType> LIBC_INLINE ErrorOr<CharType> pop() {
    if (num_to_write == 0)
      return Error(-1);

    if (cr.isEmpty() || src_idx == 0) {
      auto src_elements_read = pushFullCharacter();
      if (!src_elements_read.has_value())
        return Error(src_elements_read.error());

      if (cr.sizeAs<CharType>() > num_to_write) {
        cr.clear();
        return Error(-1);
      }

      src_idx += src_elements_read.value();
    }

    ErrorOr<CharType> out = cr.pop<CharType>();
    // if out isn't null terminator or an error
    if (out.has_value() && out.value() == 0)
      src_len = src_idx;

    num_to_write--;

    return out;
  }

  LIBC_INLINE size_t getSourceIndex() { return src_idx; }
};

} // namespace internal
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_STRING_CONVERTER_H
