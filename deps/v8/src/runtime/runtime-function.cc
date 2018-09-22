// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/accessors.h"
#include "src/arguments-inl.h"
#include "src/compiler.h"
#include "src/isolate-inl.h"
#include "src/messages.h"
#include "src/runtime/runtime-utils.h"

namespace v8 {
namespace internal {

RUNTIME_FUNCTION(Runtime_FunctionGetName) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());

  CONVERT_ARG_HANDLE_CHECKED(JSReceiver, function, 0);
  if (function->IsJSBoundFunction()) {
    RETURN_RESULT_OR_FAILURE(
        isolate, JSBoundFunction::GetName(
                     isolate, Handle<JSBoundFunction>::cast(function)));
  } else {
    return *JSFunction::GetName(isolate, Handle<JSFunction>::cast(function));
  }
}

// TODO(5530): Remove once uses in debug.js are gone.
RUNTIME_FUNCTION(Runtime_FunctionGetScriptSource) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSReceiver, function, 0);

  if (function->IsJSFunction()) {
    Handle<Object> script(
        Handle<JSFunction>::cast(function)->shared()->script(), isolate);
    if (script->IsScript()) return Handle<Script>::cast(script)->source();
  }
  return ReadOnlyRoots(isolate).undefined_value();
}

RUNTIME_FUNCTION(Runtime_FunctionGetScriptId) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSReceiver, function, 0);

  if (function->IsJSFunction()) {
    Handle<Object> script(
        Handle<JSFunction>::cast(function)->shared()->script(), isolate);
    if (script->IsScript()) {
      return Smi::FromInt(Handle<Script>::cast(script)->id());
    }
  }
  return Smi::FromInt(-1);
}

RUNTIME_FUNCTION(Runtime_FunctionGetSourceCode) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSReceiver, function, 0);
  if (function->IsJSFunction()) {
    Handle<SharedFunctionInfo> shared(
        Handle<JSFunction>::cast(function)->shared(), isolate);
    return *SharedFunctionInfo::GetSourceCode(shared);
  }
  return ReadOnlyRoots(isolate).undefined_value();
}


RUNTIME_FUNCTION(Runtime_FunctionGetScriptSourcePosition) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(1, args.length());

  CONVERT_ARG_CHECKED(JSFunction, fun, 0);
  int pos = fun->shared()->StartPosition();
  return Smi::FromInt(pos);
}


RUNTIME_FUNCTION(Runtime_FunctionIsAPIFunction) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(1, args.length());

  CONVERT_ARG_CHECKED(JSFunction, f, 0);
  return isolate->heap()->ToBoolean(f->shared()->IsApiFunction());
}


RUNTIME_FUNCTION(Runtime_SetCode) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());

  CONVERT_ARG_HANDLE_CHECKED(JSFunction, target, 0);
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, source, 1);

  Handle<SharedFunctionInfo> target_shared(target->shared(), isolate);
  Handle<SharedFunctionInfo> source_shared(source->shared(), isolate);

  if (!source->is_compiled() &&
      !Compiler::Compile(source, Compiler::KEEP_EXCEPTION)) {
    return ReadOnlyRoots(isolate).exception();
  }

  // Set the function data, scope info, formal parameter count, and the length
  // of the target shared function info.
  target_shared->set_function_data(source_shared->function_data());
  target_shared->set_length(source_shared->GetLength());
  target_shared->set_raw_outer_scope_info_or_feedback_metadata(
      source_shared->raw_outer_scope_info_or_feedback_metadata());
  target_shared->set_internal_formal_parameter_count(
      source_shared->internal_formal_parameter_count());
  bool was_native = target_shared->native();
  target_shared->set_flags(source_shared->flags());
  target_shared->set_native(was_native);
  target_shared->set_scope_info(source_shared->scope_info());

  Handle<Object> source_script(source_shared->script(), isolate);
  int function_literal_id = source_shared->FunctionLiteralId(isolate);
  if (source_script->IsScript()) {
    SharedFunctionInfo::SetScript(source_shared,
                                  isolate->factory()->undefined_value(),
                                  function_literal_id);
  }
  SharedFunctionInfo::SetScript(target_shared, source_script,
                                function_literal_id);

  // Set the code of the target function.
  target->set_code(source_shared->GetCode());
  Handle<Context> context(source->context(), isolate);
  target->set_context(*context);

  // Make sure we get a fresh copy of the feedback vector to avoid cross
  // context contamination, and that the feedback vector makes it's way into
  // the target_shared optimized code map.
  JSFunction::EnsureFeedbackVector(target);

  if (isolate->logger()->is_listening_to_code_events() ||
      isolate->is_profiling()) {
    isolate->logger()->LogExistingFunction(
        source_shared, handle(source_shared->abstract_code(), isolate));
  }

  return *target;
}


// Set the native flag on the function.
// This is used to decide if we should transform null and undefined
// into the global object when doing call and apply.
RUNTIME_FUNCTION(Runtime_SetNativeFlag) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(1, args.length());

  CONVERT_ARG_CHECKED(Object, object, 0);

  if (object->IsJSFunction()) {
    JSFunction* func = JSFunction::cast(object);
    func->shared()->set_native(true);
  }
  return ReadOnlyRoots(isolate).undefined_value();
}


RUNTIME_FUNCTION(Runtime_IsConstructor) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_CHECKED(Object, object, 0);
  return isolate->heap()->ToBoolean(object->IsConstructor());
}


RUNTIME_FUNCTION(Runtime_Call) {
  HandleScope scope(isolate);
  DCHECK_LE(2, args.length());
  int const argc = args.length() - 2;
  CONVERT_ARG_HANDLE_CHECKED(Object, target, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, receiver, 1);
  ScopedVector<Handle<Object>> argv(argc);
  for (int i = 0; i < argc; ++i) {
    argv[i] = args.at(2 + i);
  }
  RETURN_RESULT_OR_FAILURE(
      isolate, Execution::Call(isolate, target, receiver, argc, argv.start()));
}


RUNTIME_FUNCTION(Runtime_IsFunction) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_CHECKED(Object, object, 0);
  return isolate->heap()->ToBoolean(object->IsFunction());
}


}  // namespace internal
}  // namespace v8
