// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-utils-gen.h"
#include "src/builtins/builtins.h"
#include "src/codegen/code-factory.h"
#include "src/codegen/code-stub-assembler.h"
#include "src/objects/objects-inl.h"
#include "src/objects/oddball.h"

namespace v8 {
namespace internal {

// ES6 section 7.1.3 ToNumber ( argument )
TF_BUILTIN(ToNumber, CodeStubAssembler) {
  // TODO(solanes, v8:6949): Changing this to a TNode<Context> crashes with the
  // empty context. Context might not be needed, but it is propagated all over
  // the place and hard to pull out.
  Node* context = Parameter(Descriptor::kContext);
  TNode<Object> input = CAST(Parameter(Descriptor::kArgument));

  Return(ToNumber(context, input));
}

// Like ToNumber, but also converts BigInts.
TF_BUILTIN(ToNumberConvertBigInt, CodeStubAssembler) {
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<Object> input = CAST(Parameter(Descriptor::kArgument));

  Return(ToNumber(context, input, BigIntHandling::kConvertToNumber));
}

// ES6 section 7.1.2 ToBoolean ( argument )
// Requires parameter on stack so that it can be used as a continuation from a
// LAZY deopt.
TF_BUILTIN(ToBooleanLazyDeoptContinuation, CodeStubAssembler) {
  TNode<Object> value = CAST(Parameter(Descriptor::kArgument));

  Label return_true(this), return_false(this);
  BranchIfToBooleanIsTrue(value, &return_true, &return_false);

  BIND(&return_true);
  Return(TrueConstant());

  BIND(&return_false);
  Return(FalseConstant());
}

// ES6 section 12.5.5 typeof operator
TF_BUILTIN(Typeof, CodeStubAssembler) {
  TNode<Object> object = CAST(Parameter(Descriptor::kObject));

  Return(Typeof(object));
}

}  // namespace internal
}  // namespace v8
