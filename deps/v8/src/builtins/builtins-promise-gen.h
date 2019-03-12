// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BUILTINS_BUILTINS_PROMISE_GEN_H_
#define V8_BUILTINS_BUILTINS_PROMISE_GEN_H_

#include "src/code-stub-assembler.h"
#include "src/objects/promise.h"
#include "torque-generated/builtins-base-from-dsl-gen.h"
#include "torque-generated/builtins-iterator-from-dsl-gen.h"

namespace v8 {
namespace internal {

typedef compiler::CodeAssemblerState CodeAssemblerState;

class PromiseBuiltinsAssembler : public CodeStubAssembler {
 public:
  explicit PromiseBuiltinsAssembler(compiler::CodeAssemblerState* state)
      : CodeStubAssembler(state) {}
  // These allocate and initialize a promise with pending state and
  // undefined fields.
  //
  // This uses undefined as the parent promise for the promise init
  // hook.
  Node* AllocateAndInitJSPromise(Node* context);
  // This uses the given parent as the parent promise for the promise
  // init hook.
  Node* AllocateAndInitJSPromise(Node* context, Node* parent);

  // This allocates and initializes a promise with the given state and
  // fields.
  Node* AllocateAndSetJSPromise(Node* context, v8::Promise::PromiseState status,
                                Node* result);

  Node* AllocatePromiseReaction(Node* next, Node* promise_or_capability,
                                Node* fulfill_handler, Node* reject_handler);

  Node* AllocatePromiseReactionJobTask(RootIndex map_root_index, Node* context,
                                       Node* argument, Node* handler,
                                       Node* promise_or_capability);
  Node* AllocatePromiseReactionJobTask(Node* map, Node* context, Node* argument,
                                       Node* handler,
                                       Node* promise_or_capability);
  Node* AllocatePromiseResolveThenableJobTask(Node* promise_to_resolve,
                                              Node* then, Node* thenable,
                                              Node* context);

  std::pair<Node*, Node*> CreatePromiseResolvingFunctions(
      Node* promise, Node* native_context, Node* promise_context);

  Node* PromiseHasHandler(Node* promise);

  // Creates the context used by all Promise.all resolve element closures,
  // together with the values array. Since all closures for a single Promise.all
  // call use the same context, we need to store the indices for the individual
  // closures somewhere else (we put them into the identity hash field of the
  // closures), and we also need to have a separate marker for when the closure
  // was called already (we slap the native context onto the closure in that
  // case to mark it's done).
  Node* CreatePromiseAllResolveElementContext(Node* promise_capability,
                                              Node* native_context);
  Node* CreatePromiseAllResolveElementFunction(Node* context, TNode<Smi> index,
                                               Node* native_context);

  Node* CreatePromiseResolvingFunctionsContext(Node* promise, Node* debug_event,
                                               Node* native_context);

  Node* CreatePromiseGetCapabilitiesExecutorContext(Node* native_context,
                                                    Node* promise_capability);

 protected:
  void PromiseInit(Node* promise);

  void PromiseSetHasHandler(Node* promise);
  void PromiseSetHandledHint(Node* promise);

  void PerformPromiseThen(Node* context, Node* promise, Node* on_fulfilled,
                          Node* on_rejected,
                          Node* result_promise_or_capability);

  Node* CreatePromiseContext(Node* native_context, int slots);

  Node* TriggerPromiseReactions(Node* context, Node* promise, Node* result,
                                PromiseReaction::Type type);

  // We can skip the "resolve" lookup on {constructor} if it's the (initial)
  // Promise constructor and the Promise.resolve() protector is intact, as
  // that guards the lookup path for the "resolve" property on the %Promise%
  // intrinsic object.
  void BranchIfPromiseResolveLookupChainIntact(Node* native_context,
                                               Node* constructor,
                                               Label* if_fast, Label* if_slow);
  void GotoIfNotPromiseResolveLookupChainIntact(Node* native_context,
                                                Node* constructor,
                                                Label* if_slow);

  // We can shortcut the SpeciesConstructor on {promise_map} if it's
  // [[Prototype]] is the (initial)  Promise.prototype and the @@species
  // protector is intact, as that guards the lookup path for the "constructor"
  // property on JSPromise instances which have the %PromisePrototype%.
  void BranchIfPromiseSpeciesLookupChainIntact(Node* native_context,
                                               Node* promise_map,
                                               Label* if_fast, Label* if_slow);

  // We can skip the "then" lookup on {receiver_map} if it's [[Prototype]]
  // is the (initial) Promise.prototype and the Promise#then() protector
  // is intact, as that guards the lookup path for the "then" property
  // on JSPromise instances which have the (initial) %PromisePrototype%.
  void BranchIfPromiseThenLookupChainIntact(Node* native_context,
                                            Node* receiver_map, Label* if_fast,
                                            Label* if_slow);

  Node* InvokeResolve(Node* native_context, Node* constructor, Node* value,
                      Label* if_exception, Variable* var_exception);
  template <typename... TArgs>
  Node* InvokeThen(Node* native_context, Node* receiver, TArgs... args);

  void BranchIfAccessCheckFailed(Node* context, Node* native_context,
                                 Node* promise_constructor, Node* executor,
                                 Label* if_noaccess);

  std::pair<Node*, Node*> CreatePromiseFinallyFunctions(Node* on_finally,
                                                        Node* constructor,
                                                        Node* native_context);
  Node* CreateValueThunkFunction(Node* value, Node* native_context);

  Node* CreateThrowerFunction(Node* reason, Node* native_context);

  Node* PerformPromiseAll(
      Node* context, Node* constructor, Node* capability,
      const IteratorBuiltinsFromDSLAssembler::IteratorRecord& record,
      Label* if_exception, Variable* var_exception);

  void SetForwardingHandlerIfTrue(Node* context, Node* condition,
                                  const NodeGenerator& object);
  inline void SetForwardingHandlerIfTrue(Node* context, Node* condition,
                                         Node* object) {
    return SetForwardingHandlerIfTrue(context, condition,
                                      [object]() -> Node* { return object; });
  }
  void SetPromiseHandledByIfTrue(Node* context, Node* condition, Node* promise,
                                 const NodeGenerator& handled_by);

  Node* PromiseStatus(Node* promise);

  void PromiseReactionJob(Node* context, Node* argument, Node* handler,
                          Node* promise_or_capability,
                          PromiseReaction::Type type);

  Node* IsPromiseStatus(Node* actual, v8::Promise::PromiseState expected);
  void PromiseSetStatus(Node* promise, v8::Promise::PromiseState status);

  Node* AllocateJSPromise(Node* context);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_BUILTINS_BUILTINS_PROMISE_GEN_H_
