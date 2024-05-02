// Copyright 2019 The V8 Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is V8 specific. It's not rolled from the upstream project.

#ifndef V8_INSPECTOR_PROTOCOL_CRDTP_TEST_PLATFORM_H_
#define V8_INSPECTOR_PROTOCOL_CRDTP_TEST_PLATFORM_H_

#include <string>
#include <vector>

#include "span.h"
#include "src/base/logging.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8_crdtp {

std::string UTF16ToUTF8(span<uint16_t> in);

std::vector<uint16_t> UTF8ToUTF16(span<uint8_t> in);

}  // namespace v8_crdtp

#endif  // V8_INSPECTOR_PROTOCOL_CRDTP_TEST_PLATFORM_H_
