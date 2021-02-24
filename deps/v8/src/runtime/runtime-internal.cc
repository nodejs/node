// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "src/api/api.h"
#include "src/ast/ast-traversal-visitor.h"
#include "src/ast/prettyprinter.h"
#include "src/builtins/builtins.h"
#include "src/common/message-template.h"
#include "src/debug/debug.h"
#include "src/execution/arguments-inl.h"
#include "src/execution/frames-inl.h"
#include "src/execution/isolate-inl.h"
#include "src/execution/messages.h"
#include "src/execution/runtime-profiler.h"
#include "src/handles/maybe-handles.h"
#include "src/init/bootstrapper.h"
#include "src/logging/counters.h"
#include "src/numbers/conversions.h"
#include "src/objects/feedback-vector-inl.h"
#include "src/objects/js-array-inl.h"
#include "src/objects/template-objects-inl.h"
#include "src/parsing/parse-info.h"
#include "src/parsing/parsing.h"
#include "src/runtime/runtime-utils.h"
#include "src/snapshot/snapshot.h"
#include "src/strings/string-builder-inl.h"
#include "src/utils/ostreams.h"

namespace v8 {
namespace internal {

RUNTIME_FUNCTION(Runtime_AccessCheck) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSObject, object, 0);
  if (!isolate->MayAccess(handle(isolate->context(), isolate), object)) {
    isolate->ReportFailedAccessCheck(object);
    RETURN_FAILURE_IF_SCHEDULED_EXCEPTION(isolate);
  }
  return ReadOnlyRoots(isolate).undefined_value();
}

RUNTIME_FUNCTION(Runtime_FatalProcessOutOfMemoryInAllocateRaw) {
  HandleScope scope(isolate);
  DCHECK_EQ(0, args.length());
  isolate->heap()->FatalProcessOutOfMemory("CodeStubAssembler::AllocateRaw");
  UNREACHABLE();
}

RUNTIME_FUNCTION(Runtime_FatalProcessOutOfMemoryInvalidArrayLength) {
  HandleScope scope(isolate);
  DCHECK_EQ(0, args.length());
  isolate->heap()->FatalProcessOutOfMemory("invalid array length");
  UNREACHABLE();
}

RUNTIME_FUNCTION(Runtime_Throw) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  return isolate->Throw(args[0]);
}

RUNTIME_FUNCTION(Runtime_ReThrow) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  return isolate->ReThrow(args[0]);
}

RUNTIME_FUNCTION(Runtime_ThrowStackOverflow) {
  SealHandleScope shs(isolate);
  DCHECK_LE(0, args.length());
  return isolate->StackOverflow();
}

RUNTIME_FUNCTION(Runtime_ThrowSymbolAsyncIteratorInvalid) {
  HandleScope scope(isolate);
  DCHECK_EQ(0, args.length());
  THROW_NEW_ERROR_RETURN_FAILURE(
      isolate, NewTypeError(MessageTemplate::kSymbolAsyncIteratorInvalid));
}

#define THROW_ERROR(isolate, args, call)                               \
  HandleScope scope(isolate);                                          \
  DCHECK_LE(1, args.length());                                         \
  CONVERT_SMI_ARG_CHECKED(message_id_smi, 0);                          \
                                                                       \
  Handle<Object> undefined = isolate->factory()->undefined_value();    \
  Handle<Object> arg0 = (args.length() > 1) ? args.at(1) : undefined;  \
  Handle<Object> arg1 = (args.length() > 2) ? args.at(2) : undefined;  \
  Handle<Object> arg2 = (args.length() > 3) ? args.at(3) : undefined;  \
                                                                       \
  MessageTemplate message_id = MessageTemplateFromInt(message_id_smi); \
                                                                       \
  THROW_NEW_ERROR_RETURN_FAILURE(isolate, call(message_id, arg0, arg1, arg2));

