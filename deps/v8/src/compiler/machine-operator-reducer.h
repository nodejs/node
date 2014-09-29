// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_MACHINE_OPERATOR_REDUCER_H_
#define V8_COMPILER_MACHINE_OPERATOR_REDUCER_H_

#include "src/compiler/common-operator.h"
#include "src/compiler/graph-reducer.h"
#include "src/compiler/machine-operator.h"

namespace v8 {
namespace internal {
namespace compiler {

// Forward declarations.
class CommonNodeCache;

// Performs constant folding and strength reduction on nodes that have
// machine operators.
class MachineOperatorReducer : public Reducer {
 public:
  explicit MachineOperatorReducer(Graph* graph);

  MachineOperatorReducer(Graph* graph, CommonNodeCache* cache);

  virtual Reduction Reduce(Node* node);

 private:
  Graph* graph_;
  CommonNodeCache* cache_;
  CommonOperatorBuilder common_;
  MachineOperatorBuilder machine_;

  Node* Int32Constant(int32_t value);
  Node* Float64Constant(volatile double value);

  Reduction ReplaceBool(bool value) { return ReplaceInt32(value ? 1 : 0); }

  Reduction ReplaceInt32(int32_t value) {
    return Replace(Int32Constant(value));
  }

  Reduction ReplaceFloat64(volatile double value) {
    return Replace(Float64Constant(value));
  }
};
}
}
}  // namespace v8::internal::compiler

#endif  // V8_COMPILER_MACHINE_OPERATOR_REDUCER_H_
