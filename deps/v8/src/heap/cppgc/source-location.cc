// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/cppgc/source-location.h"

namespace cppgc {

std::string SourceLocation::ToString() const {
  if (!file_) {
    return {};
  }
  return std::string(function_) + "@" + file_ + ":" + std::to_string(line_);
}

}  // namespace cppgc
