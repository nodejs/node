// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/runtime/runtime-utils.h"

#include "src/accessors.h"
#include "src/arguments.h"
#include "src/compiler.h"
#include "src/frames-inl.h"
#include "src/isolate-inl.h"
#include "src/messages.h"
#include "src/wasm/wasm-module.h"

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


RUNTIME_FUNCTION(Runtime_FunctionSetName) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());

  CONVERT_ARG_HANDLE_CHECKED(JSFunction, f, 0);
  CONVERT_ARG_HANDLE_CHECKED(String, name, 1);

  name = String::Flatten(name);
  f->shared()->set_name(*name);
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_FunctionRemovePrototype) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(1, args.length());

  CONVERT_ARG_CHECKED(JSFunction, f, 0);
  CHECK(f->RemovePrototype());
  f->shared()->SetConstructStub(
      *isolate->builtins()->ConstructedNonConstructable());

  return isolate->heap()->undefined_value();
}

// TODO(5530): Remove once uses in debug.js are gone.
RUNTIME_FUNCTION(Runtime_FunctionGetScript) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSReceiver, function, 0);

  if (function->IsJSFunction()) {
    Handle<Object> script(
        Handle<JSFunction>::cast(function)->shared()->script(), isolate);
    if (script->IsScript()) {
      return *Script::GetWrapper(Handle<Script>::cast(script));
    }
  }
  return isolate->heap()->undefined_value();
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
    return *Handle<JSFunction>::cast(function)->shared()->GetSourceCode();
  }
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_FunctionGetScriptSourcePosition) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(1, args.length());

  CONVERT_ARG_CHECKED(JSFunction, fun, 0);
  int pos = fun->shared()->start_position();
  return Smi::FromInt(pos);
}

RUNTIME_FUNCTION(Runtime_FunctionGetContextData) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(1, args.length());

  CONVERT_ARG_CHECKED(JSFunction, fun, 0);
  return fun->native_context()->debug_context_id();
}

RUNTIME_FUNCTION(Runtime_FunctionSetInstanceClassName) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(2, args.length());

  CONVERT_ARG_CHECKED(JSFunction, fun, 0);
  CONVERT_ARG_CHECKED(String, name, 1);
  fun->shared()->set_instance_class_name(name);
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_FunctionSetLength) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(2, args.length());

  CONVERT_ARG_CHECKED(JSFunction, fun, 0);
  CONVERT_SMI_ARG_CHECKED(length, 1);
  CHECK((length & 0xC0000000) == 0xC0000000 || (length & 0xC0000000) == 0x0);
  fun->shared()->set_length(length);
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_FunctionSetPrototype) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());

  CONVERT_ARG_HANDLE_CHECKED(JSFunction, fun, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, value, 1);
  CHECK(fun->IsConstructor());
  JSFunction::SetPrototype(fun, value);
  return args[0];  // return TOS
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

  Handle<SharedFunctionInfo> target_shared(target->shared());
  Handle<SharedFunctionInfo> source_shared(source->shared());

  if (!Compiler::Compile(source, Compiler::KEEP_EXCEPTION)) {
    return isolate->heap()->exception();
  }

  // Mark both, the source and the target, as un-flushable because the
  // shared unoptimized code makes them impossible to enqueue in a list.
  DCHECK(target_shared->code()->gc_metadata() == NULL);
  DCHECK(source_shared->code()->gc_metadata() == NULL);
  target_shared->set_dont_flush(true);
  source_shared->set_dont_flush(true);

  // Set the code, scope info, formal parameter count, and the length
  // of the target shared function info.
  target_shared->ReplaceCode(source_shared->code());
  if (source_shared->HasBytecodeArray()) {
    target_shared->set_bytecode_array(source_shared->bytecode_array());
  }
  target_shared->set_scope_info(source_shared->scope_info());
  target_shared->set_outer_scope_info(source_shared->outer_scope_info());
  target_shared->set_length(source_shared->GetLength());
  target_shared->set_feedback_metadata(source_shared->feedback_metadata());
  target_shared->set_internal_formal_parameter_count(
      source_shared->internal_formal_parameter_count());
  target_shared->set_start_position_and_type(
      source_shared->start_position_and_type());
  target_shared->set_end_position(source_shared->end_position());
  bool was_native = target_shared->native();
  target_shared->set_compiler_hints(source_shared->compiler_hints());
  target_shared->set_opt_count_and_bailout_reason(
      source_shared->opt_count_and_bailout_reason());
  target_shared->set_native(was_native);
  target_shared->set_profiler_ticks(source_shared->profiler_ticks());
  target_shared->set_function_literal_id(source_shared->function_literal_id());

  Handle<Object> source_script(source_shared->script(), isolate);
  if (source_script->IsScript()) {
    SharedFunctionInfo::SetScript(source_shared,
                                  isolate->factory()->undefined_value());
  }
  SharedFunctionInfo::SetScript(target_shared, source_script);

  // Set the code of the target function.
  target->ReplaceCode(source_shared->code());
  DCHECK(target->next_function_link()->IsUndefined(isolate));

  Handle<Context> context(source->context());
  target->set_context(*context);

  // Make sure we get a fresh copy of the literal vector to avoid cross
  // context contamination, and that the literal vector makes it's way into
  // the target_shared optimized code map.
  JSFunction::EnsureLiterals(target);

  if (isolate->logger()->is_logging_code_events() || isolate->is_profiling()) {
    isolate->logger()->LogExistingFunction(
        source_shared, Handle<AbstractCode>(source_shared->abstract_code()));
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
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_IsConstructor) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_CHECKED(Object, object, 0);
  return isolate->heap()->ToBoolean(object->IsConstructor());
}

RUNTIME_FUNCTION(Runtime_SetForceInlineFlag) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_CHECKED(Object, object, 0);

  if (object->IsJSFunction()) {
    JSFunction* func = JSFunction::cast(object);
    func->shared()->set_force_inline(true);
  }
  return isolate->heap()->undefined_value();
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


// ES6 section 9.2.1.2, OrdinaryCallBindThis for sloppy callee.
RUNTIME_FUNCTION(Runtime_ConvertReceiver) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Object, receiver, 0);
  return *Object::ConvertReceiver(isolate, receiver).ToHandleChecked();
}


RUNTIME_FUNCTION(Runtime_IsFunction) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_CHECKED(Object, object, 0);
  return isolate->heap()->ToBoolean(object->IsFunction());
}


RUNTIME_FUNCTION(Runtime_FunctionToString) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSReceiver, function, 0);
  return function->IsJSBoundFunction()
             ? *JSBoundFunction::ToString(
                   Handle<JSBoundFunction>::cast(function))
             : *JSFunction::ToString(Handle<JSFunction>::cast(function));
}

}  // namespace internal
}  // namespace v8
