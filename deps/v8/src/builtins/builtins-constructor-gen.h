// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BUILTINS_BUILTINS_CONSTRUCTOR_GEN_H_
#define V8_BUILTINS_BUILTINS_CONSTRUCTOR_GEN_H_

#include "src/codegen/code-stub-assembler.h"

namespace v8 {
namespace internal {

class ConstructorBuiltinsAssembler : public CodeStubAssembler {
 public:
  explicit ConstructorBuiltinsAssembler(compiler::CodeAssemblerState* state)
      : CodeStubAssembler(state) {}

  Node* EmitFastNewFunctionContext(Node* closure, Node* slots, Node* context,
                                   ScopeType scope_type);

  Node* EmitCreateRegExpLiteral(Node* feedback_vector, Node* slot,
                                Node* pattern, Node* flags, Node* context);
  Node* EmitCreateShallowArrayLiteral(Node* feedback_vector, Node* slot,
                                      Node* context, Label* call_runtime,
                                      AllocationSiteMode allocation_site_mode);

  Node* EmitCreateEmptyArrayLiteral(Node* feedback_vector, Node* slot,
                                    Node* context);

  Node* EmitCreateShallowObjectLiteral(Node* feedback_vector, Node* slot,
                                       Label* call_runtime);
  Node* EmitCreateEmptyObjectLiteral(Node* context);

  Node* EmitFastNewObject(Node* context, Node* target, Node* new_target);

  Node* EmitFastNewObject(Node* context, Node* target, Node* new_target,
                          Label* call_runtime);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_BUILTINS_BUILTINS_CONSTRUCTOR_GEN_H_
