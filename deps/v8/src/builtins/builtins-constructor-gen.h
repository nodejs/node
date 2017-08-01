// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BUILTINS_BUILTINS_CONSTRUCTOR_GEN_H_
#define V8_BUILTINS_BUILTINS_CONSTRUCTOR_GEN_H_

#include "src/code-stub-assembler.h"

namespace v8 {
namespace internal {

class ConstructorBuiltinsAssembler : public CodeStubAssembler {
 public:
  explicit ConstructorBuiltinsAssembler(compiler::CodeAssemblerState* state)
      : CodeStubAssembler(state) {}

  Node* EmitFastNewClosure(Node* shared_info, Node* feedback_vector, Node* slot,
                           Node* context);
  Node* EmitFastNewFunctionContext(Node* closure, Node* slots, Node* context,
                                   ScopeType scope_type);

  Node* EmitFastCloneRegExp(Node* closure, Node* literal_index, Node* pattern,
                            Node* flags, Node* context);
  Node* EmitFastCloneShallowArray(Node* closure, Node* literal_index,
                                  Node* context, Label* call_runtime,
                                  AllocationSiteMode allocation_site_mode);

  void CreateFastCloneShallowArrayBuiltin(
      AllocationSiteMode allocation_site_mode);

  Node* EmitFastCloneShallowObject(Label* call_runtime, Node* closure,
                                   Node* literals_index);

  Node* EmitFastNewObject(Node* context, Node* target, Node* new_target);

  Node* EmitFastNewObject(Node* context, Node* target, Node* new_target,
                          Label* call_runtime);

 private:
  Node* NonEmptyShallowClone(Node* boilerplate, Node* boilerplate_map,
                             Node* boilerplate_elements, Node* allocation_site,
                             Node* capacity, ElementsKind kind);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_BUILTINS_BUILTINS_CONSTRUCTOR_GEN_H_
