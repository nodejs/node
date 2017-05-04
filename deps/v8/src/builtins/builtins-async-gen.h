// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BUILTINS_BUILTINS_ASYNC_H_
#define V8_BUILTINS_BUILTINS_ASYNC_H_

#include "src/builtins/builtins-promise-gen.h"

namespace v8 {
namespace internal {

class AsyncBuiltinsAssembler : public PromiseBuiltinsAssembler {
 public:
  explicit AsyncBuiltinsAssembler(compiler::CodeAssemblerState* state)
      : PromiseBuiltinsAssembler(state) {}

 protected:
  typedef std::function<Node*(Node*)> NodeGenerator1;

  // Perform steps to resume generator after `value` is resolved.
  // `on_reject_context_index` is an index into the Native Context, which should
  // point to a SharedFunctioninfo instance used to create the closure. The
  // value following the reject index should be a similar value for the resolve
  // closure. Returns the Promise-wrapped `value`.
  Node* Await(Node* context, Node* generator, Node* value, Node* outer_promise,
              const NodeGenerator1& create_closure_context,
              int on_resolve_context_index, int on_reject_context_index,
              bool is_predicted_as_caught);

  // Return a new built-in function object as defined in
  // Async Iterator Value Unwrap Functions
  Node* CreateUnwrapClosure(Node* const native_context, Node* const done);

 private:
  Node* AllocateAsyncIteratorValueUnwrapContext(Node* native_context,
                                                Node* done);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_BUILTINS_BUILTINS_ASYNC_H_
