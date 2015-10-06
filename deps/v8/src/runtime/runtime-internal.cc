// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/runtime/runtime-utils.h"

#include "src/arguments.h"
#include "src/bootstrapper.h"
#include "src/conversions.h"
#include "src/debug/debug.h"
#include "src/frames-inl.h"
#include "src/messages.h"
#include "src/parser.h"
#include "src/prettyprinter.h"

namespace v8 {
namespace internal {

RUNTIME_FUNCTION(Runtime_CheckIsBootstrapping) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 0);
  RUNTIME_ASSERT(isolate->bootstrapper()->IsActive());
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_ImportToRuntime) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, container, 0);
  RUNTIME_ASSERT(isolate->bootstrapper()->IsActive());
  Bootstrapper::ImportNatives(isolate, container);
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_ImportExperimentalToRuntime) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, container, 0);
  RUNTIME_ASSERT(isolate->bootstrapper()->IsActive());
  Bootstrapper::ImportExperimentalNatives(isolate, container);
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_InstallJSBuiltins) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, container, 0);
  RUNTIME_ASSERT(isolate->bootstrapper()->IsActive());
  Bootstrapper::InstallJSBuiltins(isolate, container);
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_Throw) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  return isolate->Throw(args[0]);
}


RUNTIME_FUNCTION(Runtime_ReThrow) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  return isolate->ReThrow(args[0]);
}


RUNTIME_FUNCTION(Runtime_UnwindAndFindExceptionHandler) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 0);
  return isolate->UnwindAndFindHandler();
}


RUNTIME_FUNCTION(Runtime_PromoteScheduledException) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 0);
  return isolate->PromoteScheduledException();
}


RUNTIME_FUNCTION(Runtime_ThrowReferenceError) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, name, 0);
  THROW_NEW_ERROR_RETURN_FAILURE(
      isolate, NewReferenceError(MessageTemplate::kNotDefined, name));
}


RUNTIME_FUNCTION(Runtime_NewTypeError) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);
  CONVERT_INT32_ARG_CHECKED(template_index, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, arg0, 1);
  auto message_template =
      static_cast<MessageTemplate::Template>(template_index);
  return *isolate->factory()->NewTypeError(message_template, arg0);
}


RUNTIME_FUNCTION(Runtime_NewReferenceError) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);
  CONVERT_INT32_ARG_CHECKED(template_index, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, arg0, 1);
  auto message_template =
      static_cast<MessageTemplate::Template>(template_index);
  return *isolate->factory()->NewReferenceError(message_template, arg0);
}


RUNTIME_FUNCTION(Runtime_NewSyntaxError) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);
  CONVERT_INT32_ARG_CHECKED(template_index, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, arg0, 1);
  auto message_template =
      static_cast<MessageTemplate::Template>(template_index);
  return *isolate->factory()->NewSyntaxError(message_template, arg0);
}


RUNTIME_FUNCTION(Runtime_ThrowIteratorResultNotAnObject) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, value, 0);
  THROW_NEW_ERROR_RETURN_FAILURE(
      isolate,
      NewTypeError(MessageTemplate::kIteratorResultNotAnObject, value));
}


RUNTIME_FUNCTION(Runtime_ThrowStrongModeImplicitConversion) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 0);
  THROW_NEW_ERROR_RETURN_FAILURE(
      isolate, NewTypeError(MessageTemplate::kStrongImplicitConversion));
}


RUNTIME_FUNCTION(Runtime_PromiseRejectEvent) {
  DCHECK(args.length() == 3);
  HandleScope scope(isolate);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, promise, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, value, 1);
  CONVERT_BOOLEAN_ARG_CHECKED(debug_event, 2);
  if (debug_event) isolate->debug()->OnPromiseReject(promise, value);
  Handle<Symbol> key = isolate->factory()->promise_has_handler_symbol();
  // Do not report if we actually have a handler.
  if (JSReceiver::GetDataProperty(promise, key)->IsUndefined()) {
    isolate->ReportPromiseReject(promise, value,
                                 v8::kPromiseRejectWithNoHandler);
  }
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_PromiseRevokeReject) {
  DCHECK(args.length() == 1);
  HandleScope scope(isolate);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, promise, 0);
  Handle<Symbol> key = isolate->factory()->promise_has_handler_symbol();
  // At this point, no revocation has been issued before
  RUNTIME_ASSERT(JSReceiver::GetDataProperty(promise, key)->IsUndefined());
  isolate->ReportPromiseReject(promise, Handle<Object>(),
                               v8::kPromiseHandlerAddedAfterReject);
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_PromiseHasHandlerSymbol) {
  DCHECK(args.length() == 0);
  return isolate->heap()->promise_has_handler_symbol();
}


