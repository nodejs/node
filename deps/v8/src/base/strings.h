// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BASE_STRINGS_H_
#define V8_BASE_STRINGS_H_

#include "src/base/base-export.h"
#include "src/base/macros.h"
#include "src/base/vector.h"

namespace v8 {
namespace base {

// Latin1/UTF-16 constants
// Code-point values in Unicode 4.0 are 21 bits wide.
// Code units in UTF-16 are 16 bits wide.
using uc16 = uint16_t;
using uc32 = uint32_t;
constexpr int kUC16Size = sizeof(uc16);

V8_BASE_EXPORT int PRINTF_FORMAT(2, 0)
    VSNPrintF(Vector<char> str, const char* format, va_list args);

// Safe formatting print. Ensures that str is always null-terminated.
// Returns the number of chars written, or -1 if output was truncated.
V8_BASE_EXPORT int PRINTF_FORMAT(2, 3)
    SNPrintF(Vector<char> str, const char* format, ...);

V8_BASE_EXPORT void StrNCpy(base::Vector<char> dest, const char* src, size_t n);

// Returns the value (0 .. 15) of a hexadecimal character c.
// If c is not a legal hexadecimal character, returns a value < 0.
inline int HexValue(uc32 c) {
  c -= '0';
  if (static_cast<unsigned>(c) <= 9) return c;
  c = (c | 0x20) - ('a' - '0');  // detect 0x11..0x16 and 0x31..0x36.
  if (static_cast<unsigned>(c) <= 5) return c + 10;
  return -1;
}

inline char HexCharOfValue(int value) {
  DCHECK(0 <= value && value <= 16);
  if (value < 10) return value + '0';
  return value - 10 + 'A';
}

}  // namespace base
}  // namespace v8

#endif  // V8_BASE_STRINGS_H_