RUNTIME_FUNCTION(Runtime_ThrowRangeError) {
  if (FLAG_correctness_fuzzer_suppressions) {
    DCHECK_LE(1, args.length());
    CONVERT_SMI_ARG_CHECKED(message_id_smi, 0);

    // If the result of a BigInt computation is truncated to 64 bit, Turbofan
    // can sometimes truncate intermediate results already, which can prevent
    // those from exceeding the maximum length, effectively preventing a
    // RangeError from being thrown. As this is a performance optimization, this
    // behavior is accepted. To prevent the correctness fuzzer from detecting
    // this difference, we crash the program.
    if (MessageTemplateFromInt(message_id_smi) ==
        MessageTemplate::kBigIntTooBig) {
      FATAL("Aborting on invalid BigInt length");
    }
  }

  THROW_ERROR(isolate, args, NewRangeError);
}

RUNTIME_FUNCTION(Runtime_ThrowTypeError) {
  THROW_ERROR(isolate, args, NewTypeError);
}

RUNTIME_FUNCTION(Runtime_ThrowTypeErrorIfStrict) {
  if (GetShouldThrow(isolate, Nothing<ShouldThrow>()) ==
      ShouldThrow::kDontThrow)
    return ReadOnlyRoots(isolate).undefined_value();
  THROW_ERROR(isolate, args, NewTypeError);
}

#undef THROW_ERROR

namespace {

const char* ElementsKindToType(ElementsKind fixed_elements_kind) {
  switch (fixed_elements_kind) {
#define ELEMENTS_KIND_CASE(Type, type, TYPE, ctype) \
  case TYPE##_ELEMENTS:                             \
    return #Type "Array";

    TYPED_ARRAYS(ELEMENTS_KIND_CASE)
#undef ELEMENTS_KIND_CASE

    default:
      UNREACHABLE();
  }
}

}  // namespace

RUNTIME_FUNCTION(Runtime_ThrowInvalidTypedArrayAlignment) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Map, map, 0);
  CONVERT_ARG_HANDLE_CHECKED(String, problem_string, 1);

  ElementsKind kind = map->elements_kind();

  Handle<String> type =
      isolate->factory()->NewStringFromAsciiChecked(ElementsKindToType(kind));

  ExternalArrayType external_type;
  size_t size;
  Factory::TypeAndSizeForElementsKind(kind, &external_type, &size);
  Handle<Object> element_size =
      handle(Smi::FromInt(static_cast<int>(size)), isolate);

  THROW_NEW_ERROR_RETURN_FAILURE(
      isolate, NewRangeError(MessageTemplate::kInvalidTypedArrayAlignment,
                             problem_string, type, element_size));
}

RUNTIME_FUNCTION(Runtime_UnwindAndFindExceptionHandler) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(0, args.length());
  return isolate->UnwindAndFindHandler();
}

RUNTIME_FUNCTION(Runtime_PromoteScheduledException) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(0, args.length());
  return isolate->PromoteScheduledException();
}

RUNTIME_FUNCTION(Runtime_ThrowReferenceError) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Object, name, 0);
  THROW_NEW_ERROR_RETURN_FAILURE(
      isolate, NewReferenceError(MessageTemplate::kNotDefined, name));
}

RUNTIME_FUNCTION(Runtime_ThrowAccessedUninitializedVariable) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Object, name, 0);
  THROW_NEW_ERROR_RETURN_FAILURE(
      isolate,
      NewReferenceError(MessageTemplate::kAccessedUninitializedVariable, name));
}

RUNTIME_FUNCTION(Runtime_NewError) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_INT32_ARG_CHECKED(template_index, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, arg0, 1);
  MessageTemplate message_template = MessageTemplateFromInt(template_index);
  return *isolate->factory()->NewError(message_template, arg0);
}

