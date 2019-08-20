// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-bigint-gen.h"
#include "src/builtins/builtins-utils-gen.h"
#include "src/builtins/builtins.h"
#include "src/codegen/code-stub-assembler.h"

namespace v8 {
namespace internal {

// https://tc39.github.io/proposal-bigint/#sec-to-big-int64
TF_BUILTIN(BigIntToI64, CodeStubAssembler) {
  if (!Is64()) {
    Unreachable();
    return;
  }

  TNode<Object> value = CAST(Parameter(Descriptor::kArgument));
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<BigInt> bigint = ToBigInt(context, value);

  TVARIABLE(UintPtrT, var_low);
  TVARIABLE(UintPtrT, var_high);

  // 2. Let int64bit be n modulo 2^64.
  // 3. If int64bit ≥ 2^63, return int64bit - 2^64;
  BigIntToRawBytes(bigint, &var_low, &var_high);
  ReturnRaw(var_low.value());
}

// https://tc39.github.io/proposal-bigint/#sec-bigint-constructor-number-value
TF_BUILTIN(I64ToBigInt, CodeStubAssembler) {
  if (!Is64()) {
    Unreachable();
    return;
  }

  TNode<IntPtrT> argument =
      UncheckedCast<IntPtrT>(Parameter(Descriptor::kArgument));

  Return(BigIntFromInt64(argument));
}

}  // namespace internal
}  // namespace v8
