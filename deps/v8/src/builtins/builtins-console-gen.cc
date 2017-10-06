// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-utils-gen.h"
#include "src/builtins/builtins.h"
#include "src/code-stub-assembler.h"

namespace v8 {
namespace internal {

TF_BUILTIN(FastConsoleAssert, CodeStubAssembler) {
  Label runtime(this);
  Label out(this);

  // TODO(ishell): use constants from Descriptor once the JSFunction linkage
  // arguments are reordered.
  Node* argc = Parameter(BuiltinDescriptor::kArgumentsCount);
  Node* context = Parameter(BuiltinDescriptor::kContext);
  Node* new_target = Parameter(BuiltinDescriptor::kNewTarget);
  GotoIf(Word32Equal(argc, Int32Constant(0)), &runtime);

  CodeStubArguments args(this, ChangeInt32ToIntPtr(argc));
  BranchIfToBooleanIsTrue(args.AtIndex(0), &out, &runtime);
  BIND(&out);
  args.PopAndReturn(UndefinedConstant());

  BIND(&runtime);
  {
    Node* target = LoadFromFrame(StandardFrameConstants::kFunctionOffset,
                                 MachineType::TaggedPointer());
    TailCallBuiltin(Builtins::kConsoleAssert, context, target, new_target,
                    argc);
  }
}

}  // namespace internal
}  // namespace v8
