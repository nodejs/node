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

  void CallOrConstructWithArrayLike(TNode<Object> target,
                                    SloppyTNode<Object> new_target,
                                    TNode<Object> arguments_list,
                                    TNode<Context> context);
  void CallOrConstructDoubleVarargs(TNode<Object> target,
                                    SloppyTNode<Object> new_target,
                                    TNode<FixedDoubleArray> elements,
                                    TNode<Int32T> length,
                                    TNode<Int32T> args_count,
                                    TNode<Context> context, TNode<Int32T> kind);
  void CallOrConstructWithSpread(TNode<Object> target, TNode<Object> new_target,
                                 TNode<Object> spread, TNode<Int32T> args_count,
                                 TNode<Context> context);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_BUILTINS_BUILTINS_CALL_GEN_H_
