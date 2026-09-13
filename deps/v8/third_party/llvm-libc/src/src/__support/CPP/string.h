//===-- A simple implementation of the string class -------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_CPP_STRING_H
#define LLVM_LIBC_SRC___SUPPORT_CPP_STRING_H

#include "hdr/func/free.h"
#include "hdr/func/malloc.h"
#include "hdr/func/realloc.h"
#include "src/__support/CPP/string_view.h"
#include "src/__support/integer_to_string.h" // IntegerToString
#include "src/__support/macros/config.h"
#include "src/__support/macros/null_check.h"
#include "src/string/memory_utils/inline_memcpy.h"
#include "src/string/memory_utils/inline_memset.h"
#include "src/string/string_utils.h" // string_length

#include <stddef.h> // size_t

namespace LIBC_NAMESPACE_DECL {
namespace cpp {
namespace {

char *realloc_or_die(char *ptr, size_t size) {
  void *new_ptr = ::realloc(ptr, size);
  // Out of memory: this is not handled in current implementation, so crash.
  LIBC_CRASH_ON_NULLPTR(new_ptr);
  return reinterpret_cast<char *>(new_ptr);
}

} // namespace

// This class mimics std::string but does not intend to be a full fledged
// implementation. Most notably it does not provide support for character traits
// nor custom allocator.
class string {
private:
  static constexpr char NULL_CHARACTER = '\0';
  static constexpr char *get_empty_string() {
    return const_cast<char *>(&NULL_CHARACTER);
  }

  // Backing data for the represented string.
  // This may point to NULL_CHARACTER or a heap-allocated buffer.
  char *buffer_ = get_empty_string();

  // Size of the string represented in buffer_.
  size_t size_ = 0;

  // The size of buffer_.
  size_t capacity_ = 0;

  constexpr void reset_no_deallocate() {
    buffer_ = get_empty_string();
    size_ = 0;
    capacity_ = 0;
  }

  void set_size_and_add_null_character(size_t size) {
    size_ = size;
    if (buffer_ != get_empty_string())
      buffer_[size_] = NULL_CHARACTER;
  }

public:
  LIBC_INLINE constexpr string() {}
  LIBC_INLINE string(const string &other) { this->operator+=(other); }
  LIBC_INLINE constexpr string(string &&other)
      : buffer_(other.buffer_), size_(other.size_), capacity_(other.capacity_) {
    other.reset_no_deallocate();
  }
  LIBC_INLINE string(const char *cstr, size_t count) {
    resize(count);
    inline_memcpy(buffer_, cstr, count);
  }
  LIBC_INLINE string(const string_view &view)
      : string(view.data(), view.size()) {}
  LIBC_INLINE string(const char *cstr)
      : string(cstr, ::LIBC_NAMESPACE::internal::string_length(cstr)) {}
  LIBC_INLINE string(size_t size_, char value) {
    resize(size_);
    static_assert(sizeof(char) == sizeof(uint8_t));
    inline_memset((void *)buffer_, static_cast<uint8_t>(value), size_);
  }

  LIBC_INLINE string &operator=(const string &other) {
    resize(0);
    return (*this) += other;
  }

  LIBC_INLINE string &operator=(char other) {
    resize(0);
    return (*this) += other;
  }

  LIBC_INLINE string &operator=(string &&other) {
    if (buffer_ != get_empty_string())
      ::free(buffer_);

    buffer_ = other.buffer_;
    size_ = other.size_;
    capacity_ = other.capacity_;
    other.reset_no_deallocate();
    return *this;
  }

  LIBC_INLINE string &operator=(const string_view &view) {
    return *this = string(view);
  }

  LIBC_INLINE ~string() {
    if (buffer_ != get_empty_string())
      ::free(buffer_);
  }

  // Returns the number of writable bytes in this string.
  // Does not include the string-managed null terminator.
  LIBC_INLINE constexpr size_t capacity() const {
    if (capacity_ == 0)
      return 0;
    return capacity_ - 1;
  }
  LIBC_INLINE constexpr size_t size() const { return size_; }
  LIBC_INLINE constexpr bool empty() const { return size_ == 0; }

  LIBC_INLINE constexpr const char *data() const { return buffer_; }
  LIBC_INLINE char *data() { return buffer_; }

  LIBC_INLINE constexpr const char *begin() const { return data(); }
  LIBC_INLINE char *begin() { return data(); }

  LIBC_INLINE constexpr const char *end() const { return data() + size_; }
  LIBC_INLINE char *end() { return data() + size_; }

  LIBC_INLINE constexpr const char &front() const { return data()[0]; }
  LIBC_INLINE char &front() { return data()[0]; }

  LIBC_INLINE constexpr const char &back() const { return data()[size_ - 1]; }
  LIBC_INLINE char &back() { return data()[size_ - 1]; }

