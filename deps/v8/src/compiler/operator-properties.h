// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_OPERATOR_PROPERTIES_H_
#define V8_COMPILER_OPERATOR_PROPERTIES_H_

#include "src/base/macros.h"
#include "src/common/globals.h"

namespace v8 {
namespace internal {
namespace compiler {

// Forward declarations.
class Operator;

class V8_EXPORT_PRIVATE OperatorProperties final {
 public:
  OperatorProperties(const OperatorProperties&) = delete;
  OperatorProperties& operator=(const OperatorProperties&) = delete;

  static bool HasContextInput(const Operator* op);
  static int GetContextInputCount(const Operator* op) {
    return HasContextInput(op) ? 1 : 0;
  }

  static bool NeedsExactContext(const Operator* op);

  static bool HasFrameStateInput(const Operator* op);
  static int GetFrameStateInputCount(const Operator* op) {
    return HasFrameStateInput(op) ? 1 : 0;
  }

  static int GetTotalInputCount(const Operator* op);

  static bool IsBasicBlockBegin(const Operator* op);
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_OPERATOR_PROPERTIES_H_
