// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_JS_INTRINSIC_LOWERING_H_
#define V8_COMPILER_JS_INTRINSIC_LOWERING_H_

#include "src/base/compiler-specific.h"
#include "src/compiler/common-operator.h"
#include "src/compiler/graph-reducer.h"
#include "src/globals.h"

namespace v8 {
namespace internal {

// Forward declarations.
class Callable;


namespace compiler {

// Forward declarations.
class CommonOperatorBuilder;
struct FieldAccess;
class JSOperatorBuilder;
class JSGraph;
class SimplifiedOperatorBuilder;


// Lowers certain JS-level runtime calls.
class V8_EXPORT_PRIVATE JSIntrinsicLowering final
    : public NON_EXPORTED_BASE(AdvancedReducer) {
 public:
  JSIntrinsicLowering(Editor* editor, JSGraph* jsgraph);
  ~JSIntrinsicLowering() final = default;

  const char* reducer_name() const override { return "JSIntrinsicLowering"; }

  Reduction Reduce(Node* node) final;

 private:
  Reduction ReduceCreateIterResultObject(Node* node);
  Reduction ReduceDeoptimizeNow(Node* node);
  Reduction ReduceCreateJSGeneratorObject(Node* node);
  Reduction ReduceGeneratorClose(Node* node);
  Reduction ReduceAsyncFunctionAwaitCaught(Node* node);
  Reduction ReduceAsyncFunctionAwaitUncaught(Node* node);
  Reduction ReduceAsyncFunctionEnter(Node* node);
  Reduction ReduceAsyncFunctionReject(Node* node);
  Reduction ReduceAsyncFunctionResolve(Node* node);
  Reduction ReduceAsyncGeneratorAwaitCaught(Node* node);
  Reduction ReduceAsyncGeneratorAwaitUncaught(Node* node);
  Reduction ReduceAsyncGeneratorReject(Node* node);
  Reduction ReduceAsyncGeneratorResolve(Node* node);
  Reduction ReduceAsyncGeneratorYield(Node* node);
  Reduction ReduceGeneratorGetResumeMode(Node* node);
  Reduction ReduceIsInstanceType(Node* node, InstanceType instance_type);
  Reduction ReduceIsJSReceiver(Node* node);
  Reduction ReduceIsSmi(Node* node);
  Reduction ReduceToLength(Node* node);
  Reduction ReduceToObject(Node* node);
  Reduction ReduceToString(Node* node);
  Reduction ReduceCall(Node* node);

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

  JSGraph* const jsgraph_;
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_JS_INTRINSIC_LOWERING_H_
