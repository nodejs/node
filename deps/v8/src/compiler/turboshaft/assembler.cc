// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/turboshaft/assembler.h"

#include "src/builtins/builtins.h"
#include "src/execution/isolate.h"

namespace v8::internal::compiler::turboshaft {

Handle<Code> BuiltinCodeHandle(Builtin builtin, Isolate* isolate) {
  return isolate->builtins()->code_handle(builtin);
}

}  // namespace v8::internal::compiler::turboshaft
