// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BUILTINS_BUILTINS_CALL_GEN_H_
#define V8_BUILTINS_BUILTINS_CALL_GEN_H_

#include "src/code-stub-assembler.h"

namespace v8 {
namespace internal {

class CallOrConstructBuiltinsAssembler : public CodeStubAssembler {
 public:
  explicit CallOrConstructBuiltinsAssembler(compiler::CodeAssemblerState* state)
      : CodeStubAssembler(state) {}

  void CallOrConstructWithArrayLike(Node* target, Node* new_target,
                                    Node* arguments_list, Node* context);
  void CallOrConstructDoubleVarargs(Node* target, Node* new_target,
                                    Node* elements, Node* length,
                                    Node* args_count, Node* context,
                                    Node* kind);
  void CallOrConstructWithSpread(Node* target, Node* new_target, Node* spread,
                                 Node* args_count, Node* context);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_BUILTINS_BUILTINS_CALL_GEN_H_
