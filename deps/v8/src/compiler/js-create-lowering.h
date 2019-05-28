// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_JS_CREATE_LOWERING_H_
#define V8_COMPILER_JS_CREATE_LOWERING_H_

#include "src/base/compiler-specific.h"
#include "src/compiler/graph-reducer.h"
#include "src/globals.h"

namespace v8 {
namespace internal {

// Forward declarations.
class AllocationSiteUsageContext;
class Factory;
class JSRegExp;

namespace compiler {

// Forward declarations.
class CommonOperatorBuilder;
class CompilationDependencies;
class JSGraph;
class JSOperatorBuilder;
class MachineOperatorBuilder;
class SimplifiedOperatorBuilder;
class SlackTrackingPrediction;

// Lowers JSCreate-level operators to fast (inline) allocations.
class V8_EXPORT_PRIVATE JSCreateLowering final
    : public NON_EXPORTED_BASE(AdvancedReducer) {
 public:
  JSCreateLowering(Editor* editor, CompilationDependencies* dependencies,
                   JSGraph* jsgraph, JSHeapBroker* broker, Zone* zone)
      : AdvancedReducer(editor),
        dependencies_(dependencies),
        jsgraph_(jsgraph),
        broker_(broker),
        zone_(zone) {}
  ~JSCreateLowering() final = default;

  const char* reducer_name() const override { return "JSCreateLowering"; }

  Reduction Reduce(Node* node) final;

 private:
  Reduction ReduceJSCreate(Node* node);
  Reduction ReduceJSCreateArguments(Node* node);
  Reduction ReduceJSCreateArray(Node* node);
  Reduction ReduceJSCreateArrayIterator(Node* node);
  Reduction ReduceJSCreateAsyncFunctionObject(Node* node);
  Reduction ReduceJSCreateCollectionIterator(Node* node);
  Reduction ReduceJSCreateBoundFunction(Node* node);
  Reduction ReduceJSCreateClosure(Node* node);
  Reduction ReduceJSCreateIterResultObject(Node* node);
  Reduction ReduceJSCreateStringIterator(Node* node);
  Reduction ReduceJSCreateKeyValueArray(Node* node);
  Reduction ReduceJSCreatePromise(Node* node);
  Reduction ReduceJSCreateLiteralArrayOrObject(Node* node);
  Reduction ReduceJSCreateEmptyLiteralObject(Node* node);
  Reduction ReduceJSCreateEmptyLiteralArray(Node* node);
  Reduction ReduceJSCreateLiteralRegExp(Node* node);
  Reduction ReduceJSCreateFunctionContext(Node* node);
  Reduction ReduceJSCreateWithContext(Node* node);
  Reduction ReduceJSCreateCatchContext(Node* node);
  Reduction ReduceJSCreateBlockContext(Node* node);
  Reduction ReduceJSCreateGeneratorObject(Node* node);
  Reduction ReduceNewArray(
      Node* node, Node* length, MapRef initial_map, ElementsKind elements_kind,
      AllocationType allocation,
      const SlackTrackingPrediction& slack_tracking_prediction);
  Reduction ReduceNewArray(
      Node* node, Node* length, int capacity, MapRef initial_map,
      ElementsKind elements_kind, AllocationType allocation,
      const SlackTrackingPrediction& slack_tracking_prediction);
  Reduction ReduceNewArray(
      Node* node, std::vector<Node*> values, MapRef initial_map,
      ElementsKind elements_kind, AllocationType allocation,
      const SlackTrackingPrediction& slack_tracking_prediction);
  Reduction ReduceJSCreateObject(Node* node);

  Node* AllocateArguments(Node* effect, Node* control, Node* frame_state);
  Node* AllocateRestArguments(Node* effect, Node* control, Node* frame_state,
                              int start_index);
  Node* AllocateAliasedArguments(Node* effect, Node* control, Node* frame_state,
                                 Node* context,
                                 const SharedFunctionInfoRef& shared,
                                 bool* has_aliased_arguments);
  Node* AllocateAliasedArguments(Node* effect, Node* control, Node* context,
                                 Node* arguments_frame, Node* arguments_length,
                                 const SharedFunctionInfoRef& shared,
                                 bool* has_aliased_arguments);
  Node* AllocateElements(Node* effect, Node* control,
                         ElementsKind elements_kind, int capacity,
                         AllocationType allocation);
  Node* AllocateElements(Node* effect, Node* control,
                         ElementsKind elements_kind, Node* capacity_and_length);
  Node* AllocateElements(Node* effect, Node* control,
                         ElementsKind elements_kind,
                         std::vector<Node*> const& values,
                         AllocationType allocation);
  Node* AllocateFastLiteral(Node* effect, Node* control,
                            JSObjectRef boilerplate, AllocationType allocation);
  Node* AllocateFastLiteralElements(Node* effect, Node* control,
                                    JSObjectRef boilerplate,
                                    AllocationType allocation);
  Node* AllocateLiteralRegExp(Node* effect, Node* control,
                              JSRegExpRef boilerplate);

  Factory* factory() const;
  Graph* graph() const;
  JSGraph* jsgraph() const { return jsgraph_; }
  NativeContextRef native_context() const;
  CommonOperatorBuilder* common() const;
  SimplifiedOperatorBuilder* simplified() const;
  CompilationDependencies* dependencies() const { return dependencies_; }
  JSHeapBroker* broker() const { return broker_; }
  Zone* zone() const { return zone_; }

  CompilationDependencies* const dependencies_;
  JSGraph* const jsgraph_;
  JSHeapBroker* const broker_;
  Zone* const zone_;
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_JS_CREATE_LOWERING_H_
