// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_JS_CALL_REDUCER_H_
#define V8_COMPILER_JS_CALL_REDUCER_H_

#include "src/base/flags.h"
#include "src/compiler/frame-states.h"
#include "src/compiler/graph-reducer.h"
#include "src/deoptimize-reason.h"

namespace v8 {
namespace internal {

// Forward declarations.
class Factory;
class JSGlobalProxy;
class VectorSlotPair;

namespace compiler {

// Forward declarations.
class CallFrequency;
class CommonOperatorBuilder;
class CompilationDependencies;
struct FieldAccess;
class JSGraph;
class JSHeapBroker;
class JSOperatorBuilder;
class SimplifiedOperatorBuilder;

// Performs strength reduction on {JSConstruct} and {JSCall} nodes,
// which might allow inlining or other optimizations to be performed afterwards.
class V8_EXPORT_PRIVATE JSCallReducer final : public AdvancedReducer {
 public:
  // Flags that control the mode of operation.
  enum Flag { kNoFlags = 0u, kBailoutOnUninitialized = 1u << 0 };
  typedef base::Flags<Flag> Flags;

  JSCallReducer(Editor* editor, JSGraph* jsgraph, JSHeapBroker* js_heap_broker,
                Flags flags, Handle<Context> native_context,
                CompilationDependencies* dependencies)
      : AdvancedReducer(editor),
        jsgraph_(jsgraph),
        js_heap_broker_(js_heap_broker),
        flags_(flags),
        native_context_(native_context),
        dependencies_(dependencies) {}

  const char* reducer_name() const override { return "JSCallReducer"; }

  Reduction Reduce(Node* node) final;

  // Processes the waitlist gathered while the reducer was running,
  // and does a final attempt to reduce the nodes in the waitlist.
  void Finalize() final;

 private:
  Reduction ReduceArrayConstructor(Node* node);
  Reduction ReduceBooleanConstructor(Node* node);
  Reduction ReduceCallApiFunction(Node* node,
                                  Handle<SharedFunctionInfo> shared);
  Reduction ReduceFunctionPrototypeApply(Node* node);
  Reduction ReduceFunctionPrototypeBind(Node* node);
  Reduction ReduceFunctionPrototypeCall(Node* node);
  Reduction ReduceFunctionPrototypeHasInstance(Node* node);
  Reduction ReduceObjectConstructor(Node* node);
  Reduction ReduceObjectGetPrototype(Node* node, Node* object);
  Reduction ReduceObjectGetPrototypeOf(Node* node);
  Reduction ReduceObjectIs(Node* node);
  Reduction ReduceObjectPrototypeGetProto(Node* node);
  Reduction ReduceObjectPrototypeHasOwnProperty(Node* node);
  Reduction ReduceObjectPrototypeIsPrototypeOf(Node* node);
  Reduction ReduceObjectCreate(Node* node);
  Reduction ReduceReflectApply(Node* node);
  Reduction ReduceReflectConstruct(Node* node);
  Reduction ReduceReflectGet(Node* node);
  Reduction ReduceReflectGetPrototypeOf(Node* node);
  Reduction ReduceReflectHas(Node* node);
  Reduction ReduceArrayForEach(Node* node, Handle<SharedFunctionInfo> shared);
  enum class ArrayReduceDirection { kLeft, kRight };
  Reduction ReduceArrayReduce(Node* node, ArrayReduceDirection direction,
                              Handle<SharedFunctionInfo> shared);
  Reduction ReduceArrayMap(Node* node, Handle<SharedFunctionInfo> shared);
  Reduction ReduceArrayFilter(Node* node, Handle<SharedFunctionInfo> shared);
  enum class ArrayFindVariant { kFind, kFindIndex };
  Reduction ReduceArrayFind(Node* node, ArrayFindVariant variant,
                            Handle<SharedFunctionInfo> shared);
  Reduction ReduceArrayEvery(Node* node, Handle<SharedFunctionInfo> shared);
  enum class SearchVariant { kIncludes, kIndexOf };
  Reduction ReduceArrayIndexOfIncludes(SearchVariant search_variant,
                                       Node* node);
  Reduction ReduceArraySome(Node* node, Handle<SharedFunctionInfo> shared);
  Reduction ReduceArrayPrototypePush(Node* node);
  Reduction ReduceArrayPrototypePop(Node* node);
  Reduction ReduceArrayPrototypeShift(Node* node);
  Reduction ReduceArrayPrototypeSlice(Node* node);
  Reduction ReduceArrayIsArray(Node* node);
  enum class ArrayIteratorKind { kArray, kTypedArray };
  Reduction ReduceArrayIterator(Node* node, IterationKind kind);
  Reduction ReduceArrayIteratorPrototypeNext(Node* node);
  Reduction ReduceFastArrayIteratorNext(InstanceType type, Node* node,
                                        IterationKind kind);