RUNTIME_FUNCTION(Runtime_NewTypeError) {
  HandleScope scope(isolate);
  DCHECK_LE(args.length(), 4);
  DCHECK_GE(args.length(), 1);
  CONVERT_INT32_ARG_CHECKED(template_index, 0);
  MessageTemplate message_template = MessageTemplateFromInt(template_index);

  Handle<Object> arg0;
  if (args.length() >= 2) {
    CHECK(args[1].IsObject());
    arg0 = args.at<Object>(1);
  }

  Handle<Object> arg1;
  if (args.length() >= 3) {
    CHECK(args[2].IsObject());
    arg1 = args.at<Object>(2);
  }
  Handle<Object> arg2;
  if (args.length() >= 4) {
    CHECK(args[3].IsObject());
    arg2 = args.at<Object>(3);
  }

  return *isolate->factory()->NewTypeError(message_template, arg0, arg1, arg2);
}

RUNTIME_FUNCTION(Runtime_NewReferenceError) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_INT32_ARG_CHECKED(template_index, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, arg0, 1);
  MessageTemplate message_template = MessageTemplateFromInt(template_index);
  return *isolate->factory()->NewReferenceError(message_template, arg0);
}

RUNTIME_FUNCTION(Runtime_NewSyntaxError) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_INT32_ARG_CHECKED(template_index, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, arg0, 1);
  MessageTemplate message_template = MessageTemplateFromInt(template_index);
  return *isolate->factory()->NewSyntaxError(message_template, arg0);
}

RUNTIME_FUNCTION(Runtime_ThrowInvalidStringLength) {
  HandleScope scope(isolate);
  THROW_NEW_ERROR_RETURN_FAILURE(isolate, NewInvalidStringLengthError());
}

RUNTIME_FUNCTION(Runtime_ThrowIteratorResultNotAnObject) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Object, value, 0);
  THROW_NEW_ERROR_RETURN_FAILURE(
      isolate,
      NewTypeError(MessageTemplate::kIteratorResultNotAnObject, value));
}

RUNTIME_FUNCTION(Runtime_ThrowThrowMethodMissing) {
  HandleScope scope(isolate);
  DCHECK_EQ(0, args.length());
  THROW_NEW_ERROR_RETURN_FAILURE(
      isolate, NewTypeError(MessageTemplate::kThrowMethodMissing));
}

RUNTIME_FUNCTION(Runtime_ThrowSymbolIteratorInvalid) {
  HandleScope scope(isolate);
  DCHECK_EQ(0, args.length());
  THROW_NEW_ERROR_RETURN_FAILURE(
      isolate, NewTypeError(MessageTemplate::kSymbolIteratorInvalid));
}

RUNTIME_FUNCTION(Runtime_ThrowNotConstructor) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Object, object, 0);
  THROW_NEW_ERROR_RETURN_FAILURE(
      isolate, NewTypeError(MessageTemplate::kNotConstructor, object));
}

RUNTIME_FUNCTION(Runtime_ThrowApplyNonFunction) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Object, object, 0);
  Handle<String> type = Object::TypeOf(isolate, object);
  THROW_NEW_ERROR_RETURN_FAILURE(
      isolate, NewTypeError(MessageTemplate::kApplyNonFunction, object, type));
}

RUNTIME_FUNCTION(Runtime_StackGuard) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(0, args.length());
  TRACE_EVENT0("v8.execute", "V8.StackGuard");

  // First check if this is a real stack overflow.
  StackLimitCheck check(isolate);
  if (check.JsHasOverflowed()) {
    return isolate->StackOverflow();
  }

  return isolate->stack_guard()->HandleInterrupts();
}

RUNTIME_FUNCTION(Runtime_StackGuardWithGap) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(args.length(), 1);
  CONVERT_UINT32_ARG_CHECKED(gap, 0);
  TRACE_EVENT0("v8.execute", "V8.StackGuard");

  // First check if this is a real stack overflow.
  StackLimitCheck check(isolate);
  if (check.JsHasOverflowed(gap)) {
    return isolate->StackOverflow();
  }

  return isolate->stack_guard()->HandleInterrupts();
}

