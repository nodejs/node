// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/ostreams.h"

#if V8_OS_WIN
#define snprintf sprintf_s
#endif

namespace v8 {
namespace internal {

OFStreamBase::OFStreamBase(FILE* f) : f_(f) {}


OFStreamBase::~OFStreamBase() {}


OFStreamBase::int_type OFStreamBase::sync() {
  std::fflush(f_);
  return 0;
}


OFStreamBase::int_type OFStreamBase::overflow(int_type c) {
  return (c != EOF) ? std::fputc(c, f_) : c;
}


OFStream::OFStream(FILE* f) : OFStreamBase(f), std::ostream(this) {}


OFStream::~OFStream() {}


namespace {

// Locale-independent predicates.
bool IsPrint(uint16_t c) { return 0x20 <= c && c <= 0x7e; }
bool IsSpace(uint16_t c) { return (0x9 <= c && c <= 0xd) || c == 0x20; }
bool IsOK(uint16_t c) { return (IsPrint(c) || IsSpace(c)) && c != '\\'; }


std::ostream& PrintUC16(std::ostream& os, uint16_t c, bool (*pred)(uint16_t)) {
  char buf[10];
  const char* format = pred(c) ? "%c" : (c <= 0xff) ? "\\x%02x" : "\\u%04x";
  snprintf(buf, sizeof(buf), format, c);
  return os << buf;
}

}  // namespace


std::ostream& operator<<(std::ostream& os, const AsReversiblyEscapedUC16& c) {
  return PrintUC16(os, c.value, IsOK);
}


std::ostream& operator<<(std::ostream& os, const AsUC16& c) {
  return PrintUC16(os, c.value, IsPrint);
}

}  // namespace internal
}  // namespace v8
