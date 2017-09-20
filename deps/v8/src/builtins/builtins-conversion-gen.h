// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BUILTINS_BUILTINS_CONVERSION_GEN_H_
#define V8_BUILTINS_BUILTINS_CONVERSION_GEN_H_

#include "src/code-stub-assembler.h"

namespace v8 {
namespace internal {

class ConversionBuiltinsAssembler : public CodeStubAssembler {
 public:
  explicit ConversionBuiltinsAssembler(compiler::CodeAssemblerState* state)
      : CodeStubAssembler(state) {}

  Node* ToPrimitiveToString(Node* context, Node* input,
                            Variable* feedback = nullptr);

 protected:
  void Generate_NonPrimitiveToPrimitive(Node* context, Node* input,
                                        ToPrimitiveHint hint);

  void Generate_OrdinaryToPrimitive(Node* context, Node* input,
                                    OrdinaryToPrimitiveHint hint);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_BUILTINS_BUILTINS_CONVERSION_GEN_H_