RUNTIME_FUNCTION(Runtime_BytecodeBudgetInterruptFromBytecode) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, function, 0);
  function->raw_feedback_cell().set_interrupt_budget(FLAG_interrupt_budget);
  if (!function->has_feedback_vector()) {
    IsCompiledScope is_compiled_scope(
        function->shared().is_compiled_scope(isolate));
    JSFunction::EnsureFeedbackVector(function, &is_compiled_scope);
    // Also initialize the invocation count here. This is only really needed for
    // OSR. When we OSR functions with lazy feedback allocation we want to have
    // a non zero invocation count so we can inline functions.
    function->feedback_vector().set_invocation_count(1);
    return ReadOnlyRoots(isolate).undefined_value();
  }
  {
    SealHandleScope shs(isolate);
    isolate->counters()->runtime_profiler_ticks()->Increment();
    isolate->runtime_profiler()->MarkCandidatesForOptimizationFromBytecode();
    return ReadOnlyRoots(isolate).undefined_value();
  }
}

RUNTIME_FUNCTION(Runtime_BytecodeBudgetInterruptFromCode) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(FeedbackCell, feedback_cell, 0);

  DCHECK(feedback_cell->value().IsFeedbackVector());

  feedback_cell->set_interrupt_budget(FLAG_interrupt_budget);

  SealHandleScope shs(isolate);
  isolate->counters()->runtime_profiler_ticks()->Increment();
  isolate->runtime_profiler()->MarkCandidatesForOptimizationFromCode();
  return ReadOnlyRoots(isolate).undefined_value();
}

RUNTIME_FUNCTION(Runtime_AllocateInYoungGeneration) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_SMI_ARG_CHECKED(size, 0);
  CONVERT_SMI_ARG_CHECKED(flags, 1);
  bool double_align = AllocateDoubleAlignFlag::decode(flags);
  bool allow_large_object_allocation =
      AllowLargeObjectAllocationFlag::decode(flags);
  CHECK(IsAligned(size, kTaggedSize));
  CHECK_GT(size, 0);
  CHECK(FLAG_young_generation_large_objects ||
        size <= kMaxRegularHeapObjectSize);
  if (!allow_large_object_allocation) {
    CHECK(size <= kMaxRegularHeapObjectSize);
  }

  // TODO(v8:9472): Until double-aligned allocation is fixed for new-space
  // allocations, don't request it.
  double_align = false;

  return *isolate->factory()->NewFillerObject(size, double_align,
                                              AllocationType::kYoung,
                                              AllocationOrigin::kGeneratedCode);
}

RUNTIME_FUNCTION(Runtime_AllocateInOldGeneration) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_SMI_ARG_CHECKED(size, 0);
  CONVERT_SMI_ARG_CHECKED(flags, 1);
  bool double_align = AllocateDoubleAlignFlag::decode(flags);
  bool allow_large_object_allocation =
      AllowLargeObjectAllocationFlag::decode(flags);
  CHECK(IsAligned(size, kTaggedSize));
  CHECK_GT(size, 0);
  if (!allow_large_object_allocation) {
    CHECK(size <= kMaxRegularHeapObjectSize);
  }
  return *isolate->factory()->NewFillerObject(size, double_align,
                                              AllocationType::kOld,
                                              AllocationOrigin::kGeneratedCode);
}

RUNTIME_FUNCTION(Runtime_AllocateByteArray) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_SMI_ARG_CHECKED(length, 0);
  DCHECK_LT(0, length);
  return *isolate->factory()->NewByteArray(length);
}

RUNTIME_FUNCTION(Runtime_AllocateSeqOneByteString) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_SMI_ARG_CHECKED(length, 0);
  if (length == 0) return ReadOnlyRoots(isolate).empty_string();
  Handle<SeqOneByteString> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result, isolate->factory()->NewRawOneByteString(length));
  return *result;
}

RUNTIME_FUNCTION(Runtime_AllocateSeqTwoByteString) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_SMI_ARG_CHECKED(length, 0);
  if (length == 0) return ReadOnlyRoots(isolate).empty_string();
  Handle<SeqTwoByteString> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result, isolate->factory()->NewRawTwoByteString(length));
  return *result;
}

