// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef INSPECTOR_PROTOCOL_ENCODING_STR_UTIL_H_
#define INSPECTOR_PROTOCOL_ENCODING_STR_UTIL_H_

#include <cstdint>

#include "span.h"

namespace inspector_protocol {
// Returns true iff |left| and right have the same contents, byte for byte.
bool StrEq(span<uint8_t> left, span<uint8_t> right);
bool StrEq(span<uint8_t> left, const char* null_terminated_right);
}  // namespace inspector_protocol
#endif  // INSPECTOR_PROTOCOL_ENCODING_STR_UTIL_H_
