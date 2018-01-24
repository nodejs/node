// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/runtime/runtime-utils.h"

#include <memory>

#include "src/arguments.h"
#include "src/ast/prettyprinter.h"
#include "src/bootstrapper.h"
#include "src/builtins/builtins.h"
#include "src/conversions.h"
#include "src/debug/debug.h"
#include "src/frames-inl.h"
#include "src/isolate-inl.h"
#include "src/messages.h"
#include "src/parsing/parse-info.h"
#include "src/parsing/parsing.h"
#include "src/snapshot/snapshot.h"

namespace v8 {
namespace internal {

RUNTIME_FUNCTION(Runtime_CheckIsBootstrapping) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(0, args.length());
  CHECK(isolate->bootstrapper()->IsActive());
  return isolate->heap()->undefined_value();
}

RUNTIME_FUNCTION(Runtime_ExportFromRuntime) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSObject, container, 0);
  CHECK(isolate->bootstrapper()->IsActive());
  JSObject::NormalizeProperties(container, KEEP_INOBJECT_PROPERTIES, 10,
                                "ExportFromRuntime");
  Bootstrapper::ExportFromRuntime(isolate, container);
  JSObject::MigrateSlowToFast(container, 0, "ExportFromRuntime");
  return *container;
}

RUNTIME_FUNCTION(Runtime_InstallToContext) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSArray, array, 0);
  CHECK(array->HasFastElements());
  CHECK(isolate->bootstrapper()->IsActive());
  Handle<Context> native_context = isolate->native_context();
  Handle<FixedArray> fixed_array(FixedArray::cast(array->elements()));
  int length = Smi::ToInt(array->length());
  for (int i = 0; i < length; i += 2) {
    CHECK(fixed_array->get(i)->IsString());
    Handle<String> name(String::cast(fixed_array->get(i)));
    CHECK(fixed_array->get(i + 1)->IsJSObject());
    Handle<JSObject> object(JSObject::cast(fixed_array->get(i + 1)));
    int index = Context::ImportedFieldIndexForName(name);
    if (index == Context::kNotFound) {
      index = Context::IntrinsicIndexForName(name);
    }
    CHECK_NE(index, Context::kNotFound);
    native_context->set(index, *object);
  }
  return isolate->heap()->undefined_value();
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

#define THROW_ERROR(isolate, args, call)                              \
  HandleScope scope(isolate);                                         \
  DCHECK_LE(1, args.length());                                        \
  CONVERT_SMI_ARG_CHECKED(message_id_smi, 0);                         \
                                                                      \
  Handle<Object> undefined = isolate->factory()->undefined_value();   \
  Handle<Object> arg0 = (args.length() > 1) ? args.at(1) : undefined; \
  Handle<Object> arg1 = (args.length() > 2) ? args.at(2) : undefined; \
  Handle<Object> arg2 = (args.length() > 3) ? args.at(3) : undefined; \
                                                                      \
  MessageTemplate::Template message_id =                              \
      static_cast<MessageTemplate::Template>(message_id_smi);         \
                                                                      \
  THROW_NEW_ERROR_RETURN_FAILURE(isolate, call(message_id, arg0, arg1, arg2));

RUNTIME_FUNCTION(Runtime_ThrowRangeError) {
  THROW_ERROR(isolate, args, NewRangeError);
}

RUNTIME_FUNCTION(Runtime_ThrowTypeError) {
  THROW_ERROR(isolate, args, NewTypeError);
}

#undef THROW_ERROR

namespace {

const char* ElementsKindToType(ElementsKind fixed_elements_kind) {
  switch (fixed_elements_kind) {
#define ELEMENTS_KIND_CASE(Type, type, TYPE, ctype, size) \
  case TYPE##_ELEMENTS:                                   \
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

  ExternalArrayType external_type =
      isolate->factory()->GetArrayTypeFromElementsKind(kind);
  size_t size = isolate->factory()->GetExternalArrayElementSize(external_type);
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

RUNTIME_FUNCTION(Runtime_NewTypeError) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_INT32_ARG_CHECKED(template_index, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, arg0, 1);
  auto message_template =
      static_cast<MessageTemplate::Template>(template_index);
  return *isolate->factory()->NewTypeError(message_template, arg0);
}

RUNTIME_FUNCTION(Runtime_NewReferenceError) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_INT32_ARG_CHECKED(template_index, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, arg0, 1);
  auto message_template =
      static_cast<MessageTemplate::Template>(template_index);
  return *isolate->factory()->NewReferenceError(message_template, arg0);
}

RUNTIME_FUNCTION(Runtime_NewSyntaxError) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_INT32_ARG_CHECKED(template_index, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, arg0, 1);
  auto message_template =
      static_cast<MessageTemplate::Template>(template_index);
  return *isolate->factory()->NewSyntaxError(message_template, arg0);
}

