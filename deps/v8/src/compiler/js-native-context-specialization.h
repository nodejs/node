// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_JS_NATIVE_CONTEXT_SPECIALIZATION_H_
#define V8_COMPILER_JS_NATIVE_CONTEXT_SPECIALIZATION_H_

#include "src/base/flags.h"
#include "src/compiler/graph-reducer.h"
#include "src/compiler/js-heap-broker.h"
#include "src/deoptimizer/deoptimize-reason.h"
#include "src/objects/map.h"

namespace v8 {
namespace internal {

// Forward declarations.
class Factory;
class FeedbackNexus;
class JSGlobalObject;
class JSGlobalProxy;
class StringConstantBase;

namespace compiler {

// Forward declarations.
enum class AccessMode;
class CommonOperatorBuilder;
class CompilationDependencies;
class ElementAccessInfo;
class JSGraph;
class JSHeapBroker;
class JSOperatorBuilder;
class MachineOperatorBuilder;
class PropertyAccessInfo;
class SimplifiedOperatorBuilder;
class TypeCache;

// Specializes a given JSGraph to a given native context, potentially constant
// folding some {LoadGlobal} nodes or strength reducing some {StoreGlobal}
// nodes.  And also specializes {LoadNamed} and {StoreNamed} nodes according
// to type feedback (if available).
class V8_EXPORT_PRIVATE JSNativeContextSpecialization final
    : public AdvancedReducer {
 public:
  // Flags that control the mode of operation.
  enum Flag {
    kNoFlags = 0u,
    kAccessorInliningEnabled = 1u << 0,
    kBailoutOnUninitialized = 1u << 1
  };
  using Flags = base::Flags<Flag>;

  JSNativeContextSpecialization(Editor* editor, JSGraph* jsgraph,
                                JSHeapBroker* broker, Flags flags,
                                CompilationDependencies* dependencies,
                                Zone* zone, Zone* shared_zone);

  const char* reducer_name() const override {
    return "JSNativeContextSpecialization";
  }

  Reduction Reduce(Node* node) final;

  // Utility for folding string constant concatenation.
  // Supports JSAdd nodes and nodes typed as string or number.
  // Public for the sake of unit testing.
  static base::Optional<size_t> GetMaxStringLength(JSHeapBroker* broker,
                                                   Node* node);

 private:
  Reduction ReduceJSAdd(Node* node);
  Reduction ReduceJSAsyncFunctionEnter(Node* node);
  Reduction ReduceJSAsyncFunctionReject(Node* node);
  Reduction ReduceJSAsyncFunctionResolve(Node* node);
  Reduction ReduceJSGetSuperConstructor(Node* node);
  Reduction ReduceJSInstanceOf(Node* node);
  Reduction ReduceJSHasInPrototypeChain(Node* node);
  Reduction ReduceJSOrdinaryHasInstance(Node* node);
  Reduction ReduceJSPromiseResolve(Node* node);
  Reduction ReduceJSResolvePromise(Node* node);
  Reduction ReduceJSLoadContext(Node* node);
  Reduction ReduceJSLoadGlobal(Node* node);
  Reduction ReduceJSStoreGlobal(Node* node);
  Reduction ReduceJSLoadNamed(Node* node);
  Reduction ReduceJSGetIterator(Node* node);
  Reduction ReduceJSStoreNamed(Node* node);
  Reduction ReduceJSHasProperty(Node* node);
  Reduction ReduceJSLoadProperty(Node* node);
  Reduction ReduceJSStoreProperty(Node* node);
  Reduction ReduceJSStoreNamedOwn(Node* node);
  Reduction ReduceJSStoreDataPropertyInLiteral(Node* node);
  Reduction ReduceJSStoreInArrayLiteral(Node* node);
  Reduction ReduceJSToObject(Node* node);

  Reduction ReduceElementAccess(Node* node, Node* index, Node* value,
                                ElementAccessFeedback const& processed);
  // In the case of non-keyed (named) accesses, pass the name as {static_name}
  // and use {nullptr} for {key} (load/store modes are irrelevant).
  Reduction ReducePropertyAccess(Node* node, Node* key,
                                 base::Optional<NameRef> static_name,
                                 Node* value, FeedbackSource const& source,
                                 AccessMode access_mode);
  Reduction ReduceNamedAccessFromNexus(Node* node, Node* value,
                                       FeedbackSource const& source,
                                       NameRef const& name,
                                       AccessMode access_mode);
  Reduction ReduceNamedAccess(Node* node, Node* value,
                              NamedAccessFeedback const& processed,
                              AccessMode access_mode, Node* key = nullptr);
  Reduction ReduceGlobalAccess(Node* node, Node* receiver, Node* value,
                               NameRef const& name, AccessMode access_mode,
                               Node* key = nullptr);
  Reduction ReduceGlobalAccess(Node* node, Node* receiver, Node* value,
                               NameRef const& name, AccessMode access_mode,
                               Node* key, PropertyCellRef const& property_cell);
  Reduction ReduceElementLoadFromHeapConstant(Node* node, Node* key,
                                              AccessMode access_mode,
                                              KeyedAccessLoadMode load_mode);
  Reduction ReduceElementAccessOnString(Node* node, Node* index, Node* value,
                                        KeyedAccessMode const& keyed_mode);

  Reduction ReduceSoftDeoptimize(Node* node, DeoptimizeReason reason);
  Reduction ReduceJSToString(Node* node);

  Reduction ReduceJSLoadPropertyWithEnumeratedKey(Node* node);

  const StringConstantBase* CreateDelayedStringConstant(Node* node);

  // A triple of nodes that represents a continuation.
  class ValueEffectControl final {
   public:
    ValueEffectControl()
        : value_(nullptr), effect_(nullptr), control_(nullptr) {}
    ValueEffectControl(Node* value, Node* effect, Node* control)
        : value_(value), effect_(effect), control_(control) {}

    Node* value() const { return value_; }
    Node* effect() const { return effect_; }
    Node* control() const { return control_; }

   private:
    Node* value_;
    Node* effect_;
    Node* control_;
  };

  // Construct the appropriate subgraph for property access.
  ValueEffectControl BuildPropertyAccess(Node* receiver, Node* value,
                                         Node* context, Node* frame_state,
                                         Node* effect, Node* control,
                                         NameRef const& name,
                                         ZoneVector<Node*>* if_exceptions,
                                         PropertyAccessInfo const& access_info,
                                         AccessMode access_mode);
  ValueEffectControl BuildPropertyLoad(Node* receiver, Node* context,
                                       Node* frame_state, Node* effect,
                                       Node* control, NameRef const& name,
                                       ZoneVector<Node*>* if_exceptions,
                                       PropertyAccessInfo const& access_info);

  ValueEffectControl BuildPropertyStore(Node* receiver, Node* value,
                                        Node* context, Node* frame_state,
                                        Node* effect, Node* control,
                                        NameRef const& name,
                                        ZoneVector<Node*>* if_exceptions,
                                        PropertyAccessInfo const& access_info,
                                        AccessMode access_mode);

  ValueEffectControl BuildPropertyTest(Node* effect, Node* control,
                                       PropertyAccessInfo const& access_info);

  // Helpers for accessor inlining.
  Node* InlinePropertyGetterCall(Node* receiver, Node* context,
                                 Node* frame_state, Node** effect,
                                 Node** control,
                                 ZoneVector<Node*>* if_exceptions,
                                 PropertyAccessInfo const& access_info);
  void InlinePropertySetterCall(Node* receiver, Node* value, Node* context,
                                Node* frame_state, Node** effect,
                                Node** control,
                                ZoneVector<Node*>* if_exceptions,
                                PropertyAccessInfo const& access_info);
  Node* InlineApiCall(Node* receiver, Node* holder, Node* frame_state,
                      Node* value, Node** effect, Node** control,
                      SharedFunctionInfoRef const& shared_info,
                      FunctionTemplateInfoRef const& function_template_info);

  // Construct the appropriate subgraph for element access.
  ValueEffectControl BuildElementAccess(Node* receiver, Node* index,
                                        Node* value, Node* effect,
                                        Node* control,
                                        ElementAccessInfo const& access_info,
                                        KeyedAccessMode const& keyed_mode);

  // Construct appropriate subgraph to load from a String.
  Node* BuildIndexedStringLoad(Node* receiver, Node* index, Node* length,
                               Node** effect, Node** control,
                               KeyedAccessLoadMode load_mode);

  // Construct appropriate subgraph to extend properties backing store.
  Node* BuildExtendPropertiesBackingStore(const MapRef& map, Node* properties,
                                          Node* effect, Node* control);

  // Construct appropriate subgraph to check that the {value} matches
  // the previously recorded {name} feedback.
  Node* BuildCheckEqualsName(NameRef const& name, Node* value, Node* effect,
                             Node* control);

  // Checks if we can turn the hole into undefined when loading an element
  // from an object with one of the {receiver_maps}; sets up appropriate
  // code dependencies and might use the array protector cell.
  bool CanTreatHoleAsUndefined(ZoneVector<Handle<Map>> const& receiver_maps);

  void RemoveImpossibleReceiverMaps(
      Node* receiver, ZoneVector<Handle<Map>>* receiver_maps) const;

  ElementAccessFeedback const& TryRefineElementAccessFeedback(
      ElementAccessFeedback const& feedback, Node* receiver,
      Node* effect) const;

  void FilterMapsAndGetPropertyAccessInfos(
      NamedAccessFeedback const& feedback, AccessMode access_mode,
      Node* receiver, Node* effect,
      ZoneVector<PropertyAccessInfo>* access_infos);

  // Try to infer maps for the given {receiver} at the current {effect}.
  bool InferReceiverMaps(Node* receiver, Node* effect,
                         ZoneVector<Handle<Map>>* receiver_maps) const;

  // Try to infer a root map for the {receiver} independent of the current
  // program location.
  base::Optional<MapRef> InferReceiverRootMap(Node* receiver) const;

  // Checks if we know at compile time that the {receiver} either definitely
  // has the {prototype} in it's prototype chain, or the {receiver} definitely
  // doesn't have the {prototype} in it's prototype chain.
  enum InferHasInPrototypeChainResult {
    kIsInPrototypeChain,
    kIsNotInPrototypeChain,
    kMayBeInPrototypeChain
  };
  InferHasInPrototypeChainResult InferHasInPrototypeChain(
      Node* receiver, Node* effect, HeapObjectRef const& prototype);

  Graph* graph() const;
  JSGraph* jsgraph() const { return jsgraph_; }

  JSHeapBroker* broker() const { return broker_; }
  Isolate* isolate() const;
  Factory* factory() const;
  CommonOperatorBuilder* common() const;
  JSOperatorBuilder* javascript() const;
  SimplifiedOperatorBuilder* simplified() const;
  Flags flags() const { return flags_; }
  Handle<JSGlobalObject> global_object() const { return global_object_; }
  Handle<JSGlobalProxy> global_proxy() const { return global_proxy_; }
  NativeContextRef native_context() const {
    return broker()->target_native_context();
  }
  CompilationDependencies* dependencies() const { return dependencies_; }
  Zone* zone() const { return zone_; }
  Zone* shared_zone() const { return shared_zone_; }

  JSGraph* const jsgraph_;
  JSHeapBroker* const broker_;
  Flags const flags_;
  Handle<JSGlobalObject> global_object_;
  Handle<JSGlobalProxy> global_proxy_;
  CompilationDependencies* const dependencies_;
  Zone* const zone_;
  Zone* const shared_zone_;
  TypeCache const* type_cache_;

  DISALLOW_COPY_AND_ASSIGN(JSNativeContextSpecialization);
};

DEFINE_OPERATORS_FOR_FLAGS(JSNativeContextSpecialization::Flags)

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_JS_NATIVE_CONTEXT_SPECIALIZATION_H_
