// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BUILTINS_BUILTINS_ASYNC_GEN_H_
#define V8_BUILTINS_BUILTINS_ASYNC_GEN_H_

#include "src/builtins/builtins-promise-gen.h"

namespace v8 {
namespace internal {

class AsyncBuiltinsAssembler : public PromiseBuiltinsAssembler {
 public:
  explicit AsyncBuiltinsAssembler(compiler::CodeAssemblerState* state)
      : PromiseBuiltinsAssembler(state) {}

 protected:
  // Perform steps to resume generator after `value` is resolved.
  // `on_reject_context_index` is an index into the Native Context, which should
  // point to a SharedFunctioninfo instance used to create the closure. The
  // value following the reject index should be a similar value for the resolve
  // closure. Returns the Promise-wrapped `value`.
  Node* Await(Node* context, Node* generator, Node* value, Node* outer_promise,
              Node* on_resolve_context_index, Node* on_reject_context_index,
              Node* is_predicted_as_caught);
  Node* Await(Node* context, Node* generator, Node* value, Node* outer_promise,
              int on_resolve_context_index, int on_reject_context_index,
              Node* is_predicted_as_caught) {
    return Await(context, generator, value, outer_promise,
                 IntPtrConstant(on_resolve_context_index),
                 IntPtrConstant(on_reject_context_index),
                 is_predicted_as_caught);
  }
  Node* Await(Node* context, Node* generator, Node* value, Node* outer_promise,
              int on_resolve_context_index, int on_reject_context_index,
              bool is_predicted_as_caught) {
    return Await(context, generator, value, outer_promise,
                 on_resolve_context_index, on_reject_context_index,
                 BooleanConstant(is_predicted_as_caught));
  }

  // Return a new built-in function object as defined in
  // Async Iterator Value Unwrap Functions
  Node* CreateUnwrapClosure(Node* const native_context, Node* const done);

 private:
  void InitializeNativeClosure(Node* context, Node* native_context,
                               Node* function, Node* context_index);
  Node* AllocateAsyncIteratorValueUnwrapContext(Node* native_context,
                                                Node* done);

  Node* AwaitOld(Node* context, Node* generator, Node* value,
                 Node* outer_promise, Node* on_resolve_context_index,
                 Node* on_reject_context_index, Node* is_predicted_as_caught);
  Node* AwaitOptimized(Node* context, Node* generator, Node* value,
                       Node* outer_promise, Node* on_resolve_context_index,
                       Node* on_reject_context_index,
                       Node* is_predicted_as_caught);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_BUILTINS_BUILTINS_ASYNC_GEN_H_
