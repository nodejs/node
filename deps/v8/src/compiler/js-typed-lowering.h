// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_JS_TYPED_LOWERING_H_
#define V8_COMPILER_JS_TYPED_LOWERING_H_

#include "src/base/flags.h"
#include "src/compiler/graph-reducer.h"
#include "src/compiler/opcodes.h"

namespace v8 {
namespace internal {

// Forward declarations.
class CompilationDependencies;
class Factory;
class TypeCache;


namespace compiler {

// Forward declarations.
class CommonOperatorBuilder;
class JSGraph;
class JSOperatorBuilder;
class MachineOperatorBuilder;
class SimplifiedOperatorBuilder;


// Lowers JS-level operators to simplified operators based on types.
class JSTypedLowering final : public AdvancedReducer {
 public:
  // Flags that control the mode of operation.
  enum Flag {
    kNoFlags = 0u,
    kDeoptimizationEnabled = 1u << 0,
    kDisableBinaryOpReduction = 1u << 1,
  };
  typedef base::Flags<Flag> Flags;

  JSTypedLowering(Editor* editor, CompilationDependencies* dependencies,
                  Flags flags, JSGraph* jsgraph, Zone* zone);
  ~JSTypedLowering() final {}

  Reduction Reduce(Node* node) final;

 private:
  friend class JSBinopReduction;

  Reduction ReduceJSAdd(Node* node);
  Reduction ReduceJSModulus(Node* node);
  Reduction ReduceJSBitwiseOr(Node* node);
  Reduction ReduceJSMultiply(Node* node);
  Reduction ReduceJSComparison(Node* node);
  Reduction ReduceJSLoadNamed(Node* node);
  Reduction ReduceJSLoadProperty(Node* node);
  Reduction ReduceJSStoreProperty(Node* node);
  Reduction ReduceJSInstanceOf(Node* node);
  Reduction ReduceJSLoadContext(Node* node);
  Reduction ReduceJSStoreContext(Node* node);
  Reduction ReduceJSEqual(Node* node, bool invert);
  Reduction ReduceJSStrictEqual(Node* node, bool invert);
  Reduction ReduceJSToBoolean(Node* node);
  Reduction ReduceJSToNumberInput(Node* input);
  Reduction ReduceJSToNumber(Node* node);
  Reduction ReduceJSToStringInput(Node* input);
  Reduction ReduceJSToString(Node* node);
  Reduction ReduceJSToObject(Node* node);
  Reduction ReduceJSConvertReceiver(Node* node);
  Reduction ReduceJSCallConstruct(Node* node);
  Reduction ReduceJSCallFunction(Node* node);
  Reduction ReduceJSForInDone(Node* node);
  Reduction ReduceJSForInNext(Node* node);
  Reduction ReduceJSForInStep(Node* node);
  Reduction ReduceSelect(Node* node);
  Reduction ReduceNumberBinop(Node* node, const Operator* numberOp);
  Reduction ReduceInt32Binop(Node* node, const Operator* intOp);
  Reduction ReduceUI32Shift(Node* node, Signedness left_signedness,
                            const Operator* shift_op);

  Node* Word32Shl(Node* const lhs, int32_t const rhs);

  Factory* factory() const;
  Graph* graph() const;
  JSGraph* jsgraph() const { return jsgraph_; }
  Isolate* isolate() const;
  JSOperatorBuilder* javascript() const;
  CommonOperatorBuilder* common() const;
  SimplifiedOperatorBuilder* simplified() const;
  MachineOperatorBuilder* machine() const;
  CompilationDependencies* dependencies() const;
  Flags flags() const { return flags_; }

  CompilationDependencies* dependencies_;
  Flags flags_;
  JSGraph* jsgraph_;
  Type* shifted_int32_ranges_[4];
  Type* const true_type_;
  Type* const false_type_;
  Type* const the_hole_type_;
  TypeCache const& type_cache_;
};

DEFINE_OPERATORS_FOR_FLAGS(JSTypedLowering::Flags)

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_JS_TYPED_LOWERING_H_
