// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/ostreams.h"

#include <algorithm>
#include <cmath>

#include "src/base/platform/platform.h"  // For isinf/isnan with MSVC

#if V8_OS_WIN
#define snprintf sprintf_s
#endif

namespace v8 {
namespace internal {

// Be lazy and delegate the value=>char conversion to snprintf.
template<class T>
OStream& OStream::print(const char* format, T x) {
  char buf[32];
  int n = snprintf(buf, sizeof(buf), format, x);
  return (n < 0) ? *this : write(buf, n);
}


OStream& OStream::operator<<(short x) {  // NOLINT(runtime/int)
  return print(hex_ ? "%hx" : "%hd", x);
}


OStream& OStream::operator<<(unsigned short x) {  // NOLINT(runtime/int)
  return print(hex_ ? "%hx" : "%hu", x);
}


OStream& OStream::operator<<(int x) {
  return print(hex_ ? "%x" : "%d", x);
}


OStream& OStream::operator<<(unsigned int x) {
  return print(hex_ ? "%x" : "%u", x);
}


OStream& OStream::operator<<(long x) {  // NOLINT(runtime/int)
  return print(hex_ ? "%lx" : "%ld", x);
}


OStream& OStream::operator<<(unsigned long x) {  // NOLINT(runtime/int)
  return print(hex_ ? "%lx" : "%lu", x);
}


OStream& OStream::operator<<(long long x) {  // NOLINT(runtime/int)
  return print(hex_ ? "%llx" : "%lld", x);
}


OStream& OStream::operator<<(unsigned long long x) {  // NOLINT(runtime/int)
  return print(hex_ ? "%llx" : "%llu", x);
}


OStream& OStream::operator<<(double x) {
  if (std::isinf(x)) return *this << (x < 0 ? "-inf" : "inf");
  if (std::isnan(x)) return *this << "nan";
  return print("%g", x);
}


OStream& OStream::operator<<(void* x) {
  return print("%p", x);
}


OStream& OStream::operator<<(char x) {
  return put(x);
}


OStream& OStream::operator<<(signed char x) {
  return put(x);
}


OStream& OStream::operator<<(unsigned char x) {
  return put(x);
}


OStream& OStream::dec() {
  hex_ = false;
  return *this;
}


OStream& OStream::hex() {
  hex_ = true;
  return *this;
}


OStream& flush(OStream& os) {  // NOLINT(runtime/references)
  return os.flush();
}


OStream& endl(OStream& os) {  // NOLINT(runtime/references)
  return flush(os.put('\n'));
}


OStream& hex(OStream& os) {  // NOLINT(runtime/references)
  return os.hex();
}


OStream& dec(OStream& os) {  // NOLINT(runtime/references)
  return os.dec();
}


OStringStream& OStringStream::write(const char* s, size_t n) {
  size_t new_size = size_ + n;
  if (new_size < size_) return *this;  // Overflow => no-op.
  reserve(new_size + 1);
  memcpy(data_ + size_, s, n);
  size_ = new_size;
  data_[size_] = '\0';
  return *this;
}


OStringStream& OStringStream::flush() {
  return *this;
}


void OStringStream::reserve(size_t requested_capacity) {
  if (requested_capacity <= capacity_) return;
  size_t new_capacity =  // Handle possible overflow by not doubling.
      std::max(std::max(capacity_ * 2, capacity_), requested_capacity);
  char * new_data = allocate(new_capacity);
  memcpy(new_data, data_, size_);
  deallocate(data_, capacity_);
  capacity_ = new_capacity;
  data_ = new_data;
}


OFStream& OFStream::write(const char* s, size_t n) {
  if (f_) fwrite(s, n, 1, f_);
  return *this;
}


OFStream& OFStream::flush() {
  if (f_) fflush(f_);
  return *this;
}


// Locale-independent predicates.
static bool IsPrint(uint16_t c) { return 0x20 <= c && c <= 0x7e; }
static bool IsSpace(uint16_t c) { return (0x9 <= c && c <= 0xd) || c == 0x20; }
static bool IsOK(uint16_t c) { return (IsPrint(c) || IsSpace(c)) && c != '\\'; }


static OStream& PrintUC16(OStream& os, uint16_t c, bool (*pred)(uint16_t)) {
  char buf[10];
  const char* format = pred(c) ? "%c" : (c <= 0xff) ? "\\x%02x" : "\\u%04x";
  snprintf(buf, sizeof(buf), format, c);
  return os << buf;
}


OStream& operator<<(OStream& os, const AsReversiblyEscapedUC16& c) {
  return PrintUC16(os, c.value, IsOK);
}


OStream& operator<<(OStream& os, const AsUC16& c) {
  return PrintUC16(os, c.value, IsPrint);
}
} }  // namespace v8::internal
