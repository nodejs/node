// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/node-matchers.h"

namespace v8 {
namespace internal {
namespace compiler {

bool NodeMatcher::IsComparison() const {
  return IrOpcode::IsComparisonOpcode(opcode());
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
