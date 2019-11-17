// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-utils-gen.h"
#include "src/builtins/builtins.h"
#include "src/codegen/code-stub-assembler.h"
#include "src/codegen/macro-assembler.h"
#include "src/execution/frame-constants.h"

namespace v8 {
namespace internal {

TF_BUILTIN(FastConsoleAssert, CodeStubAssembler) {
  Label runtime(this);
  Label out(this);

  TNode<Int32T> argc =
      UncheckedCast<Int32T>(Parameter(Descriptor::kJSActualArgumentsCount));
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<Object> new_target = CAST(Parameter(Descriptor::kJSNewTarget));
  GotoIf(Word32Equal(argc, Int32Constant(0)), &runtime);

  CodeStubArguments args(this, argc);
  BranchIfToBooleanIsTrue(args.AtIndex(0), &out, &runtime);
  BIND(&out);
  args.PopAndReturn(UndefinedConstant());

  BIND(&runtime);
  {
    // We are not using Parameter(Descriptor::kJSTarget) and loading the value
    // from the current frame here in order to reduce register pressure on the
    // fast path.
    TNode<JSFunction> target = LoadTargetFromFrame();
    TailCallBuiltin(Builtins::kConsoleAssert, context, target, new_target,
                    argc);
  }
}

}  // namespace internal
}  // namespace v8