RUNTIME_FUNCTION(Runtime_ThrowIteratorError) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Object, object, 0);
  return isolate->Throw(*ErrorUtils::NewIteratorError(isolate, object));
}

RUNTIME_FUNCTION(Runtime_ThrowSpreadArgError) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_SMI_ARG_CHECKED(message_id_smi, 0);
  MessageTemplate message_id = MessageTemplateFromInt(message_id_smi);
  CONVERT_ARG_HANDLE_CHECKED(Object, object, 1);
  return ErrorUtils::ThrowSpreadArgError(isolate, message_id, object);
}

RUNTIME_FUNCTION(Runtime_ThrowCalledNonCallable) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Object, object, 0);
  return isolate->Throw(
      *ErrorUtils::NewCalledNonCallableError(isolate, object));
}

RUNTIME_FUNCTION(Runtime_ThrowConstructedNonConstructable) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Object, object, 0);
  return isolate->Throw(
      *ErrorUtils::NewConstructedNonConstructable(isolate, object));
}

RUNTIME_FUNCTION(Runtime_ThrowPatternAssignmentNonCoercible) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Object, object, 0);
  return ErrorUtils::ThrowLoadFromNullOrUndefined(isolate, object,
                                                  MaybeHandle<Object>());
}

RUNTIME_FUNCTION(Runtime_ThrowConstructorReturnedNonObject) {
  HandleScope scope(isolate);
  DCHECK_EQ(0, args.length());

  THROW_NEW_ERROR_RETURN_FAILURE(
      isolate,
      NewTypeError(MessageTemplate::kDerivedConstructorReturnedNonObject));
}

// ES6 section 7.3.17 CreateListFromArrayLike (obj)
RUNTIME_FUNCTION(Runtime_CreateListFromArrayLike) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Object, object, 0);
  RETURN_RESULT_OR_FAILURE(isolate, Object::CreateListFromArrayLike(
                                        isolate, object, ElementTypes::kAll));
}

RUNTIME_FUNCTION(Runtime_IncrementUseCounter) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_SMI_ARG_CHECKED(counter, 0);
  isolate->CountUsage(static_cast<v8::Isolate::UseCounterFeature>(counter));
  return ReadOnlyRoots(isolate).undefined_value();
}

RUNTIME_FUNCTION(Runtime_GetAndResetRuntimeCallStats) {
  HandleScope scope(isolate);

  // Append any worker thread runtime call stats to the main table before
  // printing.
  isolate->counters()->worker_thread_runtime_call_stats()->AddToMainTable(
      isolate->counters()->runtime_call_stats());

  if (args.length() == 0) {
    // Without arguments, the result is returned as a string.
    DCHECK_EQ(0, args.length());
    std::stringstream stats_stream;
    isolate->counters()->runtime_call_stats()->Print(stats_stream);
    Handle<String> result = isolate->factory()->NewStringFromAsciiChecked(
        stats_stream.str().c_str());
    isolate->counters()->runtime_call_stats()->Reset();
    return *result;
  } else {
    DCHECK_LE(args.length(), 2);
    std::FILE* f;
    if (args[0].IsString()) {
      // With a string argument, the results are appended to that file.
      CONVERT_ARG_HANDLE_CHECKED(String, arg0, 0);
      DisallowGarbageCollection no_gc;
      String::FlatContent flat = arg0->GetFlatContent(no_gc);
      const char* filename =
          reinterpret_cast<const char*>(&(flat.ToOneByteVector()[0]));
      f = std::fopen(filename, "a");
      DCHECK_NOT_NULL(f);
    } else {
      // With an integer argument, the results are written to stdout/stderr.
      CONVERT_SMI_ARG_CHECKED(fd, 0);
      DCHECK(fd == 1 || fd == 2);
      f = fd == 1 ? stdout : stderr;
    }
    // The second argument (if any) is a message header to be printed.
    if (args.length() >= 2) {
      CONVERT_ARG_HANDLE_CHECKED(String, arg1, 1);
      arg1->PrintOn(f);
      std::fputc('\n', f);
      std::fflush(f);
    }
    OFStream stats_stream(f);
    isolate->counters()->runtime_call_stats()->Print(stats_stream);
    isolate->counters()->runtime_call_stats()->Reset();
    if (args[0].IsString())
      std::fclose(f);
    else
      std::fflush(f);
    return ReadOnlyRoots(isolate).undefined_value();
  }
}