RUNTIME_FUNCTION(Runtime_ThrowCannotConvertToPrimitive) {
  HandleScope scope(isolate);
  THROW_NEW_ERROR_RETURN_FAILURE(
      isolate, NewTypeError(MessageTemplate::kCannotConvertToPrimitive));
}

RUNTIME_FUNCTION(Runtime_ThrowIncompatibleMethodReceiver) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Object, arg0, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, arg1, 1);
  THROW_NEW_ERROR_RETURN_FAILURE(
      isolate,
      NewTypeError(MessageTemplate::kIncompatibleMethodReceiver, arg0, arg1));
}

RUNTIME_FUNCTION(Runtime_ThrowInvalidHint) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Object, hint, 0);
  THROW_NEW_ERROR_RETURN_FAILURE(
      isolate, NewTypeError(MessageTemplate::kInvalidHint, hint));
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

RUNTIME_FUNCTION(Runtime_ThrowNonCallableInInstanceOfCheck) {
  HandleScope scope(isolate);
  THROW_NEW_ERROR_RETURN_FAILURE(
      isolate, NewTypeError(MessageTemplate::kNonCallableInInstanceOfCheck));
}

RUNTIME_FUNCTION(Runtime_ThrowNonObjectInInstanceOfCheck) {
  HandleScope scope(isolate);
  THROW_NEW_ERROR_RETURN_FAILURE(
      isolate, NewTypeError(MessageTemplate::kNonObjectInInstanceOfCheck));
}

RUNTIME_FUNCTION(Runtime_ThrowNotConstructor) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Object, object, 0);
  THROW_NEW_ERROR_RETURN_FAILURE(
      isolate, NewTypeError(MessageTemplate::kNotConstructor, object));
}

RUNTIME_FUNCTION(Runtime_ThrowGeneratorRunning) {
  HandleScope scope(isolate);
  DCHECK_EQ(0, args.length());
  THROW_NEW_ERROR_RETURN_FAILURE(
      isolate, NewTypeError(MessageTemplate::kGeneratorRunning));
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

  // First check if this is a real stack overflow.
  StackLimitCheck check(isolate);
  if (check.JsHasOverflowed()) {
    return isolate->StackOverflow();
  }

  return isolate->stack_guard()->HandleInterrupts();
}

RUNTIME_FUNCTION(Runtime_Interrupt) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(0, args.length());
  return isolate->stack_guard()->HandleInterrupts();
}

RUNTIME_FUNCTION(Runtime_AllocateInNewSpace) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_SMI_ARG_CHECKED(size, 0);
  CHECK(IsAligned(size, kPointerSize));
  CHECK_GT(size, 0);
  CHECK_LE(size, kMaxRegularHeapObjectSize);
  return *isolate->factory()->NewFillerObject(size, false, NEW_SPACE);
}

RUNTIME_FUNCTION(Runtime_AllocateInTargetSpace) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_SMI_ARG_CHECKED(size, 0);
  CONVERT_SMI_ARG_CHECKED(flags, 1);
  CHECK(IsAligned(size, kPointerSize));
  CHECK_GT(size, 0);
  bool double_align = AllocateDoubleAlignFlag::decode(flags);
  AllocationSpace space = AllocateTargetSpace::decode(flags);
  CHECK(size <= kMaxRegularHeapObjectSize || space == LO_SPACE);
  return *isolate->factory()->NewFillerObject(size, double_align, space);
}

RUNTIME_FUNCTION(Runtime_AllocateSeqOneByteString) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_SMI_ARG_CHECKED(length, 0);
  if (length == 0) return isolate->heap()->empty_string();
  Handle<SeqOneByteString> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result, isolate->factory()->NewRawOneByteString(length));
  return *result;
}

RUNTIME_FUNCTION(Runtime_AllocateSeqTwoByteString) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_SMI_ARG_CHECKED(length, 0);
  if (length == 0) return isolate->heap()->empty_string();
  Handle<SeqTwoByteString> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result, isolate->factory()->NewRawTwoByteString(length));
  return *result;
}

