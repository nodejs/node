// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_MACHINE_OPERATOR_REDUCER_H_
#define V8_COMPILER_MACHINE_OPERATOR_REDUCER_H_

#include "src/base/compiler-specific.h"
#include "src/common/globals.h"
#include "src/compiler/graph-reducer.h"
#include "src/compiler/machine-operator.h"

namespace v8 {
namespace internal {
namespace compiler {

// Forward declarations.
class CommonOperatorBuilder;
class MachineGraph;
class Word32Adapter;
class Word64Adapter;

// Performs constant folding and strength reduction on nodes that have
// machine operators.
class V8_EXPORT_PRIVATE MachineOperatorReducer final
    : public NON_EXPORTED_BASE(AdvancedReducer) {
 public:
  explicit MachineOperatorReducer(Editor* editor, MachineGraph* mcgraph,
                                  bool allow_signalling_nan = true);
  ~MachineOperatorReducer() override;

  const char* reducer_name() const override { return "MachineOperatorReducer"; }

  Reduction Reduce(Node* node) override;

 private:
  friend class Word32Adapter;
  friend class Word64Adapter;

  Node* Float32Constant(volatile float value);
  Node* Float64Constant(volatile double value);
  Node* Int32Constant(int32_t value);
  Node* Int64Constant(int64_t value);
  Node* Uint32Constant(uint32_t value) {
    return Int32Constant(bit_cast<int32_t>(value));
  }
  Node* Uint64Constant(uint64_t value) {
    return Int64Constant(bit_cast<int64_t>(value));
  }
  Node* Float64Mul(Node* lhs, Node* rhs);
  Node* Float64PowHalf(Node* value);
  Node* Word32And(Node* lhs, Node* rhs);
  Node* Word32And(Node* lhs, uint32_t rhs) {
    return Word32And(lhs, Uint32Constant(rhs));
  }
  Node* Word32Sar(Node* lhs, uint32_t rhs);
  Node* Word32Shr(Node* lhs, uint32_t rhs);
  Node* Word32Equal(Node* lhs, Node* rhs);
  Node* Word64And(Node* lhs, Node* rhs);
  Node* Int32Add(Node* lhs, Node* rhs);
  Node* Int32Sub(Node* lhs, Node* rhs);
  Node* Int32Mul(Node* lhs, Node* rhs);
  Node* Int32Div(Node* dividend, int32_t divisor);
  Node* Uint32Div(Node* dividend, uint32_t divisor);
  Node* TruncateInt64ToInt32(Node* value);

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
  Reduction ReduceInt64Add(Node* node);
  Reduction ReduceInt32Sub(Node* node);
  Reduction ReduceInt64Sub(Node* node);
  Reduction ReduceInt64Mul(Node* node);
  Reduction ReduceInt32Div(Node* node);
  Reduction ReduceUint32Div(Node* node);
  Reduction ReduceInt32Mod(Node* node);
  Reduction ReduceUint32Mod(Node* node);
  Reduction ReduceStore(Node* node);
  Reduction ReduceProjection(size_t index, Node* node);
  const Operator* Map64To32Comparison(const Operator* op, bool sign_extended);
  Reduction ReduceWord32Comparisons(Node* node);
  Reduction ReduceWord64Comparisons(Node* node);
  Reduction ReduceWord32Shifts(Node* node);
  Reduction ReduceWord32Shl(Node* node);
  Reduction ReduceWord64Shl(Node* node);
  Reduction ReduceWord32Shr(Node* node);
  Reduction ReduceWord64Shr(Node* node);
  Reduction ReduceWord32Sar(Node* node);
  Reduction ReduceWord64Sar(Node* node);
  Reduction ReduceWord32And(Node* node);
  Reduction ReduceWord64And(Node* node);
  Reduction TryMatchWord32Ror(Node* node);
  Reduction ReduceWord32Or(Node* node);
  Reduction ReduceWord64Or(Node* node);
  Reduction ReduceWord32Xor(Node* node);
  Reduction ReduceWord64Xor(Node* node);
  Reduction ReduceWord32Equal(Node* node);
  Reduction ReduceFloat64InsertLowWord32(Node* node);
  Reduction ReduceFloat64InsertHighWord32(Node* node);
  Reduction ReduceFloat64Compare(Node* node);
  Reduction ReduceFloat64RoundDown(Node* node);
  Reduction ReduceTruncateInt64ToInt32(Node* node);
  Reduction ReduceConditional(Node* node);

  Graph* graph() const;
  MachineGraph* mcgraph() const { return mcgraph_; }
  CommonOperatorBuilder* common() const;
  MachineOperatorBuilder* machine() const;

  // These reductions can be applied to operations of different word sizes.
  // Use Word32Adapter or Word64Adapter to specialize for a particular one.
  template <typename WordNAdapter>
  Reduction ReduceWordNAnd(Node* node);
  template <typename WordNAdapter>
  Reduction ReduceWordNOr(Node* node);
  template <typename WordNAdapter>
  Reduction ReduceWordNXor(Node* node);

  // Helper for ReduceConditional. Does not perform the actual reduction; just
  // returns a new Node that could be used as the input to the condition.
  template <typename WordNAdapter>
  base::Optional<Node*> ReduceConditionalN(Node* node);

  // Helper for finding a reduced equality condition. Does not perform the
  // actual reduction; just returns a new pair that could be compared for the
  // same outcome.
  template <typename WordNAdapter>
  base::Optional<std::pair<Node*, uint32_t>> ReduceWord32EqualForConstantRhs(
      Node* lhs, uint32_t rhs);

  MachineGraph* mcgraph_;
  bool allow_signalling_nan_;
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_MACHINE_OPERATOR_REDUCER_H_
