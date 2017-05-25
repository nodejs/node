// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_JS_CALL_REDUCER_H_
#define V8_COMPILER_JS_CALL_REDUCER_H_

#include "src/base/flags.h"
#include "src/compiler/graph-reducer.h"

namespace v8 {
namespace internal {

// Forward declarations.
class CompilationDependencies;
class Factory;

namespace compiler {

// Forward declarations.
class CommonOperatorBuilder;
class JSGraph;
class JSOperatorBuilder;
class SimplifiedOperatorBuilder;

// Performs strength reduction on {JSConstruct} and {JSCall} nodes,
// which might allow inlining or other optimizations to be performed afterwards.
class JSCallReducer final : public AdvancedReducer {
 public:
  // Flags that control the mode of operation.
  enum Flag {
    kNoFlags = 0u,
    kDeoptimizationEnabled = 1u << 0,
  };
  typedef base::Flags<Flag> Flags;

  JSCallReducer(Editor* editor, JSGraph* jsgraph, Flags flags,
                Handle<Context> native_context,
                CompilationDependencies* dependencies)
      : AdvancedReducer(editor),
        jsgraph_(jsgraph),
        flags_(flags),
        native_context_(native_context),
        dependencies_(dependencies) {}

  Reduction Reduce(Node* node) final;

 private:
  Reduction ReduceArrayConstructor(Node* node);
  Reduction ReduceCallApiFunction(
      Node* node, Node* target,
      Handle<FunctionTemplateInfo> function_template_info);
  Reduction ReduceNumberConstructor(Node* node);
  Reduction ReduceFunctionPrototypeApply(Node* node);
  Reduction ReduceFunctionPrototypeCall(Node* node);
  Reduction ReduceFunctionPrototypeHasInstance(Node* node);
  Reduction ReduceObjectPrototypeGetProto(Node* node);
  Reduction ReduceSpreadCall(Node* node, int arity);
  Reduction ReduceJSConstruct(Node* node);
  Reduction ReduceJSConstructWithSpread(Node* node);
  Reduction ReduceJSCall(Node* node);
  Reduction ReduceJSCallWithSpread(Node* node);

  enum HolderLookup { kHolderNotFound, kHolderIsReceiver, kHolderFound };

  HolderLookup LookupHolder(Handle<JSObject> object,
                            Handle<FunctionTemplateInfo> function_template_info,
                            Handle<JSObject>* holder);

  Graph* graph() const;
  Flags flags() const { return flags_; }
  JSGraph* jsgraph() const { return jsgraph_; }
  Isolate* isolate() const;
  Factory* factory() const;
  Handle<Context> native_context() const { return native_context_; }
  CommonOperatorBuilder* common() const;
  JSOperatorBuilder* javascript() const;
  SimplifiedOperatorBuilder* simplified() const;
  CompilationDependencies* dependencies() const { return dependencies_; }

  JSGraph* const jsgraph_;
  Flags const flags_;
  Handle<Context> const native_context_;
  CompilationDependencies* const dependencies_;
};

DEFINE_OPERATORS_FOR_FLAGS(JSCallReducer::Flags)

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_JS_CALL_REDUCER_H_
