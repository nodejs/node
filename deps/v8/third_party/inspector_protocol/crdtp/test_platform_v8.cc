// Copyright 2019 The V8 Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is V8 specific. It's not rolled from the upstream project.

#include "test_platform.h"

#include "src/inspector/v8-string-conversions.h"

namespace v8_crdtp {

std::string UTF16ToUTF8(span<uint16_t> in) {
  return v8_inspector::UTF16ToUTF8(in.data(), in.size());
}

std::vector<uint16_t> UTF8ToUTF16(span<uint8_t> in) {
  std::basic_string<uint16_t> utf16 = v8_inspector::UTF8ToUTF16(
      reinterpret_cast<const char*>(in.data()), in.size());
  return std::vector<uint16_t>(utf16.begin(), utf16.end());
}

}  // namespace v8_crdtp
