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
  auto context = Parameter<Context>(Descriptor::kContext);
  auto input = Parameter<Object>(Descriptor::kArgument);

  Return(ToNumber(context, input));
}

TF_BUILTIN(PlainPrimitiveToNumber, CodeStubAssembler) {
  auto input = Parameter<Object>(Descriptor::kArgument);

  Return(PlainPrimitiveToNumber(input));
}

// Like ToNumber, but also converts BigInts.
TF_BUILTIN(ToNumberConvertBigInt, CodeStubAssembler) {
  auto context = Parameter<Context>(Descriptor::kContext);
  auto input = Parameter<Object>(Descriptor::kArgument);

  Return(ToNumber(context, input, BigIntHandling::kConvertToNumber));
}

// ES6 section 7.1.2 ToBoolean ( argument )
// Requires parameter on stack so that it can be used as a continuation from a
// LAZY deopt.
TF_BUILTIN(ToBooleanLazyDeoptContinuation, CodeStubAssembler) {
  auto value = Parameter<Object>(Descriptor::kArgument);

  Label return_true(this), return_false(this);
  BranchIfToBooleanIsTrue(value, &return_true, &return_false);

  BIND(&return_true);
  Return(TrueConstant());

  BIND(&return_false);
  Return(FalseConstant());
}

// ES6 section 12.5.5 typeof operator
TF_BUILTIN(Typeof, CodeStubAssembler) {
  auto object = Parameter<Object>(Descriptor::kObject);

  Return(Typeof(object));
}

}  // namespace internal
}  // namespace v8
