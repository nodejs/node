//===-- Standalone implementation std::string_view --------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_CPP_STRING_VIEW_H
#define LLVM_LIBC_SRC___SUPPORT_CPP_STRING_VIEW_H

#include "limits.h"
#include "src/__support/common.h"
#include "src/__support/macros/config.h"

#include <stddef.h>

namespace LIBC_NAMESPACE_DECL {
namespace cpp {

template <typename CharT> class basic_string_view;

using string_view = basic_string_view<char>;
using wstring_view = basic_string_view<wchar_t>;

// This is a very simple alternate of the std::basic_string_view class template.
// There is no bounds check performed in any of the methods. The callers are
// expected to do the checks before invoking the methods.
//
// This class will be extended as needed in future.
template <typename CharT> class basic_string_view {
private:
  const CharT *Data;
  size_t Len;

  LIBC_INLINE static constexpr size_t min(size_t A, size_t B) {
    return A <= B ? A : B;
  }

  LIBC_INLINE static constexpr int compareN(const CharT *Lhs, const CharT *Rhs,
                                            size_t Length) {
    for (size_t i = 0; i < Length; ++i)
      if (Lhs[i] != Rhs[i])
        return Lhs[i] < Rhs[i] ? -1 : 1;
    return 0;
  }

  LIBC_INLINE static constexpr size_t length(const CharT *Str) {
    for (const CharT *End = Str;; ++End)
      if (*End == CharT{0})
        return static_cast<size_t>(End - Str);
  }

  LIBC_INLINE constexpr bool equals(basic_string_view Other) const {
    return (Len == Other.Len && compareN(Data, Other.Data, Other.Len) == 0);
  }

public:
  using value_type = CharT;
  using size_type = size_t;
  using difference_type = ptrdiff_t;
  using pointer = CharT *;
  using const_pointer = const CharT *;
  using reference = CharT &;
  using const_reference = const CharT &;
  using const_iterator = CharT *;
  using iterator = const_iterator;

  // special value equal to the maximum value representable by the type
  // size_type.
  LIBC_INLINE_VAR static constexpr size_t npos =
      cpp::numeric_limits<size_t>::max();

  LIBC_INLINE constexpr basic_string_view() : Data(nullptr), Len(0) {}

  // Assumes Str is a null-terminated string. The length of the string does
  // not include the terminating null character.
  // Preconditions: [Str, Str + ​length(Str)) is a valid range.
  LIBC_INLINE constexpr basic_string_view(const CharT *Str)
      : Data(Str), Len(length(Str)) {}

  // Preconditions: [Str, Str + N) is a valid range.
  LIBC_INLINE constexpr basic_string_view(const CharT *Str, size_t N)
      : Data(Str), Len(N) {}

  LIBC_INLINE constexpr const CharT *data() const { return Data; }

  // Returns the size of the basic_string_view.
  LIBC_INLINE constexpr size_t size() const { return Len; }

  // Returns whether the basic_string_view is empty.
  LIBC_INLINE constexpr bool empty() const { return Len == 0; }

  // Returns an iterator to the first character of the view.
  LIBC_INLINE constexpr const CharT *begin() const { return Data; }

  // Returns an iterator to the character following the last character of the
  // view.
  LIBC_INLINE constexpr const CharT *end() const { return Data + Len; }

  // Returns a const reference to the character at specified location pos.
  // No bounds checking is performed: the behavior is undefined if pos >=
  // size().
  LIBC_INLINE constexpr const CharT &operator[](size_t Index) const {
    return Data[Index];
  }

  /// compare - Compare two strings; the result is -1, 0, or 1 if this string
  /// is lexicographically less than, equal to, or greater than the \p Other.
  LIBC_INLINE constexpr int compare(basic_string_view Other) const {
    // Check the prefix for a mismatch.
    if (int Res = compareN(Data, Other.Data, min(Len, Other.Len)))
      return Res;
    // Otherwise the prefixes match, so we only need to check the lengths.
    if (Len == Other.Len)
      return 0;
    return Len < Other.Len ? -1 : 1;
  }

  LIBC_INLINE constexpr bool operator==(basic_string_view Other) const {
    return equals(Other);
  }
  LIBC_INLINE constexpr bool operator!=(basic_string_view Other) const {
    return !(*this == Other);
  }
  LIBC_INLINE constexpr bool operator<(basic_string_view Other) const {
    return compare(Other) == -1;
  }
  LIBC_INLINE constexpr bool operator<=(basic_string_view Other) const {
    return compare(Other) != 1;
  }
  LIBC_INLINE constexpr bool operator>(basic_string_view Other) const {
    return compare(Other) == 1;
  }
  LIBC_INLINE constexpr bool operator>=(basic_string_view Other) const {
    return compare(Other) != -1;
  }

  // Moves the start of the view forward by n characters.
  // The behavior is undefined if n > size().
  LIBC_INLINE constexpr void remove_prefix(size_t N) {
    Len -= N;
    Data += N;
  }

  // Moves the end of the view back by n characters.
  // The behavior is undefined if n > size().
  LIBC_INLINE constexpr void remove_suffix(size_t N) { Len -= N; }

  // Check if this string starts with the given Prefix.
  LIBC_INLINE constexpr bool starts_with(basic_string_view Prefix) const {
    return Len >= Prefix.Len && compareN(Data, Prefix.Data, Prefix.Len) == 0;
  }

  // Check if this string starts with the given Prefix.
  LIBC_INLINE constexpr bool starts_with(const CharT Prefix) const {
    return !empty() && front() == Prefix;
  }

  // Check if this string ends with the given Prefix.
  LIBC_INLINE constexpr bool ends_with(const CharT Suffix) const {
    return !empty() && back() == Suffix;
  }

  // Check if this string ends with the given Suffix.
  LIBC_INLINE constexpr bool ends_with(basic_string_view Suffix) const {
    return Len >= Suffix.Len &&
           compareN(end() - Suffix.Len, Suffix.Data, Suffix.Len) == 0;
  }

  // Return a reference to the substring from [Start, Start + N).
  //
  // Start The index of the starting character in the substring; if the index
  // is npos or greater than the length of the string then the empty substring
  // will be returned.
  //
  // N The number of characters to included in the substring. If N exceeds the
  // number of characters remaining in the string, the string suffix (starting
  // with Start) will be returned.
  LIBC_INLINE constexpr basic_string_view substr(size_t Start,
                                                 size_t N = npos) const {
    Start = min(Start, Len);
    return basic_string_view(Data + Start, min(N, Len - Start));
  }

  // front - Get the first character in the string.
  LIBC_INLINE constexpr CharT front() const { return Data[0]; }

  // back - Get the last character in the string.
  LIBC_INLINE constexpr CharT back() const { return Data[Len - 1]; }

  // Finds the first occurence of c in this view, starting at position From.
  LIBC_INLINE constexpr size_t find_first_of(const CharT c,
                                             size_t From = 0) const {
    for (size_t Pos = From; Pos < size(); ++Pos)
      if ((*this)[Pos] == c)
        return Pos;
    return npos;
  }

  // Finds the last occurence of c in this view, ending at position End.
  LIBC_INLINE constexpr size_t find_last_of(const CharT c,
                                            size_t End = npos) const {
    End = End >= size() ? size() : End + 1;
    for (; End > 0; --End)
      if ((*this)[End - 1] == c)
        return End - 1;
    return npos;
  }

  LIBC_INLINE constexpr size_t find_last_not_of(const char c,
                                                size_t end = npos) const {
    end = end >= size() ? size() : end + 1;
    for (; end > 0; --end)
      if ((*this)[end - 1] != c)
        return end - 1;
    return npos;
  }

  // Finds the first character not equal to c in this view, starting at
  // position From.
  LIBC_INLINE constexpr size_t find_first_not_of(const CharT c,
                                                 size_t From = 0) const {
    for (size_t Pos = From; Pos < size(); ++Pos)
      if ((*this)[Pos] != c)
        return Pos;
    return npos;
  }

  // Check if this view contains the given character.
  LIBC_INLINE constexpr bool contains(CharT c) const {
    return find_first_of(c) != npos;
  }
};

} // namespace cpp
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_CPP_STRING_VIEW_H
