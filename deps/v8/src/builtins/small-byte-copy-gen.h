// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BUILTINS_SMALL_BYTE_COPY_GEN_H_
#define V8_BUILTINS_SMALL_BYTE_COPY_GEN_H_

#include "src/builtins/builtins-typed-array-gen.h"

namespace v8::internal {

class SmallByteCopyAssembler : public TypedArrayBuiltinsAssembler {
 public:
  using TypedArrayBuiltinsAssembler::TypedArrayBuiltinsAssembler;
  TNode<JSTypedArray> TryAllocatePrivateByteView(
      TNode<Context> context, TNode<Map> map, TNode<UintPtrT> length,
      Label* generic);
  TNode<JSTypedArray> TryAllocateExternalByteView(
      TNode<Map> map, TNode<JSArrayBuffer> buffer, TNode<UintPtrT> length,
      Label* generic);
  void GotoIfUnsupportedByteViewMap(TNode<Map> map, Label* generic);
  void InitializeEmptyOnHeapBuffer(TNode<Context> context,
                                   TNode<JSArrayBuffer> buffer);
};

}  // namespace v8::internal

#endif  // V8_BUILTINS_SMALL_BYTE_COPY_GEN_H_