RUNTIME_FUNCTION(Runtime_IS_VAR) {
  UNREACHABLE();  // implemented as macro in the parser
}

namespace {

bool ComputeLocation(Isolate* isolate, MessageLocation* target) {
  JavaScriptFrameIterator it(isolate);
  if (!it.done()) {
    // Compute the location from the function and the relocation info of the
    // baseline code. For optimized code this will use the deoptimization
    // information to get canonical location information.
    std::vector<FrameSummary> frames;
    it.frame()->Summarize(&frames);
    auto& summary = frames.back().AsJavaScript();
    Handle<SharedFunctionInfo> shared(summary.function()->shared());
    Handle<Object> script(shared->script(), isolate);
    int pos = summary.abstract_code()->SourcePosition(summary.code_offset());
    if (script->IsScript() &&
        !(Handle<Script>::cast(script)->source()->IsUndefined(isolate))) {
      Handle<Script> casted_script = Handle<Script>::cast(script);
      *target = MessageLocation(casted_script, pos, pos + 1, shared);
      return true;
    }
  }
  return false;
}

Handle<String> RenderCallSite(Isolate* isolate, Handle<Object> object,
                              CallPrinter::ErrorHint* hint) {
  MessageLocation location;
  if (ComputeLocation(isolate, &location)) {
    ParseInfo info(location.shared());
    if (parsing::ParseAny(&info, location.shared(), isolate)) {
      info.ast_value_factory()->Internalize(isolate);
      CallPrinter printer(isolate, location.shared()->IsUserJavaScript());
      Handle<String> str = printer.Print(info.literal(), location.start_pos());
      *hint = printer.GetErrorHint();
      if (str->length() > 0) return str;
    } else {
      isolate->clear_pending_exception();
    }
  }
  return Object::TypeOf(isolate, object);
}

MessageTemplate::Template UpdateErrorTemplate(
    CallPrinter::ErrorHint hint, MessageTemplate::Template default_id) {
  switch (hint) {
    case CallPrinter::ErrorHint::kNormalIterator:
      return MessageTemplate::kNotIterable;

    case CallPrinter::ErrorHint::kCallAndNormalIterator:
      return MessageTemplate::kNotCallableOrIterable;

    case CallPrinter::ErrorHint::kAsyncIterator:
      return MessageTemplate::kNotAsyncIterable;

    case CallPrinter::ErrorHint::kCallAndAsyncIterator:
      return MessageTemplate::kNotCallableOrAsyncIterable;

    case CallPrinter::ErrorHint::kNone:
      return default_id;
  }
  return default_id;
}

}  // namespace

MaybeHandle<Object> Runtime::ThrowIteratorError(Isolate* isolate,
                                                Handle<Object> object) {
  CallPrinter::ErrorHint hint = CallPrinter::kNone;
  Handle<String> callsite = RenderCallSite(isolate, object, &hint);
  MessageTemplate::Template id = MessageTemplate::kNonObjectPropertyLoad;

  if (hint == CallPrinter::kNone) {
    Handle<Symbol> iterator_symbol = isolate->factory()->iterator_symbol();
    THROW_NEW_ERROR(isolate, NewTypeError(id, iterator_symbol, callsite),
                    Object);
  }

  id = UpdateErrorTemplate(hint, id);
  THROW_NEW_ERROR(isolate, NewTypeError(id, callsite), Object);
}

RUNTIME_FUNCTION(Runtime_ThrowCalledNonCallable) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Object, object, 0);
  CallPrinter::ErrorHint hint = CallPrinter::kNone;
  Handle<String> callsite = RenderCallSite(isolate, object, &hint);
  MessageTemplate::Template id = MessageTemplate::kCalledNonCallable;
  id = UpdateErrorTemplate(hint, id);
  THROW_NEW_ERROR_RETURN_FAILURE(isolate, NewTypeError(id, callsite));
}

RUNTIME_FUNCTION(Runtime_ThrowCalledOnNullOrUndefined) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(String, name, 0);
  THROW_NEW_ERROR_RETURN_FAILURE(
      isolate, NewTypeError(MessageTemplate::kCalledOnNullOrUndefined, name));
}

