// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/turboshaft/assembler.h"

#include "src/builtins/builtins.h"
#include "src/execution/isolate.h"
#include "src/objects/heap-object-inl.h"

namespace v8::internal::compiler::turboshaft {

Handle<Code> BuiltinCodeHandle(Builtin builtin, Isolate* isolate) {
  return isolate->builtins()->code_handle(builtin);
}

void CanonicalizeEmbeddedBuiltinsConstantIfNeededImpl(
    Isolate* isolate, Handle<HeapObject> object) {
  DCHECK_NOT_NULL(isolate);
  DCHECK(isolate->IsGeneratingEmbeddedBuiltins());

  RootIndex dummy_root;
  Builtin dummy_builtin;
  if (!isolate->roots_table().IsRootHandle(object, &dummy_root) &&
      !isolate->builtins()->IsBuiltinHandle(object, &dummy_builtin) &&
      !IsInstructionStream(*object)) {
    if (ThreadId::Current() == isolate->thread_id()) {
      // Adding to the table must be done on the main thread so that the
      // builtins constant indices are reproducible from run to run of
      // mksnapshot.
      isolate->builtins_constants_table_builder()->AddObject(object);
    } else {
      // On background threads we assume that the table has a valid entry for
      // this object already.
      CHECK(isolate->builtins_constants_table_builder()->HasObject(object));
    }
  }
}

}  // namespace v8::internal::compiler::turboshaft