RUNTIME_FUNCTION(Runtime_OrdinaryHasInstance) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Object, callable, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, object, 1);
  RETURN_RESULT_OR_FAILURE(
      isolate, Object::OrdinaryHasInstance(isolate, callable, object));
}

RUNTIME_FUNCTION(Runtime_Typeof) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Object, object, 0);
  return *Object::TypeOf(isolate, object);
}

RUNTIME_FUNCTION(Runtime_AllowDynamicFunction) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, target, 0);
  Handle<JSObject> global_proxy(target->global_proxy(), isolate);
  return *isolate->factory()->ToBoolean(
      Builtins::AllowDynamicFunction(isolate, target, global_proxy));
}

RUNTIME_FUNCTION(Runtime_CreateAsyncFromSyncIterator) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());

  CONVERT_ARG_HANDLE_CHECKED(Object, sync_iterator, 0);

  if (!sync_iterator->IsJSReceiver()) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kSymbolIteratorInvalid));
  }

  Handle<Object> next;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, next,
      Object::GetProperty(isolate, sync_iterator,
                          isolate->factory()->next_string()));

  return *isolate->factory()->NewJSAsyncFromSyncIterator(
      Handle<JSReceiver>::cast(sync_iterator), next);
}

RUNTIME_FUNCTION(Runtime_GetTemplateObject) {
  HandleScope scope(isolate);
  DCHECK_EQ(3, args.length());
  CONVERT_ARG_HANDLE_CHECKED(TemplateObjectDescription, description, 0);
  CONVERT_ARG_HANDLE_CHECKED(SharedFunctionInfo, shared_info, 1);
  CONVERT_SMI_ARG_CHECKED(slot_id, 2);

  Handle<NativeContext> native_context(isolate->context().native_context(),
                                       isolate);
  return *TemplateObjectDescription::GetTemplateObject(
      isolate, native_context, description, shared_info, slot_id);
}

RUNTIME_FUNCTION(Runtime_ReportMessageFromMicrotask) {
  // Helper to report messages and continue JS execution. This is intended to
  // behave similarly to reporting exceptions which reach the top-level, but
  // allow the JS code to continue.
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());

  CONVERT_ARG_HANDLE_CHECKED(Object, exception, 0);

  DCHECK(!isolate->has_pending_exception());
  isolate->set_pending_exception(*exception);
  MessageLocation* no_location = nullptr;
  Handle<JSMessageObject> message =
      isolate->CreateMessageOrAbort(exception, no_location);
  MessageHandler::ReportMessage(isolate, no_location, message);
  isolate->clear_pending_exception();
  return ReadOnlyRoots(isolate).undefined_value();
}

RUNTIME_FUNCTION(Runtime_GetInitializerFunction) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());

  CONVERT_ARG_HANDLE_CHECKED(JSReceiver, constructor, 0);
  Handle<Symbol> key = isolate->factory()->class_fields_symbol();
  Handle<Object> initializer = JSReceiver::GetDataProperty(constructor, key);
  return *initializer;
}

RUNTIME_FUNCTION(Runtime_DoubleToStringWithRadix) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_DOUBLE_ARG_CHECKED(number, 0);
  CONVERT_INT32_ARG_CHECKED(radix, 1);

  char* const str = DoubleToRadixCString(number, radix);
  Handle<String> result = isolate->factory()->NewStringFromAsciiChecked(str);
  DeleteArray(str);
  return *result;
}

}  // namespace internal
}  // namespace v8
