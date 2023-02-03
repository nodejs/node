// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/interpreter/bytecode-source-info.h"

#include <iomanip>

namespace v8 {
namespace internal {
namespace interpreter {

std::ostream& operator<<(std::ostream& os, const BytecodeSourceInfo& info) {
  if (info.is_valid()) {
    char description = info.is_statement() ? 'S' : 'E';
    os << info.source_position() << ' ' << description << '>';
  }
  return os;
}

}  // namespace interpreter
}  // namespace internal
}  // namespace v8
