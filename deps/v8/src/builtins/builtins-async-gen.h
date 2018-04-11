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
  void Await(Node* context, Node* generator, Node* value, Node* outer_promise,
             Builtins::Name fulfill_builtin, Builtins::Name reject_builtin,
             Node* is_predicted_as_caught);
  void Await(Node* context, Node* generator, Node* value, Node* outer_promise,
             Builtins::Name fulfill_builtin, Builtins::Name reject_builtin,
             bool is_predicted_as_caught);

  // Return a new built-in function object as defined in
  // Async Iterator Value Unwrap Functions
  Node* CreateUnwrapClosure(Node* const native_context, Node* const done);

 private:
  Node* AllocateAsyncIteratorValueUnwrapContext(Node* native_context,
                                                Node* done);
  Node* AllocateAwaitPromiseJobTask(Node* generator, Node* fulfill_handler,
                                    Node* reject_handler, Node* promise,
                                    Node* context);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_BUILTINS_BUILTINS_ASYNC_GEN_H_