  Reduction ReduceCallOrConstructWithArrayLikeOrSpread(
      Node* node, int arity, CallFrequency const& frequency,
      VectorSlotPair const& feedback);
  Reduction ReduceJSConstruct(Node* node);
  Reduction ReduceJSConstructWithArrayLike(Node* node);
  Reduction ReduceJSConstructWithSpread(Node* node);
  Reduction ReduceJSCall(Node* node);
  Reduction ReduceJSCall(Node* node, Handle<SharedFunctionInfo> shared);
  Reduction ReduceJSCallWithArrayLike(Node* node);
  Reduction ReduceJSCallWithSpread(Node* node);
  Reduction ReduceRegExpPrototypeTest(Node* node);
  Reduction ReduceReturnReceiver(Node* node);
  Reduction ReduceStringPrototypeIndexOf(Node* node);
  Reduction ReduceStringPrototypeSubstring(Node* node);
  Reduction ReduceStringPrototypeSlice(Node* node);
  Reduction ReduceStringPrototypeSubstr(Node* node);
  Reduction ReduceStringPrototypeStringAt(
      const Operator* string_access_operator, Node* node);
  Reduction ReduceStringPrototypeCharAt(Node* node);

#ifdef V8_INTL_SUPPORT
  Reduction ReduceStringPrototypeToLowerCaseIntl(Node* node);
  Reduction ReduceStringPrototypeToUpperCaseIntl(Node* node);
#endif  // V8_INTL_SUPPORT

  Reduction ReduceStringFromCharCode(Node* node);
  Reduction ReduceStringFromCodePoint(Node* node);
  Reduction ReduceStringPrototypeIterator(Node* node);
  Reduction ReduceStringIteratorPrototypeNext(Node* node);
  Reduction ReduceStringPrototypeConcat(Node* node,
                                        Handle<SharedFunctionInfo> shared);

  Reduction ReduceAsyncFunctionPromiseCreate(Node* node);
  Reduction ReduceAsyncFunctionPromiseRelease(Node* node);
  Reduction ReducePromiseConstructor(Node* node);
  Reduction ReducePromiseInternalConstructor(Node* node);
  Reduction ReducePromiseInternalReject(Node* node);
  Reduction ReducePromiseInternalResolve(Node* node);
  Reduction ReducePromisePrototypeCatch(Node* node);
  Reduction ReducePromisePrototypeFinally(Node* node);
  Reduction ReducePromisePrototypeThen(Node* node);
  Reduction ReducePromiseResolveTrampoline(Node* node);

  Reduction ReduceTypedArrayConstructor(Node* node,
                                        Handle<SharedFunctionInfo> shared);
  Reduction ReduceTypedArrayPrototypeToStringTag(Node* node);

  Reduction ReduceSoftDeoptimize(Node* node, DeoptimizeReason reason);

  Reduction ReduceMathUnary(Node* node, const Operator* op);
  Reduction ReduceMathBinary(Node* node, const Operator* op);
  Reduction ReduceMathImul(Node* node);
  Reduction ReduceMathClz32(Node* node);
  Reduction ReduceMathMinMax(Node* node, const Operator* op, Node* empty_value);

  Reduction ReduceNumberIsFinite(Node* node);
  Reduction ReduceNumberIsInteger(Node* node);
  Reduction ReduceNumberIsSafeInteger(Node* node);
  Reduction ReduceNumberIsNaN(Node* node);

