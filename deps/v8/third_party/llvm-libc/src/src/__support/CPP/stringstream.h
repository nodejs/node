//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// A simple implementation of a non-allocating string stream class.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_CPP_STRINGSTREAM_H
#define LLVM_LIBC_SRC___SUPPORT_CPP_STRINGSTREAM_H

#include "hdr/types/size_t.h"
#include "src/__support/CPP/span.h"
#include "src/__support/CPP/string_view.h"
#include "src/__support/CPP/type_traits.h"
#include "src/__support/integer_to_string.h"
#include "src/__support/macros/attributes.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {
namespace cpp {

// This class is to be used to write simple strings into a user provided buffer
// without any dynamic memory allocation. There is no requirement to mimic the
// C++ standard library class std::stringstream.
class StringStream {
  span<char> data;
  size_t write_ptr = 0; // The current write pointer
  bool err = false;     // If an error occurs while writing

  LIBC_INLINE constexpr void write(span<const char> bytes) {
    size_t i = 0;
    const size_t data_size = data.size();
    for (; write_ptr < data_size && i < bytes.size(); ++i, ++write_ptr)
      data[write_ptr] = bytes[i];
    if (i < bytes.size())
      err = true;
  }

public:
  static constexpr char ENDS = '\0';

  // Create a string stream which will write into |buf|.
  LIBC_INLINE constexpr StringStream(const span<char> &buf) : data(buf) {}

  // Return a string_view to the current characters in the stream. If a
  // null terminator was not explicitly written, then the return value
  // will not include one. In order to produce a string_view to a null
  // terminated string, write ENDS explicitly.
  LIBC_INLINE constexpr string_view str() const {
    span<const char> written = data.first(write_ptr);
    return string_view(written.data(), written.size());
  }

  // Write the characters from |str| to the stream.
  LIBC_INLINE constexpr StringStream &operator<<(string_view str) {
    write({str.data(), str.size()});
    return *this;
  }

  // Write the |val| as string.
  template <typename T, enable_if_t<is_integral_v<T>, int> = 0>
  LIBC_INLINE StringStream &operator<<(T val) {
    const IntegerToString<T> buffer(val);
    return *this << buffer.view();
  }

  template <typename T, enable_if_t<is_floating_point_v<T>, int> = 0>
  LIBC_INLINE StringStream &operator<<(T) {
    // If this specialization gets activated, then the static_assert will
    // trigger a compile error about missing floating point number support.
    static_assert(!is_floating_point_v<T>,
                  "Writing floating point numbers is not yet supported");
    return *this;
  }

  // Write a null-terminated string. The terminating null character is not
  // written to allow streaming to continue.
  LIBC_INLINE constexpr StringStream &operator<<(const char *str) {
    return operator<<(string_view(str));
  }

  // Write a single character.
  LIBC_INLINE constexpr StringStream &operator<<(char a) {
    write({&a, 1});
    return *this;
  }

  // Return true if any write operation(s) failed due to insufficient size.
  LIBC_INLINE constexpr bool overflow() const { return err; }

  LIBC_INLINE constexpr size_t bufsize() const { return data.size(); }
};

} // namespace cpp
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_CPP_STRINGSTREAM_H
