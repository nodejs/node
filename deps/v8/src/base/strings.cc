// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/base/strings.h"

#include <cstdint>
#include <cstring>
#include <limits>
#include <span>

#include "src/base/platform/platform.h"

namespace v8 {
namespace base {

int VSNPrintF(std::span<char> str, const char* format, va_list args) {
  return OS::VSNPrintF(str.data(), str.size(), format, args);
}

int SNPrintF(std::span<char> str, const char* format, ...) {
  va_list args;
  va_start(args, format);
  int result = VSNPrintF(str, format, args);
  va_end(args);
  return result;
}

void StrNCpy(std::span<char> dest, const char* src, uint32_t n) {
  base::OS::StrNCpy(dest.data(), dest.size(), src, n);
}

}  // namespace base
}  // namespace v8
