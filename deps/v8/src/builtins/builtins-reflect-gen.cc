// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-utils-gen.h"
#include "src/builtins/builtins.h"
#include "src/code-stub-assembler.h"

namespace v8 {
namespace internal {

// ES section #sec-reflect.has
TF_BUILTIN(ReflectHas, CodeStubAssembler) {
  Node* target = Parameter(Descriptor::kTarget);
  Node* key = Parameter(Descriptor::kKey);
  Node* context = Parameter(Descriptor::kContext);

  ThrowIfNotJSReceiver(context, target, MessageTemplate::kCalledOnNonObject,
                       "Reflect.has");

  Return(CallBuiltin(Builtins::kHasProperty, context, key, target));
}

}  // namespace internal
}  // namespace v8
