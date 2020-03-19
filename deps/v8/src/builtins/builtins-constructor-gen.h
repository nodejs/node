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

  TNode<Context> EmitFastNewFunctionContext(TNode<ScopeInfo> scope_info,
                                            TNode<Uint32T> slots,
                                            TNode<Context> context,
                                            ScopeType scope_type);

  TNode<JSRegExp> EmitCreateRegExpLiteral(
      TNode<HeapObject> maybe_feedback_vector, TNode<TaggedIndex> slot,
      TNode<Object> pattern, TNode<Smi> flags, TNode<Context> context);

  TNode<JSArray> EmitCreateShallowArrayLiteral(
      TNode<FeedbackVector> feedback_vector, TNode<TaggedIndex> slot,
      TNode<Context> context, Label* call_runtime,
      AllocationSiteMode allocation_site_mode);

  TNode<JSArray> EmitCreateEmptyArrayLiteral(
      TNode<FeedbackVector> feedback_vector, TNode<TaggedIndex> slot,
      TNode<Context> context);

  TNode<HeapObject> EmitCreateShallowObjectLiteral(
      TNode<FeedbackVector> feedback_vector, TNode<TaggedIndex> slot,
      Label* call_runtime);
  TNode<JSObject> EmitCreateEmptyObjectLiteral(TNode<Context> context);

  TNode<JSObject> EmitFastNewObject(TNode<Context> context,
                                    TNode<JSFunction> target,
                                    TNode<JSReceiver> new_target);

  TNode<JSObject> EmitFastNewObject(TNode<Context> context,
                                    TNode<JSFunction> target,
                                    TNode<JSReceiver> new_target,
                                    Label* call_runtime);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_BUILTINS_BUILTINS_CONSTRUCTOR_GEN_H_
