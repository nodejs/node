// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BUILTINS_BUILTINS_COLLECTIONS_GEN_H_
#define V8_BUILTINS_BUILTINS_COLLECTIONS_GEN_H_

#include "src/code-stub-assembler.h"

namespace v8 {
namespace internal {

void BranchIfIterableWithOriginalKeyOrValueMapIterator(
    compiler::CodeAssemblerState* state, compiler::TNode<Object> iterable,
    compiler::TNode<Context> context, compiler::CodeAssemblerLabel* if_true,
    compiler::CodeAssemblerLabel* if_false);

void BranchIfIterableWithOriginalValueSetIterator(
    compiler::CodeAssemblerState* state, compiler::TNode<Object> iterable,
    compiler::TNode<Context> context, compiler::CodeAssemblerLabel* if_true,
    compiler::CodeAssemblerLabel* if_false);

}  // namespace internal
}  // namespace v8

#endif  // V8_BUILTINS_BUILTINS_COLLECTIONS_GEN_H_
