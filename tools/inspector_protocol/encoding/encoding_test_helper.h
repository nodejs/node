// Copyright 2019 The V8 Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is V8 specific, to make encoding_test.cc work.
// It is not rolled from the upstream project.

#ifndef V8_INSPECTOR_PROTOCOL_ENCODING_ENCODING_TEST_HELPER_H_
#define V8_INSPECTOR_PROTOCOL_ENCODING_ENCODING_TEST_HELPER_H_

#include <string>
#include <vector>

#include "src/base/logging.h"
#include "src/inspector/v8-string-conversions.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8_inspector_protocol_encoding {

std::string UTF16ToUTF8(span<uint16_t> in) {
  return v8_inspector::UTF16ToUTF8(in.data(), in.size());
}

std::vector<uint16_t> UTF8ToUTF16(span<uint8_t> in) {
  std::basic_string<uint16_t> utf16 = v8_inspector::UTF8ToUTF16(
      reinterpret_cast<const char*>(in.data()), in.size());
  return std::vector<uint16_t>(utf16.begin(), utf16.end());
}

}  // namespace v8_inspector_protocol_encoding

#endif  // V8_INSPECTOR_PROTOCOL_ENCODING_ENCODING_TEST_HELPER_H_
