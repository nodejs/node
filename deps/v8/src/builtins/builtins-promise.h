// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/code-stub-assembler.h"
#include "src/contexts.h"

namespace v8 {
namespace internal {

typedef compiler::Node Node;
typedef CodeStubAssembler::ParameterMode ParameterMode;
typedef compiler::CodeAssemblerState CodeAssemblerState;

class PromiseBuiltinsAssembler : public CodeStubAssembler {
 public:
  enum PromiseResolvingFunctionContextSlot {
    // Whether the resolve/reject callback was already called.
    kAlreadyVisitedSlot = Context::MIN_CONTEXT_SLOTS,

    // The promise which resolve/reject callbacks fulfill.
    kPromiseSlot,

    // Whether to trigger a debug event or not. Used in catch
    // prediction.
    kDebugEventSlot,
    kPromiseContextLength,
  };

  enum FunctionContextSlot {
    kCapabilitySlot = Context::MIN_CONTEXT_SLOTS,

    kCapabilitiesContextLength,
  };

  explicit PromiseBuiltinsAssembler(CodeAssemblerState* state)
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
  Node* AllocateAndSetJSPromise(Node* context, Node* status, Node* result);

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

  Node* ThrowIfNotJSReceiver(Node* context, Node* value,
                             MessageTemplate::Template msg_template,
                             const char* method_name = nullptr);

  Node* SpeciesConstructor(Node* context, Node* object,
                           Node* default_constructor);

  void PromiseSetHasHandler(Node* promise);

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

 private:
  Node* AllocateJSPromise(Node* context);
};

}  // namespace internal
}  // namespace v8
