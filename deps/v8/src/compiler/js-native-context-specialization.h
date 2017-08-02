// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_JS_NATIVE_CONTEXT_SPECIALIZATION_H_
#define V8_COMPILER_JS_NATIVE_CONTEXT_SPECIALIZATION_H_

#include "src/base/flags.h"
#include "src/compiler/graph-reducer.h"
#include "src/deoptimize-reason.h"
#include "src/objects/map.h"

namespace v8 {
namespace internal {

// Forward declarations.
class CompilationDependencies;
class Factory;
class FeedbackNexus;

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
    kBailoutOnUninitialized = 1u << 1
  };
  typedef base::Flags<Flag> Flags;

  JSNativeContextSpecialization(Editor* editor, JSGraph* jsgraph, Flags flags,
                                Handle<Context> native_context,
                                CompilationDependencies* dependencies,
                                Zone* zone);

  Reduction Reduce(Node* node) final;

 private:
  Reduction ReduceJSAdd(Node* node);
  Reduction ReduceJSGetSuperConstructor(Node* node);
  Reduction ReduceJSInstanceOf(Node* node);
  Reduction ReduceJSOrdinaryHasInstance(Node* node);
  Reduction ReduceJSLoadContext(Node* node);
  Reduction ReduceJSLoadGlobal(Node* node);
  Reduction ReduceJSStoreGlobal(Node* node);
  Reduction ReduceJSLoadNamed(Node* node);
  Reduction ReduceJSStoreNamed(Node* node);
  Reduction ReduceJSLoadProperty(Node* node);
  Reduction ReduceJSStoreProperty(Node* node);
  Reduction ReduceJSStoreNamedOwn(Node* node);
  Reduction ReduceJSStoreDataPropertyInLiteral(Node* node);

  Reduction ReduceElementAccess(Node* node, Node* index, Node* value,
                                MapHandles const& receiver_maps,
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
                              MapHandles const& receiver_maps,
                              Handle<Name> name, AccessMode access_mode,
                              LanguageMode language_mode,
                              Node* index = nullptr);
  Reduction ReduceGlobalAccess(Node* node, Node* receiver, Node* value,
                               Handle<Name> name, AccessMode access_mode,
                               Node* index = nullptr);

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
  ValueEffectControl BuildPropertyAccess(Node* receiver, Node* value,
                                         Node* context, Node* frame_state,
                                         Node* effect, Node* control,
                                         Handle<Name> name,
                                         PropertyAccessInfo const& access_info,
                                         AccessMode access_mode,
                                         LanguageMode language_mode);

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
                       MapHandles const& maps);

  // Construct appropriate subgraph to extend properties backing store.
  Node* BuildExtendPropertiesBackingStore(Handle<Map> map, Node* properties,
                                          Node* effect, Node* control);

  // Adds stability dependencies on all prototypes of every class in
  // {receiver_type} up to (and including) the {holder}.
  void AssumePrototypesStable(MapHandles const& receiver_maps,
                              Handle<JSObject> holder);

  // Checks if we can turn the hole into undefined when loading an element
  // from an object with one of the {receiver_maps}; sets up appropriate
  // code dependencies and might use the array protector cell.
  bool CanTreatHoleAsUndefined(MapHandles const& receiver_maps);

  // Checks if we know at compile time that the {receiver} either definitely
  // has the {prototype} in it's prototype chain, or the {receiver} definitely
  // doesn't have the {prototype} in it's prototype chain.
  enum InferHasInPrototypeChainResult {
    kIsInPrototypeChain,
    kIsNotInPrototypeChain,
    kMayBeInPrototypeChain
  };
  InferHasInPrototypeChainResult InferHasInPrototypeChain(
      Node* receiver, Node* effect, Handle<JSReceiver> prototype);

  // Extract receiver maps from {nexus} and filter based on {receiver} if
  // possible.
  bool ExtractReceiverMaps(Node* receiver, Node* effect,
                           FeedbackNexus const& nexus,
                           MapHandles* receiver_maps);

  // Try to infer maps for the given {receiver} at the current {effect}.
  // If maps are returned then you can be sure that the {receiver} definitely
  // has one of the returned maps at this point in the program (identified
  // by {effect}).
  bool InferReceiverMaps(Node* receiver, Node* effect,
                         MapHandles* receiver_maps);
  // Try to infer a root map for the {receiver} independent of the current
  // program location.
  MaybeHandle<Map> InferReceiverRootMap(Node* receiver);

  ValueEffectControl InlineApiCall(
      Node* receiver, Node* context, Node* target, Node* frame_state,
      Node* parameter, Node* effect, Node* control,
      Handle<SharedFunctionInfo> shared_info,
      Handle<FunctionTemplateInfo> function_template_info);

  // Script context lookup logic.
  struct ScriptContextTableLookupResult;
  bool LookupInScriptContextTable(Handle<Name> name,
                                  ScriptContextTableLookupResult* result);

  Graph* graph() const;
  JSGraph* jsgraph() const { return jsgraph_; }
  Isolate* isolate() const;
  Factory* factory() const;
  CommonOperatorBuilder* common() const;
  JSOperatorBuilder* javascript() const;
  SimplifiedOperatorBuilder* simplified() const;
  MachineOperatorBuilder* machine() const;
  Flags flags() const { return flags_; }
  Handle<JSGlobalObject> global_object() const { return global_object_; }
  Handle<JSGlobalProxy> global_proxy() const { return global_proxy_; }
  Handle<Context> native_context() const { return native_context_; }
  CompilationDependencies* dependencies() const { return dependencies_; }
  Zone* zone() const { return zone_; }

  JSGraph* const jsgraph_;
  Flags const flags_;
  Handle<JSGlobalObject> global_object_;
  Handle<JSGlobalProxy> global_proxy_;
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
