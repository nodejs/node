// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "str_util.h"

#include <cstring>

namespace inspector_protocol {
bool StrEq(span<uint8_t> left, span<uint8_t> right) {
  return left.size() == right.size() &&
         0 == memcmp(left.data(), right.data(), left.size());
}

bool StrEq(span<uint8_t> left, const char* null_terminated_right) {
  return static_cast<std::size_t>(left.size()) ==
             strlen(null_terminated_right) &&
         0 == memcmp(left.data(), null_terminated_right, left.size());
}
}  // namespace inspector_protocol
