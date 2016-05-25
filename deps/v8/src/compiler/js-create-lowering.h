// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_JS_CREATE_LOWERING_H_
#define V8_COMPILER_JS_CREATE_LOWERING_H_

#include "src/compiler/graph-reducer.h"

namespace v8 {
namespace internal {

// Forward declarations.
class AllocationSiteUsageContext;
class CompilationDependencies;
class Factory;


namespace compiler {

// Forward declarations.
class CommonOperatorBuilder;
class JSGraph;
class JSOperatorBuilder;
class MachineOperatorBuilder;
class SimplifiedOperatorBuilder;


// Lowers JSCreate-level operators to fast (inline) allocations.
class JSCreateLowering final : public AdvancedReducer {
 public:
  JSCreateLowering(Editor* editor, CompilationDependencies* dependencies,
                   JSGraph* jsgraph, MaybeHandle<LiteralsArray> literals_array,
                   Zone* zone)
      : AdvancedReducer(editor),
        dependencies_(dependencies),
        jsgraph_(jsgraph),
        literals_array_(literals_array),
        zone_(zone) {}
  ~JSCreateLowering() final {}

  Reduction Reduce(Node* node) final;

 private:
  Reduction ReduceJSCreate(Node* node);
  Reduction ReduceJSCreateArguments(Node* node);
  Reduction ReduceJSCreateArray(Node* node);
  Reduction ReduceJSCreateIterResultObject(Node* node);
  Reduction ReduceJSCreateLiteral(Node* node);
  Reduction ReduceJSCreateFunctionContext(Node* node);
  Reduction ReduceJSCreateWithContext(Node* node);
  Reduction ReduceJSCreateCatchContext(Node* node);
  Reduction ReduceJSCreateBlockContext(Node* node);
  Reduction ReduceNewArray(Node* node, Node* length, int capacity,
                           Handle<AllocationSite> site);

  Node* AllocateArguments(Node* effect, Node* control, Node* frame_state);
  Node* AllocateRestArguments(Node* effect, Node* control, Node* frame_state,
                              int start_index);
  Node* AllocateAliasedArguments(Node* effect, Node* control, Node* frame_state,
                                 Node* context, Handle<SharedFunctionInfo>,
                                 bool* has_aliased_arguments);
  Node* AllocateElements(Node* effect, Node* control,
                         ElementsKind elements_kind, int capacity,
                         PretenureFlag pretenure);
  Node* AllocateFastLiteral(Node* effect, Node* control,
                            Handle<JSObject> boilerplate,
                            AllocationSiteUsageContext* site_context);
  Node* AllocateFastLiteralElements(Node* effect, Node* control,
                                    Handle<JSObject> boilerplate,
                                    PretenureFlag pretenure,
                                    AllocationSiteUsageContext* site_context);

  // Infers the LiteralsArray to use for a given {node}.
  MaybeHandle<LiteralsArray> GetSpecializationLiterals(Node* node);

  Factory* factory() const;
  Graph* graph() const;
  JSGraph* jsgraph() const { return jsgraph_; }
  Isolate* isolate() const;
  JSOperatorBuilder* javascript() const;
  CommonOperatorBuilder* common() const;
  SimplifiedOperatorBuilder* simplified() const;
  MachineOperatorBuilder* machine() const;
  CompilationDependencies* dependencies() const { return dependencies_; }
  Zone* zone() const { return zone_; }

  CompilationDependencies* const dependencies_;
  JSGraph* const jsgraph_;
  MaybeHandle<LiteralsArray> const literals_array_;
  Zone* const zone_;
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_JS_CREATE_LOWERING_H_
