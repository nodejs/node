// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BUILTINS_BUILTINS_PROMISE_H_
#define V8_BUILTINS_BUILTINS_PROMISE_H_

#include "src/code-stub-assembler.h"
#include "src/contexts.h"

namespace v8 {
namespace internal {

typedef compiler::CodeAssemblerState CodeAssemblerState;

class PromiseBuiltinsAssembler : public CodeStubAssembler {
 public:
  enum PromiseResolvingFunctionContextSlot {
    // The promise which resolve/reject callbacks fulfill. If this is
    // undefined, then we've already visited this callback and it
    // should be a no-op.
    kPromiseSlot = Context::MIN_CONTEXT_SLOTS,

    // Whether to trigger a debug event or not. Used in catch
    // prediction.
    kDebugEventSlot,
    kPromiseContextLength,
  };

 protected:
  enum PromiseAllResolveElementContextSlots {
    // Whether the resolve callback was already called.
    kPromiseAllResolveElementAlreadyVisitedSlot = Context::MIN_CONTEXT_SLOTS,

    // Index into the values array
    kPromiseAllResolveElementIndexSlot,

    // Remaining elements count (mutable HeapNumber)
    kPromiseAllResolveElementRemainingElementsSlot,

    // Promise capability from Promise.all
    kPromiseAllResolveElementCapabilitySlot,

    // Values array from Promise.all
    kPromiseAllResolveElementValuesArraySlot,

    kPromiseAllResolveElementLength
  };

 public:
  enum FunctionContextSlot {
    kCapabilitySlot = Context::MIN_CONTEXT_SLOTS,

    kCapabilitiesContextLength,
  };

  // This is used by the Promise.prototype.finally builtin to store
  // onFinally callback and the Promise constructor.
  // TODO(gsathya): For native promises we can create a variant of
  // this without extra space for the constructor to save memory.
  enum PromiseFinallyContextSlot {
    kOnFinallySlot = Context::MIN_CONTEXT_SLOTS,
    kConstructorSlot,

    kPromiseFinallyContextLength,
  };

  // This is used by the ThenFinally and CatchFinally builtins to
  // store the value to return or reason to throw.
  enum PromiseValueThunkOrReasonContextSlot {
    kValueSlot = Context::MIN_CONTEXT_SLOTS,

    kPromiseValueThunkOrReasonContextLength,
  };

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

  Node* AllocatePromiseResolveThenableJobInfo(Node* result, Node* then,
                                              Node* resolve, Node* reject,
                                              Node* context);

  std::pair<Node*, Node*> CreatePromiseResolvingFunctions(
      Node* promise, Node* native_context, Node* promise_context);

  Node* PromiseHasHandler(Node* promise);

  Node* CreatePromiseResolvingFunctionsContext(Node* promise, Node* debug_event,
                                               Node* native_context);

  Node* CreatePromiseGetCapabilitiesExecutorContext(Node* native_context,
                                                    Node* promise_capability);

  Node* NewPromiseCapability(Node* context, Node* constructor,
                             Node* debug_event = nullptr);

 protected:
  void PromiseInit(Node* promise);

  Node* SpeciesConstructor(Node* context, Node* object,
                           Node* default_constructor);

  void PromiseSetHasHandler(Node* promise);
  void PromiseSetHandledHint(Node* promise);

  void AppendPromiseCallback(int offset, compiler::Node* promise,
                             compiler::Node* value);

  Node* InternalPromiseThen(Node* context, Node* promise, Node* on_resolve,
                            Node* on_reject);

  Node* InternalPerformPromiseThen(Node* context, Node* promise,
                                   Node* on_resolve, Node* on_reject,
                                   Node* deferred_promise,
                                   Node* deferred_on_resolve,
                                   Node* deferred_on_reject);

  void InternalResolvePromise(Node* context, Node* promise, Node* result);

  void BranchIfFastPath(Node* context, Node* promise, Label* if_isunmodified,
                        Label* if_ismodified);

  void BranchIfFastPath(Node* native_context, Node* promise_fun, Node* promise,
                        Label* if_isunmodified, Label* if_ismodified);

  Node* CreatePromiseContext(Node* native_context, int slots);
  void PromiseFulfill(Node* context, Node* promise, Node* result,
                      v8::Promise::PromiseState status);

  void BranchIfAccessCheckFailed(Node* context, Node* native_context,
                                 Node* promise_constructor, Node* executor,
                                 Label* if_noaccess);

  void InternalPromiseReject(Node* context, Node* promise, Node* value,
                             bool debug_event);
  void InternalPromiseReject(Node* context, Node* promise, Node* value,
                             Node* debug_event);
  std::pair<Node*, Node*> CreatePromiseFinallyFunctions(Node* on_finally,
                                                        Node* constructor,
                                                        Node* native_context);
  Node* CreateValueThunkFunction(Node* value, Node* native_context);

  Node* CreateThrowerFunction(Node* reason, Node* native_context);

  Node* PerformPromiseAll(Node* context, Node* constructor, Node* capability,
                          const IteratorRecord& record, Label* if_exception,
                          Variable* var_exception);

  Node* IncrementSmiCell(Node* cell, Label* if_overflow = nullptr);
  Node* DecrementSmiCell(Node* cell);

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
  void PerformFulfillClosure(Node* context, Node* value, bool should_resolve);

 private:
  Node* IsPromiseStatus(Node* actual, v8::Promise::PromiseState expected);
  void PromiseSetStatus(Node* promise, v8::Promise::PromiseState status);

  Node* AllocateJSPromise(Node* context);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_BUILTINS_BUILTINS_PROMISE_H_
