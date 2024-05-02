// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/base/strings.h"

#include <cstdint>
#include <cstring>
#include <limits>

#include "src/base/platform/platform.h"

namespace v8 {
namespace base {

int VSNPrintF(Vector<char> str, const char* format, va_list args) {
  return OS::VSNPrintF(str.begin(), str.length(), format, args);
}

int SNPrintF(Vector<char> str, const char* format, ...) {
  va_list args;
  va_start(args, format);
  int result = VSNPrintF(str, format, args);
  va_end(args);
  return result;
}

void StrNCpy(base::Vector<char> dest, const char* src, size_t n) {
  base::OS::StrNCpy(dest.begin(), dest.length(), src, n);
}

}  // namespace base
}  // namespace v8
