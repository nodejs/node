// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_COMMON_OPERATOR_REDUCER_H_
#define V8_COMPILER_COMMON_OPERATOR_REDUCER_H_

#include "src/compiler/graph-reducer.h"

namespace v8 {
namespace internal {
namespace compiler {

// Performs strength reduction on nodes that have common operators.
class CommonOperatorReducer FINAL : public Reducer {
 public:
  CommonOperatorReducer() {}
  ~CommonOperatorReducer() FINAL {}

  Reduction Reduce(Node* node) FINAL;
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_COMMON_OPERATOR_REDUCER_H_