  LIBC_INLINE constexpr const char &operator[](size_t index) const {
    return data()[index];
  }
  LIBC_INLINE char &operator[](size_t index) { return data()[index]; }

  LIBC_INLINE const char *c_str() const { return data(); }

  LIBC_INLINE operator string_view() const {
    return string_view(buffer_, size_);
  }

  LIBC_INLINE void reserve(size_t new_cap) {
    size_t allocation_size = new_cap + 1; // +1 for terminating '\0'
    if (allocation_size <= capacity_)
      return;

    // We extend the capacity to amortize buffer_ reallocations.
    // We choose to augment the value by 11 / 8, this is about +40% and division
    // by 8 is cheap. We guard the extension so the operation doesn't overflow.
    if (allocation_size < SIZE_MAX / 11)
      allocation_size = allocation_size * 11 / 8;

    if (buffer_ == get_empty_string()) {
      buffer_ = realloc_or_die(nullptr, allocation_size);
      buffer_[0] = NULL_CHARACTER;
    } else {
      buffer_ = realloc_or_die(buffer_, allocation_size);
    }

    capacity_ = allocation_size;
  }

  LIBC_INLINE void resize(size_t size) {
    // Avoid growing out of the static empty string during `resize(0)`,
    // which may happen in string constructors.
    if (size == size_)
      return;

    if (size > capacity()) {
      reserve(size);
      const size_t size_extension = size - size_;
      inline_memset(data() + size_, '\0', size_extension);
    }
    set_size_and_add_null_character(size);
  }

  // Releases the backing C-string.
  //
  // The returned pointer must be free'd by the caller.
  // This is a non-standard extension to std::string.
  LIBC_INLINE char *release_c_str() {
    if (buffer_ == get_empty_string()) {
      // Ensure the buffer is heap allocated,
      // so that it may later be passed to `free`.
      char *res = realloc_or_die(nullptr, 1);
      res[0] = '\0';
      return res;
    }

    char *res = buffer_;
    reset_no_deallocate();
    return res;
  }

  LIBC_INLINE string &operator+=(const string &rhs) {
    const size_t new_size = size_ + rhs.size();
    reserve(new_size);
    inline_memcpy(buffer_ + size_, rhs.data(), rhs.size());
    set_size_and_add_null_character(new_size);
    return *this;
  }

  LIBC_INLINE string &operator+=(const char c) {
    const size_t new_size = size_ + 1;
    reserve(new_size);
    buffer_[size_] = c;
    set_size_and_add_null_character(new_size);
    return *this;
  }
};

LIBC_INLINE bool operator==(const string &lhs, const string &rhs) {
  return string_view(lhs) == string_view(rhs);
}
LIBC_INLINE bool operator!=(const string &lhs, const string &rhs) {
  return string_view(lhs) != string_view(rhs);
}
LIBC_INLINE bool operator<(const string &lhs, const string &rhs) {
  return string_view(lhs) < string_view(rhs);
}
LIBC_INLINE bool operator<=(const string &lhs, const string &rhs) {
  return string_view(lhs) <= string_view(rhs);
}
LIBC_INLINE bool operator>(const string &lhs, const string &rhs) {
  return string_view(lhs) > string_view(rhs);
}
LIBC_INLINE bool operator>=(const string &lhs, const string &rhs) {
  return string_view(lhs) >= string_view(rhs);
}

LIBC_INLINE string operator+(const string &lhs, const string &rhs) {
  string Tmp(lhs);
  return Tmp += rhs;
}
LIBC_INLINE string operator+(const string &lhs, const char *rhs) {
  return lhs + string(rhs);
}
LIBC_INLINE string operator+(const char *lhs, const string &rhs) {
  return string(lhs) + rhs;
}

namespace internal {
template <typename T> string to_dec_string(T value) {
  const IntegerToString<T> buffer(value);
  return buffer.view();
}
} // namespace internal

LIBC_INLINE string to_string(int value) {
  return internal::to_dec_string<int>(value);
}
LIBC_INLINE string to_string(long value) {
  return internal::to_dec_string<long>(value);
}
LIBC_INLINE string to_string(long long value) {
  return internal::to_dec_string<long long>(value);
}
LIBC_INLINE string to_string(unsigned value) {
  return internal::to_dec_string<unsigned>(value);
}
LIBC_INLINE string to_string(unsigned long value) {
  return internal::to_dec_string<unsigned long>(value);
}
LIBC_INLINE string to_string(unsigned long long value) {
  return internal::to_dec_string<unsigned long long>(value);
}

// TODO: Support floating point
// LIBC_INLINE string to_string(float value);
// LIBC_INLINE string to_string(double value);
// LIBC_INLINE string to_string(long double value);

} // namespace cpp
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_CPP_STRING_H