  Reduction ReduceGlobalIsFinite(Node* node);
  Reduction ReduceGlobalIsNaN(Node* node);

  Reduction ReduceMapPrototypeHas(Node* node);
  Reduction ReduceMapPrototypeGet(Node* node);
  Reduction ReduceCollectionIteration(Node* node,
                                      CollectionKind collection_kind,
                                      IterationKind iteration_kind);
  Reduction ReduceCollectionPrototypeSize(Node* node,
                                          CollectionKind collection_kind);
  Reduction ReduceCollectionIteratorPrototypeNext(
      Node* node, int entry_size, Handle<HeapObject> empty_collection,
      InstanceType collection_iterator_instance_type_first,
      InstanceType collection_iterator_instance_type_last);

  Reduction ReduceArrayBufferIsView(Node* node);
  Reduction ReduceArrayBufferViewAccessor(Node* node,
                                          InstanceType instance_type,
                                          FieldAccess const& access);

  enum class DataViewAccess { kGet, kSet };
  Reduction ReduceDataViewAccess(Node* node, DataViewAccess access,
                                 ExternalArrayType element_type);

  Reduction ReduceDatePrototypeGetTime(Node* node);
  Reduction ReduceDateNow(Node* node);
  Reduction ReduceNumberParseInt(Node* node);

  Reduction ReduceNumberConstructor(Node* node);

  // Returns the updated {to} node, and updates control and effect along the
  // way.
  Node* DoFilterPostCallbackWork(ElementsKind kind, Node** control,
                                 Node** effect, Node* a, Node* to,
                                 Node* element, Node* callback_value);

  // If {fncallback} is not callable, throw a TypeError.
  // {control} is altered, and new nodes {check_fail} and {check_throw} are
  // returned. {check_fail} is the control branch where IsCallable failed,
  // and {check_throw} is the call to throw a TypeError in that
  // branch.
  void WireInCallbackIsCallableCheck(Node* fncallback, Node* context,
                                     Node* check_frame_state, Node* effect,
                                     Node** control, Node** check_fail,
                                     Node** check_throw);
  void RewirePostCallbackExceptionEdges(Node* check_throw, Node* on_exception,
                                        Node* effect, Node** check_fail,
                                        Node** control);

  // Begin the central loop of a higher-order array builtin. A Loop is wired
  // into {control}, an EffectPhi into {effect}, and the array index {k} is
  // threaded into a Phi, which is returned. It's helpful to save the
  // value of {control} as the loop node, and of {effect} as the corresponding
  // EffectPhi after function return.
  Node* WireInLoopStart(Node* k, Node** control, Node** effect);
  void WireInLoopEnd(Node* loop, Node* eloop, Node* vloop, Node* k,
                     Node* control, Node* effect);

  // Load receiver[k], first bounding k by receiver array length.
  // k is thusly changed, and the effect is changed as well.
  Node* SafeLoadElement(ElementsKind kind, Node* receiver, Node* control,
                        Node** effect, Node** k,
                        const VectorSlotPair& feedback);

  Node* CreateArtificialFrameState(Node* node, Node* outer_frame_state,
                                   int parameter_count, BailoutId bailout_id,
                                   FrameStateType frame_state_type,
                                   Handle<SharedFunctionInfo> shared,
                                   Node* context = nullptr);

  Graph* graph() const;
  JSGraph* jsgraph() const { return jsgraph_; }
  JSHeapBroker* js_heap_broker() const { return js_heap_broker_; }
  Isolate* isolate() const;
  Factory* factory() const;
  Handle<Context> native_context() const { return native_context_; }
  Handle<JSGlobalProxy> global_proxy() const;
  CommonOperatorBuilder* common() const;
  JSOperatorBuilder* javascript() const;
  SimplifiedOperatorBuilder* simplified() const;
  Flags flags() const { return flags_; }
  CompilationDependencies* dependencies() const { return dependencies_; }

  JSGraph* const jsgraph_;
  JSHeapBroker* const js_heap_broker_;
  Flags const flags_;
  Handle<Context> const native_context_;
  CompilationDependencies* const dependencies_;
  std::set<Node*> waitlist_;
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_JS_CALL_REDUCER_H_
