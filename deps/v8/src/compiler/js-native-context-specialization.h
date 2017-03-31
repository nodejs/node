// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_JS_NATIVE_CONTEXT_SPECIALIZATION_H_
#define V8_COMPILER_JS_NATIVE_CONTEXT_SPECIALIZATION_H_

#include "src/base/flags.h"
#include "src/compiler/graph-reducer.h"
#include "src/deoptimize-reason.h"
#include "src/feedback-vector.h"

namespace v8 {
namespace internal {

// Forward declarations.
class CompilationDependencies;
class Factory;

namespace compiler {

// Forward declarations.
enum class AccessMode;
class CommonOperatorBuilder;
class ElementAccessInfo;
class JSGraph;
class JSOperatorBuilder;
class MachineOperatorBuilder;
class PropertyAccessInfo;
class SimplifiedOperatorBuilder;
class TypeCache;

// Specializes a given JSGraph to a given native context, potentially constant
// folding some {LoadGlobal} nodes or strength reducing some {StoreGlobal}
// nodes.  And also specializes {LoadNamed} and {StoreNamed} nodes according
// to type feedback (if available).
class JSNativeContextSpecialization final : public AdvancedReducer {
 public:
  // Flags that control the mode of operation.
  enum Flag {
    kNoFlags = 0u,
    kAccessorInliningEnabled = 1u << 0,
    kBailoutOnUninitialized = 1u << 1,
    kDeoptimizationEnabled = 1u << 2,
  };
  typedef base::Flags<Flag> Flags;

  JSNativeContextSpecialization(Editor* editor, JSGraph* jsgraph, Flags flags,
                                Handle<Context> native_context,
                                CompilationDependencies* dependencies,
                                Zone* zone);

  Reduction Reduce(Node* node) final;

 private:
  Reduction ReduceJSInstanceOf(Node* node);
  Reduction ReduceJSOrdinaryHasInstance(Node* node);
  Reduction ReduceJSLoadContext(Node* node);
  Reduction ReduceJSLoadNamed(Node* node);
  Reduction ReduceJSStoreNamed(Node* node);
  Reduction ReduceJSLoadProperty(Node* node);
  Reduction ReduceJSStoreProperty(Node* node);
  Reduction ReduceJSStoreDataPropertyInLiteral(Node* node);

  Reduction ReduceElementAccess(Node* node, Node* index, Node* value,
                                MapHandleList const& receiver_maps,
                                AccessMode access_mode,
                                LanguageMode language_mode,
                                KeyedAccessStoreMode store_mode);
  template <typename KeyedICNexus>
  Reduction ReduceKeyedAccess(Node* node, Node* index, Node* value,
                              KeyedICNexus const& nexus, AccessMode access_mode,
                              LanguageMode language_mode,
                              KeyedAccessStoreMode store_mode);
  Reduction ReduceNamedAccessFromNexus(Node* node, Node* value,
                                       FeedbackNexus const& nexus,
                                       Handle<Name> name,
                                       AccessMode access_mode,
                                       LanguageMode language_mode);
  Reduction ReduceNamedAccess(Node* node, Node* value,
                              MapHandleList const& receiver_maps,
                              Handle<Name> name, AccessMode access_mode,
                              LanguageMode language_mode,
                              Handle<FeedbackVector> vector,
                              FeedbackVectorSlot slot, Node* index = nullptr);

  Reduction ReduceSoftDeoptimize(Node* node, DeoptimizeReason reason);

  // A triple of nodes that represents a continuation.
  class ValueEffectControl final {
   public:
    ValueEffectControl(Node* value, Node* effect, Node* control)
        : value_(value), effect_(effect), control_(control) {}

    Node* value() const { return value_; }
    Node* effect() const { return effect_; }
    Node* control() const { return control_; }

   private:
    Node* const value_;
    Node* const effect_;
    Node* const control_;
  };

  // Construct the appropriate subgraph for property access.
  ValueEffectControl BuildPropertyAccess(
      Node* receiver, Node* value, Node* context, Node* frame_state,
      Node* effect, Node* control, Handle<Name> name,
      PropertyAccessInfo const& access_info, AccessMode access_mode,
      LanguageMode language_mode, Handle<FeedbackVector> vector,
      FeedbackVectorSlot slot);

  // Construct the appropriate subgraph for element access.
  ValueEffectControl BuildElementAccess(Node* receiver, Node* index,
                                        Node* value, Node* effect,
                                        Node* control,
                                        ElementAccessInfo const& access_info,
                                        AccessMode access_mode,
                                        KeyedAccessStoreMode store_mode);

  // Construct an appropriate heap object check.
  Node* BuildCheckHeapObject(Node* receiver, Node** effect, Node* control);

  // Construct an appropriate map check.
  Node* BuildCheckMaps(Node* receiver, Node* effect, Node* control,
                       std::vector<Handle<Map>> const& maps);

  // Adds stability dependencies on all prototypes of every class in
  // {receiver_type} up to (and including) the {holder}.
  void AssumePrototypesStable(std::vector<Handle<Map>> const& receiver_maps,
                              Handle<JSObject> holder);

  // Checks if we can turn the hole into undefined when loading an element
  // from an object with one of the {receiver_maps}; sets up appropriate
  // code dependencies and might use the array protector cell.
  bool CanTreatHoleAsUndefined(std::vector<Handle<Map>> const& receiver_maps);

  // Extract receiver maps from {nexus} and filter based on {receiver} if
  // possible.
  bool ExtractReceiverMaps(Node* receiver, Node* effect,
                           FeedbackNexus const& nexus,
                           MapHandleList* receiver_maps);

  // Try to infer a map for the given {receiver} at the current {effect}.
  // If a map is returned then you can be sure that the {receiver} definitely
  // has the returned map at this point in the program (identified by {effect}).
  MaybeHandle<Map> InferReceiverMap(Node* receiver, Node* effect);
  // Try to infer a root map for the {receiver} independent of the current
  // program location.
  MaybeHandle<Map> InferReceiverRootMap(Node* receiver);

  ValueEffectControl InlineApiCall(
      Node* receiver, Node* context, Node* target, Node* frame_state,
      Node* parameter, Node* effect, Node* control,
      Handle<SharedFunctionInfo> shared_info,
      Handle<FunctionTemplateInfo> function_template_info);

  Graph* graph() const;
  JSGraph* jsgraph() const { return jsgraph_; }
  Isolate* isolate() const;
  Factory* factory() const;
  CommonOperatorBuilder* common() const;
  JSOperatorBuilder* javascript() const;
  SimplifiedOperatorBuilder* simplified() const;
  MachineOperatorBuilder* machine() const;
  Flags flags() const { return flags_; }
  Handle<Context> native_context() const { return native_context_; }
  CompilationDependencies* dependencies() const { return dependencies_; }
  Zone* zone() const { return zone_; }

  JSGraph* const jsgraph_;
  Flags const flags_;
  Handle<Context> native_context_;
  CompilationDependencies* const dependencies_;
  Zone* const zone_;
  TypeCache const& type_cache_;

  DISALLOW_COPY_AND_ASSIGN(JSNativeContextSpecialization);
};

DEFINE_OPERATORS_FOR_FLAGS(JSNativeContextSpecialization::Flags)

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_JS_NATIVE_CONTEXT_SPECIALIZATION_H_
