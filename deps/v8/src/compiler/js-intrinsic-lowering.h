// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_JS_INTRINSIC_LOWERING_H_
#define V8_COMPILER_JS_INTRINSIC_LOWERING_H_

#include "src/compiler/common-operator.h"
#include "src/compiler/graph-reducer.h"

namespace v8 {
namespace internal {

// Forward declarations.
class Callable;


namespace compiler {

// Forward declarations.
class CommonOperatorBuilder;
class JSOperatorBuilder;
class JSGraph;
class SimplifiedOperatorBuilder;


// Lowers certain JS-level runtime calls.
class JSIntrinsicLowering final : public AdvancedReducer {
 public:
  enum DeoptimizationMode { kDeoptimizationEnabled, kDeoptimizationDisabled };

  JSIntrinsicLowering(Editor* editor, JSGraph* jsgraph,
                      DeoptimizationMode mode);
  ~JSIntrinsicLowering() final {}

  Reduction Reduce(Node* node) final;

 private:
  Reduction ReduceCreateIterResultObject(Node* node);
  Reduction ReduceDeoptimizeNow(Node* node);
  Reduction ReduceGeneratorClose(Node* node);
  Reduction ReduceGeneratorGetInputOrDebugPos(Node* node);
  Reduction ReduceGeneratorGetResumeMode(Node* node);
  Reduction ReduceIsInstanceType(Node* node, InstanceType instance_type);
  Reduction ReduceIsJSReceiver(Node* node);
  Reduction ReduceIsSmi(Node* node);
  Reduction ReduceFixedArrayGet(Node* node);
  Reduction ReduceFixedArraySet(Node* node);
  Reduction ReduceRegExpConstructResult(Node* node);
  Reduction ReduceRegExpExec(Node* node);
  Reduction ReduceRegExpFlags(Node* node);
  Reduction ReduceRegExpSource(Node* node);
  Reduction ReduceSubString(Node* node);
  Reduction ReduceToInteger(Node* node);
  Reduction ReduceToLength(Node* node);
  Reduction ReduceToNumber(Node* node);
  Reduction ReduceToObject(Node* node);
  Reduction ReduceToString(Node* node);
  Reduction ReduceCall(Node* node);
  Reduction ReduceNewObject(Node* node);
  Reduction ReduceGetSuperConstructor(Node* node);

  Reduction Change(Node* node, const Operator* op);
  Reduction Change(Node* node, const Operator* op, Node* a, Node* b);
  Reduction Change(Node* node, const Operator* op, Node* a, Node* b, Node* c);
  Reduction Change(Node* node, const Operator* op, Node* a, Node* b, Node* c,
                   Node* d);
  Reduction Change(Node* node, Callable const& callable,
                   int stack_parameter_count);

  Graph* graph() const;
  JSGraph* jsgraph() const { return jsgraph_; }
  Isolate* isolate() const;
  CommonOperatorBuilder* common() const;
  JSOperatorBuilder* javascript() const;
  SimplifiedOperatorBuilder* simplified() const;
  DeoptimizationMode mode() const { return mode_; }

  JSGraph* const jsgraph_;
  DeoptimizationMode const mode_;
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_JS_INTRINSIC_LOWERING_H_
