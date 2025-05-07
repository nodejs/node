// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/accessors.h"
#include "src/execution/isolate-inl.h"
#include "src/heap/heap-inl.h"  // For ToBoolean. TODO(jkummerow): Drop.

namespace v8 {
namespace internal {

// TODO(5530): Remove once uses in debug.js are gone.
RUNTIME_FUNCTION(Runtime_FunctionGetScriptSource) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  DirectHandle<JSReceiver> function = args.at<JSReceiver>(0);

  if (IsJSFunction(*function)) {
    DirectHandle<Object> script(Cast<JSFunction>(function)->shared()->script(),
                                isolate);
    if (IsScript(*script)) return Cast<Script>(script)->source();
  }
  return ReadOnlyRoots(isolate).undefined_value();
}

RUNTIME_FUNCTION(Runtime_FunctionGetScriptId) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  DirectHandle<JSReceiver> function = args.at<JSReceiver>(0);

  if (IsJSFunction(*function)) {
    DirectHandle<Object> script(Cast<JSFunction>(function)->shared()->script(),
                                isolate);
    if (IsScript(*script)) {
      return Smi::FromInt(Cast<Script>(script)->id());
    }
  }
  return Smi::FromInt(-1);
}

RUNTIME_FUNCTION(Runtime_FunctionGetSourceCode) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  DirectHandle<JSReceiver> function = args.at<JSReceiver>(0);
  if (IsJSFunction(*function)) {
    DirectHandle<SharedFunctionInfo> shared(
        Cast<JSFunction>(function)->shared(), isolate);
    return *SharedFunctionInfo::GetSourceCode(isolate, shared);
  }
  return ReadOnlyRoots(isolate).undefined_value();
}


RUNTIME_FUNCTION(Runtime_FunctionGetScriptSourcePosition) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(1, args.length());

  auto fun = Cast<JSFunction>(args[0]);
  int pos = fun->shared()->StartPosition();
  return Smi::FromInt(pos);
}


RUNTIME_FUNCTION(Runtime_FunctionIsAPIFunction) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(1, args.length());

  auto f = Cast<JSFunction>(args[0]);
  return isolate->heap()->ToBoolean(f->shared()->IsApiFunction());
}


RUNTIME_FUNCTION(Runtime_Call) {
  HandleScope scope(isolate);
  DCHECK_LE(2, args.length());
  int const argc = args.length() - 2;
  DirectHandle<Object> target = args.at(0);
  DirectHandle<Object> receiver = args.at(1);
  DirectHandleVector<Object> arguments(isolate, argc);
  for (int i = 0; i < argc; ++i) {
    arguments[i] = args.at(2 + i);
  }
  RETURN_RESULT_OR_FAILURE(isolate, Execution::Call(isolate, target, receiver,
                                                    base::VectorOf(arguments)));
}


}  // namespace internal
}  // namespace v8
