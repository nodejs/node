// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OSTREAMS_H_
#define V8_OSTREAMS_H_

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "include/v8config.h"
#include "src/base/macros.h"

namespace v8 {
namespace internal {

// An abstract base class for output streams with a cut-down standard interface.
class OStream {
 public:
  OStream() : hex_(false) { }
  virtual ~OStream() { }

  // For manipulators like 'os << endl' or 'os << flush', etc.
  OStream& operator<<(OStream& (*manipulator)(OStream& os)) {
    return manipulator(*this);
  }

  // Numeric conversions.
  OStream& operator<<(short x);  // NOLINT(runtime/int)
  OStream& operator<<(unsigned short x);  // NOLINT(runtime/int)
  OStream& operator<<(int x);
  OStream& operator<<(unsigned int x);
  OStream& operator<<(long x);  // NOLINT(runtime/int)
  OStream& operator<<(unsigned long x);  // NOLINT(runtime/int)
  OStream& operator<<(long long x);  // NOLINT(runtime/int)
  OStream& operator<<(unsigned long long x);  // NOLINT(runtime/int)
  OStream& operator<<(double x);
  OStream& operator<<(void* x);

  // Character output.
  OStream& operator<<(char x);
  OStream& operator<<(signed char x);
  OStream& operator<<(unsigned char x);
  OStream& operator<<(const char* s) { return write(s, strlen(s)); }
  OStream& put(char c) { return write(&c, 1); }

  // Primitive format flag handling, can be extended if needed.
  OStream& dec();
  OStream& hex();

  virtual OStream& write(const char* s, size_t n) = 0;
  virtual OStream& flush() = 0;

 private:
  template<class T> OStream& print(const char* format, T x);

  bool hex_;

  DISALLOW_COPY_AND_ASSIGN(OStream);
};


// Some manipulators.
OStream& flush(OStream& os);  // NOLINT(runtime/references)
OStream& endl(OStream& os);  // NOLINT(runtime/references)
OStream& dec(OStream& os);  // NOLINT(runtime/references)
OStream& hex(OStream& os);  // NOLINT(runtime/references)


// An output stream writing to a character buffer.
class OStringStream: public OStream {
 public:
  OStringStream() : size_(0), capacity_(32), data_(allocate(capacity_)) {
    data_[0] = '\0';
  }
  ~OStringStream() { deallocate(data_, capacity_); }

  size_t size() const { return size_; }
  size_t capacity() const { return capacity_; }
  const char* data() const { return data_; }

  // Internally, our character data is always 0-terminated.
  const char* c_str() const { return data(); }

  virtual OStringStream& write(const char* s, size_t n) OVERRIDE;
  virtual OStringStream& flush() OVERRIDE;

 private:
  // Primitive allocator interface, can be extracted if needed.
  static char* allocate (size_t n) { return new char[n]; }
  static void deallocate (char* s, size_t n) { delete[] s; }

  void reserve(size_t requested_capacity);

  size_t size_;
  size_t capacity_;
  char* data_;

  DISALLOW_COPY_AND_ASSIGN(OStringStream);
};


// An output stream writing to a file.
class OFStream: public OStream {
 public:
  explicit OFStream(FILE* f) : f_(f) { }
  virtual ~OFStream() { }

  virtual OFStream& write(const char* s, size_t n) OVERRIDE;
  virtual OFStream& flush() OVERRIDE;

 private:
  FILE* const f_;

  DISALLOW_COPY_AND_ASSIGN(OFStream);
};


// Wrappers to disambiguate uint16_t and uc16.
struct AsUC16 {
  explicit AsUC16(uint16_t v) : value(v) {}
  uint16_t value;
};


struct AsReversiblyEscapedUC16 {
  explicit AsReversiblyEscapedUC16(uint16_t v) : value(v) {}
  uint16_t value;
};


// Writes the given character to the output escaping everything outside of
// printable/space ASCII range. Additionally escapes '\' making escaping
// reversible.
OStream& operator<<(OStream& os, const AsReversiblyEscapedUC16& c);

// Writes the given character to the output escaping everything outside
// of printable ASCII range.
OStream& operator<<(OStream& os, const AsUC16& c);
} }  // namespace v8::internal

#endif  // V8_OSTREAMS_H_