RUNTIME_FUNCTION(Runtime_ThrowConstructedNonConstructable) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Object, object, 0);
  CallPrinter::ErrorHint hint = CallPrinter::kNone;
  Handle<String> callsite = RenderCallSite(isolate, object, &hint);
  MessageTemplate::Template id = MessageTemplate::kNotConstructor;
  THROW_NEW_ERROR_RETURN_FAILURE(isolate, NewTypeError(id, callsite));
}

RUNTIME_FUNCTION(Runtime_ThrowConstructorReturnedNonObject) {
  HandleScope scope(isolate);
  DCHECK_EQ(0, args.length());
  if (FLAG_harmony_restrict_constructor_return) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate,
        NewTypeError(MessageTemplate::kClassConstructorReturnedNonObject));
  }

  THROW_NEW_ERROR_RETURN_FAILURE(
      isolate,
      NewTypeError(MessageTemplate::kDerivedConstructorReturnedNonObject));
}

RUNTIME_FUNCTION(Runtime_ThrowUndefinedOrNullToObject) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(String, name, 0);
  THROW_NEW_ERROR_RETURN_FAILURE(
      isolate, NewTypeError(MessageTemplate::kUndefinedOrNullToObject, name));
}

// ES6 section 7.3.17 CreateListFromArrayLike (obj)
RUNTIME_FUNCTION(Runtime_CreateListFromArrayLike) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Object, object, 0);
  RETURN_RESULT_OR_FAILURE(isolate, Object::CreateListFromArrayLike(
                                        isolate, object, ElementTypes::kAll));
}

RUNTIME_FUNCTION(Runtime_DeserializeLazy) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, function, 0);

  DCHECK(FLAG_lazy_deserialization);

  Handle<SharedFunctionInfo> shared(function->shared(), isolate);
  int builtin_id = shared->lazy_deserialization_builtin_id();

  // At this point, the builtins table should definitely have DeserializeLazy
  // set at the position of the target builtin. Also, we should never lazily
  // deserialize DeserializeLazy.

  DCHECK_NE(Builtins::kDeserializeLazy, builtin_id);
  DCHECK_EQ(Builtins::kDeserializeLazy,
            isolate->builtins()->builtin(builtin_id)->builtin_index());

  // The DeserializeLazy builtin tail-calls the deserialized builtin. This only
  // works with JS-linkage.
  DCHECK(Builtins::IsLazy(builtin_id));
  DCHECK_EQ(Builtins::TFJ, Builtins::KindOf(builtin_id));

  if (FLAG_trace_lazy_deserialization) {
    PrintF("Lazy-deserializing builtin %s\n", Builtins::name(builtin_id));
  }

  Code* code = Snapshot::DeserializeBuiltin(isolate, builtin_id);
  DCHECK_EQ(builtin_id, code->builtin_index());
  DCHECK_EQ(code, isolate->builtins()->builtin(builtin_id));
  shared->set_code(code);
  function->set_code(code);

  return code;
}

RUNTIME_FUNCTION(Runtime_IncrementUseCounter) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_SMI_ARG_CHECKED(counter, 0);
  isolate->CountUsage(static_cast<v8::Isolate::UseCounterFeature>(counter));
  return isolate->heap()->undefined_value();
}

RUNTIME_FUNCTION(
    Runtime_IncrementUseCounterConstructorReturnNonUndefinedPrimitive) {
  HandleScope scope(isolate);
  DCHECK_EQ(0, args.length());
  isolate->CountUsage(
      v8::Isolate::UseCounterFeature::kConstructorNonUndefinedPrimitiveReturn);
  return isolate->heap()->undefined_value();
}

RUNTIME_FUNCTION(Runtime_GetAndResetRuntimeCallStats) {
  HandleScope scope(isolate);
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
    if (args[0]->IsString()) {
      // With a string argument, the results are appended to that file.
      CONVERT_ARG_HANDLE_CHECKED(String, arg0, 0);
      String::FlatContent flat = arg0->GetFlatContent();
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
    if (args[0]->IsString())
      std::fclose(f);
    else
      std::fflush(f);
    return isolate->heap()->undefined_value();
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

  return *isolate->factory()->NewJSAsyncFromSyncIterator(
      Handle<JSReceiver>::cast(sync_iterator));
}

RUNTIME_FUNCTION(Runtime_GetTemplateObject) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(TemplateObjectDescription, description, 0);

  return *TemplateObjectDescription::GetTemplateObject(
      description, isolate->native_context());
}

}  // namespace internal
}  // namespace v8
