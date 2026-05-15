// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BUILTINS_BUILTINS_ASYNC_GEN_H_
#define V8_BUILTINS_BUILTINS_ASYNC_GEN_H_

#include "src/builtins/builtins-promise-gen.h"
#include "src/objects/js-generator.h"

namespace v8 {
namespace internal {

class AsyncBuiltinsAssembler : public PromiseBuiltinsAssembler {
 public:
  explicit AsyncBuiltinsAssembler(compiler::CodeAssemblerState* state)
      : PromiseBuiltinsAssembler(state) {}

 protected:
  // Allocates an await context that stores the generator as extension.
  TNode<Context> AllocateAwaitContext(TNode<NativeContext> native_context,
                                      TNode<JSGeneratorObject> generator);

  // Callback that returns (on_resolve, on_reject) closures.
  // Responsible for allocating context and closures, or reusing existing ones.
  using GetClosures =
      std::function<std::pair<TNode<JSFunction>, TNode<JSFunction>>(
          TNode<NativeContext>)>;

  // Perform steps to resume generator after `value` is resolved.
  // Returns the Promise-wrapped `value`.
  TNode<Object> Await(TNode<Context> context,
                      TNode<JSGeneratorObject> generator, TNode<JSAny> value,
                      TNode<JSPromise> outer_promise,
                      const GetClosures& get_closures);
  TNode<Object> Await(TNode<Context> context,
                      TNode<JSGeneratorObject> generator, TNode<JSAny> value,
                      TNode<JSPromise> outer_promise, RootIndex on_resolve_sfi,
                      RootIndex on_reject_sfi);

  // Optimized Await for async functions that lazily allocates closures on
  // first await and reuses them for subsequent awaits. This avoids per-await
  // allocation of the context and closures, and saves memory for async
  // functions that never suspend.
  TNode<Object> AwaitWithReusableClosures(
      TNode<Context> context,
      TNode<JSAsyncFunctionObject> async_function_object, TNode<JSAny> value,
      TNode<JSPromise> outer_promise);

  // Return a new built-in function object as defined in
  // Async Iterator Value Unwrap Functions
  TNode<JSFunction> CreateUnwrapClosure(TNode<NativeContext> native_context,
                                        TNode<Boolean> done);

  // Branches to |if_non_thenable| when |value| is guaranteed not to have a
  // "then" property and no promise hooks/debug are active. Falls through to
  // |if_slow| otherwise. Used to gate fast paths that skip promise/closure
  // allocation for non-thenable awaited/yielded values.
  void BranchIfNonThenable(TNode<Context> context, TNode<Object> value,
                           Label* if_non_thenable, Label* if_slow);

  // Allocates an AsyncResumeTask, stores all fields, and enqueues it on
  // the microtask queue.  Used by both async generator yield and async
  // function await fast paths.
  void EnqueueAsyncResumeTask(TNode<NativeContext> native_context,
                              TNode<JSGeneratorObject> generator,
                              TNode<Object> value, int kind);

 private:
  TNode<Context> AllocateAsyncIteratorValueUnwrapContext(
      TNode<NativeContext> native_context, TNode<Boolean> done);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_BUILTINS_BUILTINS_ASYNC_GEN_H_
