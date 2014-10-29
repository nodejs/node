// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_MACHINE_OPERATOR_REDUCER_H_
#define V8_COMPILER_MACHINE_OPERATOR_REDUCER_H_

#include "src/compiler/graph-reducer.h"
#include "src/compiler/machine-operator.h"

namespace v8 {
namespace internal {
namespace compiler {

// Forward declarations.
class CommonOperatorBuilder;
class JSGraph;


// Performs constant folding and strength reduction on nodes that have
// machine operators.
class MachineOperatorReducer FINAL : public Reducer {
 public:
  explicit MachineOperatorReducer(JSGraph* jsgraph);
  ~MachineOperatorReducer();

  virtual Reduction Reduce(Node* node) OVERRIDE;

 private:
  Node* Float32Constant(volatile float value);
  Node* Float64Constant(volatile double value);
  Node* Int32Constant(int32_t value);
  Node* Int64Constant(int64_t value);
  Node* Uint32Constant(uint32_t value) {
    return Int32Constant(bit_cast<uint32_t>(value));
  }

  Reduction ReplaceBool(bool value) { return ReplaceInt32(value ? 1 : 0); }
  Reduction ReplaceFloat32(volatile float value) {
    return Replace(Float32Constant(value));
  }
  Reduction ReplaceFloat64(volatile double value) {
    return Replace(Float64Constant(value));
  }
  Reduction ReplaceInt32(int32_t value) {
    return Replace(Int32Constant(value));
  }
  Reduction ReplaceInt64(int64_t value) {
    return Replace(Int64Constant(value));
  }

  Reduction ReduceProjection(size_t index, Node* node);

  Graph* graph() const;
  JSGraph* jsgraph() const { return jsgraph_; }
  CommonOperatorBuilder* common() const;
  MachineOperatorBuilder* machine() const;

  JSGraph* jsgraph_;
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_MACHINE_OPERATOR_REDUCER_H_
