// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_IC_KEYED_STORE_GENERIC_H_
#define V8_IC_KEYED_STORE_GENERIC_H_

#include "src/compiler/code-assembler.h"
#include "src/globals.h"

namespace v8 {
namespace internal {

class KeyedStoreGenericGenerator {
 public:
  template <class T>
  using TNode = compiler::TNode<T>;

  static void Generate(compiler::CodeAssemblerState* state);

  // Building block for fast path of Object.assign implementation.
  static void SetProperty(compiler::CodeAssemblerState* state,
                          TNode<Context> context, TNode<JSReceiver> receiver,
                          TNode<BoolT> is_simple_receiver, TNode<Name> name,
                          TNode<Object> value, LanguageMode language_mode);
};

class StoreICUninitializedGenerator {
 public:
  static void Generate(compiler::CodeAssemblerState* state);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_IC_KEYED_STORE_GENERIC_H_
