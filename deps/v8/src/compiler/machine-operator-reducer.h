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
class MachineOperatorReducer final : public Reducer {
 public:
  explicit MachineOperatorReducer(JSGraph* jsgraph);
  ~MachineOperatorReducer();

  Reduction Reduce(Node* node) override;

 private:
  Node* Float32Constant(volatile float value);
  Node* Float64Constant(volatile double value);
  Node* Int32Constant(int32_t value);
  Node* Int64Constant(int64_t value);
  Node* Uint32Constant(uint32_t value) {
    return Int32Constant(bit_cast<uint32_t>(value));
  }
  Node* Word32And(Node* lhs, Node* rhs);
  Node* Word32And(Node* lhs, uint32_t rhs) {
    return Word32And(lhs, Uint32Constant(rhs));
  }
  Node* Word32Sar(Node* lhs, uint32_t rhs);
  Node* Word32Shr(Node* lhs, uint32_t rhs);
  Node* Word32Equal(Node* lhs, Node* rhs);
  Node* Int32Add(Node* lhs, Node* rhs);
  Node* Int32Sub(Node* lhs, Node* rhs);
  Node* Int32Mul(Node* lhs, Node* rhs);
  Node* Int32Div(Node* dividend, int32_t divisor);
  Node* Uint32Div(Node* dividend, uint32_t divisor);

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
  Reduction ReplaceUint32(uint32_t value) {
    return Replace(Uint32Constant(value));
  }
  Reduction ReplaceInt64(int64_t value) {
    return Replace(Int64Constant(value));
  }

  Reduction ReduceInt32Add(Node* node);
  Reduction ReduceInt32Sub(Node* node);
  Reduction ReduceInt32Div(Node* node);
  Reduction ReduceUint32Div(Node* node);
  Reduction ReduceInt32Mod(Node* node);
  Reduction ReduceUint32Mod(Node* node);
  Reduction ReduceTruncateFloat64ToInt32(Node* node);
  Reduction ReduceStore(Node* node);
  Reduction ReduceProjection(size_t index, Node* node);
  Reduction ReduceWord32Shifts(Node* node);
  Reduction ReduceWord32Shl(Node* node);
  Reduction ReduceWord32Sar(Node* node);
  Reduction ReduceWord32And(Node* node);
  Reduction ReduceWord32Or(Node* node);
  Reduction ReduceFloat64InsertLowWord32(Node* node);
  Reduction ReduceFloat64InsertHighWord32(Node* node);
  Reduction ReduceFloat64Compare(Node* node);

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
