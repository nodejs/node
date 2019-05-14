// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-utils-gen.h"
#include "src/builtins/builtins.h"
#include "src/code-stub-assembler.h"
#include "src/frame-constants.h"
#include "src/macro-assembler.h"

namespace v8 {
namespace internal {

TF_BUILTIN(FastConsoleAssert, CodeStubAssembler) {
  Label runtime(this);
  Label out(this);

  // TODO(ishell): use constants from Descriptor once the JSFunction linkage
  // arguments are reordered.
  Node* argc = Parameter(Descriptor::kJSActualArgumentsCount);
  Node* context = Parameter(Descriptor::kContext);
  Node* new_target = Parameter(Descriptor::kJSNewTarget);
  GotoIf(Word32Equal(argc, Int32Constant(0)), &runtime);

  CodeStubArguments args(this, ChangeInt32ToIntPtr(argc));
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
