// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_JS_TYPED_LOWERING_H_
#define V8_COMPILER_JS_TYPED_LOWERING_H_

#include "src/compiler/graph-reducer.h"
#include "src/compiler/opcodes.h"
#include "src/compiler/simplified-operator.h"

namespace v8 {
namespace internal {

// Forward declarations.
class Factory;


namespace compiler {

// Forward declarations.
class CommonOperatorBuilder;
class JSGraph;
class JSOperatorBuilder;
class MachineOperatorBuilder;


// Lowers JS-level operators to simplified operators based on types.
class JSTypedLowering final : public Reducer {
 public:
  JSTypedLowering(JSGraph* jsgraph, Zone* zone);
  ~JSTypedLowering() final {}

  Reduction Reduce(Node* node) final;

 private:
  friend class JSBinopReduction;

  Reduction ReplaceEagerly(Node* old, Node* node);
  Reduction ReduceJSAdd(Node* node);
  Reduction ReduceJSBitwiseOr(Node* node);
  Reduction ReduceJSMultiply(Node* node);
  Reduction ReduceJSComparison(Node* node);
  Reduction ReduceJSLoadNamed(Node* node);
  Reduction ReduceJSLoadProperty(Node* node);
  Reduction ReduceJSStoreProperty(Node* node);
  Reduction ReduceJSLoadContext(Node* node);
  Reduction ReduceJSStoreContext(Node* node);
  Reduction ReduceJSEqual(Node* node, bool invert);
  Reduction ReduceJSStrictEqual(Node* node, bool invert);
  Reduction ReduceJSUnaryNot(Node* node);
  Reduction ReduceJSToBoolean(Node* node);
  Reduction ReduceJSToNumberInput(Node* input);
  Reduction ReduceJSToNumber(Node* node);
  Reduction ReduceJSToStringInput(Node* input);
  Reduction ReduceJSToString(Node* node);
  Reduction ReduceJSCreateClosure(Node* node);
  Reduction ReduceJSCreateLiteralArray(Node* node);
  Reduction ReduceJSCreateLiteralObject(Node* node);
  Reduction ReduceJSCreateWithContext(Node* node);
  Reduction ReduceJSCreateBlockContext(Node* node);
  Reduction ReduceNumberBinop(Node* node, const Operator* numberOp);
  Reduction ReduceInt32Binop(Node* node, const Operator* intOp);
  Reduction ReduceUI32Shift(Node* node, Signedness left_signedness,
                            const Operator* shift_op);

  Node* ConvertPrimitiveToNumber(Node* input);

  Node* Word32Shl(Node* const lhs, int32_t const rhs);

  Factory* factory() const;
  Graph* graph() const;
  JSGraph* jsgraph() const { return jsgraph_; }
  JSOperatorBuilder* javascript() const;
  CommonOperatorBuilder* common() const;
  SimplifiedOperatorBuilder* simplified() { return &simplified_; }
  MachineOperatorBuilder* machine() const;

  // Limits up to which context allocations are inlined.
  static const int kBlockContextAllocationLimit = 16;

  JSGraph* jsgraph_;
  SimplifiedOperatorBuilder simplified_;
  Type* zero_range_;
  Type* one_range_;
  Type* zero_thirtyone_range_;
  Type* shifted_int32_ranges_[4];
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_JS_TYPED_LOWERING_H_