RUNTIME_FUNCTION(Runtime_StackGuard) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 0);

  // First check if this is a real stack overflow.
  StackLimitCheck check(isolate);
  if (check.JsHasOverflowed()) {
    return isolate->StackOverflow();
  }

  return isolate->stack_guard()->HandleInterrupts();
}


RUNTIME_FUNCTION(Runtime_Interrupt) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 0);
  return isolate->stack_guard()->HandleInterrupts();
}


RUNTIME_FUNCTION(Runtime_AllocateInNewSpace) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_SMI_ARG_CHECKED(size, 0);
  RUNTIME_ASSERT(IsAligned(size, kPointerSize));
  RUNTIME_ASSERT(size > 0);
  RUNTIME_ASSERT(size <= Page::kMaxRegularHeapObjectSize);
  return *isolate->factory()->NewFillerObject(size, false, NEW_SPACE);
}


RUNTIME_FUNCTION(Runtime_AllocateInTargetSpace) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);
  CONVERT_SMI_ARG_CHECKED(size, 0);
  CONVERT_SMI_ARG_CHECKED(flags, 1);
  RUNTIME_ASSERT(IsAligned(size, kPointerSize));
  RUNTIME_ASSERT(size > 0);
  RUNTIME_ASSERT(size <= Page::kMaxRegularHeapObjectSize);
  bool double_align = AllocateDoubleAlignFlag::decode(flags);
  AllocationSpace space = AllocateTargetSpace::decode(flags);
  return *isolate->factory()->NewFillerObject(size, double_align, space);
}


// Collect the raw data for a stack trace.  Returns an array of 4
// element segments each containing a receiver, function, code and
// native code offset.
RUNTIME_FUNCTION(Runtime_CollectStackTrace) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, error_object, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, caller, 1);

  if (!isolate->bootstrapper()->IsActive()) {
    // Optionally capture a more detailed stack trace for the message.
    RETURN_FAILURE_ON_EXCEPTION(
        isolate, isolate->CaptureAndSetDetailedStackTrace(error_object));
    // Capture a simple stack trace for the stack property.
    RETURN_FAILURE_ON_EXCEPTION(
        isolate, isolate->CaptureAndSetSimpleStackTrace(error_object, caller));
  }
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_RenderCallSite) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 0);
  MessageLocation location;
  isolate->ComputeLocation(&location);
  if (location.start_pos() == -1) return isolate->heap()->empty_string();

  Zone zone;
  base::SmartPointer<ParseInfo> info(
      location.function()->shared()->is_function()
          ? new ParseInfo(&zone, location.function())
          : new ParseInfo(&zone, location.script()));

  if (!Parser::ParseStatic(info.get())) {
    isolate->clear_pending_exception();
    return isolate->heap()->empty_string();
  }
  CallPrinter printer(isolate, &zone);
  const char* string = printer.Print(info->literal(), location.start_pos());
  return *isolate->factory()->NewStringFromAsciiChecked(string);
}


RUNTIME_FUNCTION(Runtime_MessageGetStartPosition) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_CHECKED(JSMessageObject, message, 0);
  return Smi::FromInt(message->start_position());
}


RUNTIME_FUNCTION(Runtime_MessageGetScript) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_CHECKED(JSMessageObject, message, 0);
  return message->script();
}


RUNTIME_FUNCTION(Runtime_ErrorToStringRT) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, error, 0);
  Handle<String> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result,
      isolate->error_tostring_helper()->Stringify(isolate, error));
  return *result;
}


RUNTIME_FUNCTION(Runtime_FormatMessageString) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 4);
  CONVERT_INT32_ARG_CHECKED(template_index, 0);
  CONVERT_ARG_HANDLE_CHECKED(String, arg0, 1);
  CONVERT_ARG_HANDLE_CHECKED(String, arg1, 2);
  CONVERT_ARG_HANDLE_CHECKED(String, arg2, 3);
  Handle<String> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result,
      MessageTemplate::FormatMessage(template_index, arg0, arg1, arg2));
  return *result;
}


#define CALLSITE_GET(NAME, RETURN)                   \
  RUNTIME_FUNCTION(Runtime_CallSite##NAME##RT) {     \
    HandleScope scope(isolate);                      \
    DCHECK(args.length() == 3);                      \
    CONVERT_ARG_HANDLE_CHECKED(Object, receiver, 0); \
    CONVERT_ARG_HANDLE_CHECKED(JSFunction, fun, 1);  \
    CONVERT_INT32_ARG_CHECKED(pos, 2);               \
    Handle<String> result;                           \
    CallSite call_site(receiver, fun, pos);          \
    return RETURN(call_site.NAME(isolate), isolate); \
  }

static inline Object* ReturnDereferencedHandle(Handle<Object> obj,
                                               Isolate* isolate) {
  return *obj;
}


static inline Object* ReturnPositiveSmiOrNull(int value, Isolate* isolate) {
  if (value >= 0) return Smi::FromInt(value);
  return isolate->heap()->null_value();
}


static inline Object* ReturnBoolean(bool value, Isolate* isolate) {
  return isolate->heap()->ToBoolean(value);
}


CALLSITE_GET(GetFileName, ReturnDereferencedHandle)
CALLSITE_GET(GetFunctionName, ReturnDereferencedHandle)
CALLSITE_GET(GetScriptNameOrSourceUrl, ReturnDereferencedHandle)
CALLSITE_GET(GetMethodName, ReturnDereferencedHandle)
CALLSITE_GET(GetLineNumber, ReturnPositiveSmiOrNull)
CALLSITE_GET(GetColumnNumber, ReturnPositiveSmiOrNull)
CALLSITE_GET(IsNative, ReturnBoolean)
CALLSITE_GET(IsToplevel, ReturnBoolean)
CALLSITE_GET(IsEval, ReturnBoolean)
CALLSITE_GET(IsConstructor, ReturnBoolean)

#undef CALLSITE_GET


RUNTIME_FUNCTION(Runtime_IS_VAR) {
  UNREACHABLE();  // implemented as macro in the parser
  return NULL;
}


RUNTIME_FUNCTION(Runtime_IncrementStatsCounter) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_CHECKED(String, name, 0);

  if (FLAG_native_code_counters) {
    StatsCounter(isolate, name->ToCString().get()).Increment();
  }
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_Likely) {
  DCHECK(args.length() == 1);
  return args[0];
}


RUNTIME_FUNCTION(Runtime_Unlikely) {
  DCHECK(args.length() == 1);
  return args[0];
}


RUNTIME_FUNCTION(Runtime_HarmonyToString) {
  // TODO(caitp): Delete this runtime method when removing --harmony-tostring
  return isolate->heap()->ToBoolean(FLAG_harmony_tostring);
}


RUNTIME_FUNCTION(Runtime_GetTypeFeedbackVector) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_CHECKED(JSFunction, function, 0);
  return function->shared()->feedback_vector();
}


RUNTIME_FUNCTION(Runtime_GetCallerJSFunction) {
  SealHandleScope shs(isolate);
  StackFrameIterator it(isolate);
  RUNTIME_ASSERT(it.frame()->type() == StackFrame::STUB);
  it.Advance();
  RUNTIME_ASSERT(it.frame()->type() == StackFrame::JAVA_SCRIPT);
  return JavaScriptFrame::cast(it.frame())->function();
}


RUNTIME_FUNCTION(Runtime_GetCodeStubExportsObject) {
  HandleScope shs(isolate);
  return isolate->heap()->code_stub_exports_object();
}

}  // namespace internal
}  // namespace v8
