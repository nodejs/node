// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/messages.h"

#include <memory>

#include "src/api.h"
#include "src/execution.h"
#include "src/isolate-inl.h"
#include "src/keys.h"
#include "src/objects/frame-array-inl.h"
#include "src/string-builder.h"
#include "src/wasm/wasm-heap.h"
#include "src/wasm/wasm-objects.h"

namespace v8 {
namespace internal {

MessageLocation::MessageLocation(Handle<Script> script, int start_pos,
                                 int end_pos)
    : script_(script), start_pos_(start_pos), end_pos_(end_pos) {}
MessageLocation::MessageLocation(Handle<Script> script, int start_pos,
                                 int end_pos, Handle<SharedFunctionInfo> shared)
    : script_(script),
      start_pos_(start_pos),
      end_pos_(end_pos),
      shared_(shared) {}
MessageLocation::MessageLocation() : start_pos_(-1), end_pos_(-1) {}

// If no message listeners have been registered this one is called
// by default.
void MessageHandler::DefaultMessageReport(Isolate* isolate,
                                          const MessageLocation* loc,
                                          Handle<Object> message_obj) {
  std::unique_ptr<char[]> str = GetLocalizedMessage(isolate, message_obj);
  if (loc == nullptr) {
    PrintF("%s\n", str.get());
  } else {
    HandleScope scope(isolate);
    Handle<Object> data(loc->script()->name(), isolate);
    std::unique_ptr<char[]> data_str;
    if (data->IsString())
      data_str = Handle<String>::cast(data)->ToCString(DISALLOW_NULLS);
    PrintF("%s:%i: %s\n", data_str.get() ? data_str.get() : "<unknown>",
           loc->start_pos(), str.get());
  }
}

Handle<JSMessageObject> MessageHandler::MakeMessageObject(
    Isolate* isolate, MessageTemplate::Template message,
    const MessageLocation* location, Handle<Object> argument,
    Handle<FixedArray> stack_frames) {
  Factory* factory = isolate->factory();

  int start = -1;
  int end = -1;
  Handle<Object> script_handle = factory->undefined_value();
  if (location != nullptr) {
    start = location->start_pos();
    end = location->end_pos();
    script_handle = Script::GetWrapper(location->script());
  } else {
    script_handle = Script::GetWrapper(isolate->factory()->empty_script());
  }

  Handle<Object> stack_frames_handle = stack_frames.is_null()
      ? Handle<Object>::cast(factory->undefined_value())
      : Handle<Object>::cast(stack_frames);

  Handle<JSMessageObject> message_obj = factory->NewJSMessageObject(
      message, argument, start, end, script_handle, stack_frames_handle);

  return message_obj;
}

void MessageHandler::ReportMessage(Isolate* isolate, const MessageLocation* loc,
                                   Handle<JSMessageObject> message) {
  v8::Local<v8::Message> api_message_obj = v8::Utils::MessageToLocal(message);

  if (api_message_obj->ErrorLevel() == v8::Isolate::kMessageError) {
    // We are calling into embedder's code which can throw exceptions.
    // Thus we need to save current exception state, reset it to the clean one
    // and ignore scheduled exceptions callbacks can throw.

    // We pass the exception object into the message handler callback though.
    Object* exception_object = isolate->heap()->undefined_value();
    if (isolate->has_pending_exception()) {
      exception_object = isolate->pending_exception();
    }
    Handle<Object> exception(exception_object, isolate);

    Isolate::ExceptionScope exception_scope(isolate);
    isolate->clear_pending_exception();
    isolate->set_external_caught_exception(false);

    // Turn the exception on the message into a string if it is an object.
    if (message->argument()->IsJSObject()) {
      HandleScope scope(isolate);
      Handle<Object> argument(message->argument(), isolate);

      MaybeHandle<Object> maybe_stringified;
      Handle<Object> stringified;
      // Make sure we don't leak uncaught internally generated Error objects.
      if (argument->IsJSError()) {
        maybe_stringified = Object::NoSideEffectsToString(isolate, argument);
      } else {
        v8::TryCatch catcher(reinterpret_cast<v8::Isolate*>(isolate));
        catcher.SetVerbose(false);
        catcher.SetCaptureMessage(false);

        maybe_stringified = Object::ToString(isolate, argument);
      }

      if (!maybe_stringified.ToHandle(&stringified)) {
        DCHECK(isolate->has_pending_exception());
        isolate->clear_pending_exception();
        isolate->set_external_caught_exception(false);
        stringified =
            isolate->factory()->NewStringFromAsciiChecked("exception");
      }
      message->set_argument(*stringified);
    }

    v8::Local<v8::Value> api_exception_obj = v8::Utils::ToLocal(exception);
    ReportMessageNoExceptions(isolate, loc, message, api_exception_obj);
  } else {
    ReportMessageNoExceptions(isolate, loc, message, v8::Local<v8::Value>());
  }
}

void MessageHandler::ReportMessageNoExceptions(
    Isolate* isolate, const MessageLocation* loc, Handle<Object> message,
    v8::Local<v8::Value> api_exception_obj) {
  v8::Local<v8::Message> api_message_obj = v8::Utils::MessageToLocal(message);
  int error_level = api_message_obj->ErrorLevel();

  Handle<TemplateList> global_listeners =
      isolate->factory()->message_listeners();
  int global_length = global_listeners->length();
  if (global_length == 0) {
    DefaultMessageReport(isolate, loc, message);
    if (isolate->has_scheduled_exception()) {
      isolate->clear_scheduled_exception();
    }
  } else {
    for (int i = 0; i < global_length; i++) {
      HandleScope scope(isolate);
      if (global_listeners->get(i)->IsUndefined(isolate)) continue;
      FixedArray* listener = FixedArray::cast(global_listeners->get(i));
      Foreign* callback_obj = Foreign::cast(listener->get(0));
      int32_t message_levels =
          static_cast<int32_t>(Smi::ToInt(listener->get(2)));
      if (!(message_levels & error_level)) {
        continue;
      }
      v8::MessageCallback callback =
          FUNCTION_CAST<v8::MessageCallback>(callback_obj->foreign_address());
      Handle<Object> callback_data(listener->get(1), isolate);
      {
        // Do not allow exceptions to propagate.
        v8::TryCatch try_catch(reinterpret_cast<v8::Isolate*>(isolate));
        callback(api_message_obj, callback_data->IsUndefined(isolate)
                                      ? api_exception_obj
                                      : v8::Utils::ToLocal(callback_data));
      }
      if (isolate->has_scheduled_exception()) {
        isolate->clear_scheduled_exception();
      }
    }
  }
}


Handle<String> MessageHandler::GetMessage(Isolate* isolate,
                                          Handle<Object> data) {
  Handle<JSMessageObject> message = Handle<JSMessageObject>::cast(data);
  Handle<Object> arg = Handle<Object>(message->argument(), isolate);
  return MessageTemplate::FormatMessage(isolate, message->type(), arg);
}

std::unique_ptr<char[]> MessageHandler::GetLocalizedMessage(
    Isolate* isolate, Handle<Object> data) {
  HandleScope scope(isolate);
  return GetMessage(isolate, data)->ToCString(DISALLOW_NULLS);
}

namespace {

Object* EvalFromFunctionName(Isolate* isolate, Handle<Script> script) {
  if (script->eval_from_shared()->IsUndefined(isolate))
    return isolate->heap()->undefined_value();

  Handle<SharedFunctionInfo> shared(
      SharedFunctionInfo::cast(script->eval_from_shared()));
  // Find the name of the function calling eval.
  if (shared->name()->BooleanValue()) {
    return shared->name();
  }

  return shared->inferred_name();
}

Object* EvalFromScript(Isolate* isolate, Handle<Script> script) {
  if (script->eval_from_shared()->IsUndefined(isolate))
    return isolate->heap()->undefined_value();

  Handle<SharedFunctionInfo> eval_from_shared(
      SharedFunctionInfo::cast(script->eval_from_shared()));
  return eval_from_shared->script()->IsScript()
             ? eval_from_shared->script()
             : isolate->heap()->undefined_value();
}

MaybeHandle<String> FormatEvalOrigin(Isolate* isolate, Handle<Script> script) {
  Handle<Object> sourceURL(script->GetNameOrSourceURL(), isolate);
  if (!sourceURL->IsUndefined(isolate)) {
    DCHECK(sourceURL->IsString());
    return Handle<String>::cast(sourceURL);
  }

  IncrementalStringBuilder builder(isolate);
  builder.AppendCString("eval at ");

  Handle<Object> eval_from_function_name =
      handle(EvalFromFunctionName(isolate, script), isolate);
  if (eval_from_function_name->BooleanValue()) {
    Handle<String> str;
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, str, Object::ToString(isolate, eval_from_function_name),
        String);
    builder.AppendString(str);
  } else {
    builder.AppendCString("<anonymous>");
  }

  Handle<Object> eval_from_script_obj =
      handle(EvalFromScript(isolate, script), isolate);
  if (eval_from_script_obj->IsScript()) {
    Handle<Script> eval_from_script =
        Handle<Script>::cast(eval_from_script_obj);
    builder.AppendCString(" (");
    if (eval_from_script->compilation_type() == Script::COMPILATION_TYPE_EVAL) {
      // Eval script originated from another eval.
      Handle<String> str;
      ASSIGN_RETURN_ON_EXCEPTION(
          isolate, str, FormatEvalOrigin(isolate, eval_from_script), String);
      builder.AppendString(str);
    } else {
      DCHECK(eval_from_script->compilation_type() !=
             Script::COMPILATION_TYPE_EVAL);
      // eval script originated from "real" source.
      Handle<Object> name_obj = handle(eval_from_script->name(), isolate);
      if (eval_from_script->name()->IsString()) {
        builder.AppendString(Handle<String>::cast(name_obj));

        Script::PositionInfo info;
        if (Script::GetPositionInfo(eval_from_script, script->GetEvalPosition(),
                                    &info, Script::NO_OFFSET)) {
          builder.AppendCString(":");

          Handle<String> str = isolate->factory()->NumberToString(
              handle(Smi::FromInt(info.line + 1), isolate));
          builder.AppendString(str);

          builder.AppendCString(":");

          str = isolate->factory()->NumberToString(
              handle(Smi::FromInt(info.column + 1), isolate));
          builder.AppendString(str);
        }
      } else {
        DCHECK(!eval_from_script->name()->IsString());
        builder.AppendCString("unknown source");
      }
    }
    builder.AppendCString(")");
  }

  Handle<String> result;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, result, builder.Finish(), String);
  return result;
}

}  // namespace

Handle<Object> StackFrameBase::GetEvalOrigin() {
  if (!HasScript()) return isolate_->factory()->undefined_value();
  return FormatEvalOrigin(isolate_, GetScript()).ToHandleChecked();
}

bool StackFrameBase::IsEval() {
  return HasScript() &&
         GetScript()->compilation_type() == Script::COMPILATION_TYPE_EVAL;
}

void JSStackFrame::FromFrameArray(Isolate* isolate, Handle<FrameArray> array,
                                  int frame_ix) {
  DCHECK(!array->IsWasmFrame(frame_ix));
  isolate_ = isolate;
  receiver_ = handle(array->Receiver(frame_ix), isolate);
  function_ = handle(array->Function(frame_ix), isolate);
  code_ = handle(array->Code(frame_ix), isolate);
  offset_ = array->Offset(frame_ix)->value();

  const int flags = array->Flags(frame_ix)->value();
  is_constructor_ = (flags & FrameArray::kIsConstructor) != 0;
  is_strict_ = (flags & FrameArray::kIsStrict) != 0;
}

JSStackFrame::JSStackFrame() {}

JSStackFrame::JSStackFrame(Isolate* isolate, Handle<Object> receiver,
                           Handle<JSFunction> function,
                           Handle<AbstractCode> code, int offset)
    : StackFrameBase(isolate),
      receiver_(receiver),
      function_(function),
      code_(code),
      offset_(offset),
      is_constructor_(false),
      is_strict_(false) {}

Handle<Object> JSStackFrame::GetFunction() const {
  return Handle<Object>::cast(function_);
}

Handle<Object> JSStackFrame::GetFileName() {
  if (!HasScript()) return isolate_->factory()->null_value();
  return handle(GetScript()->name(), isolate_);
}

Handle<Object> JSStackFrame::GetFunctionName() {
  Handle<String> result = JSFunction::GetName(function_);
  if (result->length() != 0) return result;

  if (HasScript() &&
      GetScript()->compilation_type() == Script::COMPILATION_TYPE_EVAL) {
    return isolate_->factory()->eval_string();
  }
  return isolate_->factory()->null_value();
}

namespace {

bool CheckMethodName(Isolate* isolate, Handle<JSReceiver> receiver,
                     Handle<Name> name, Handle<JSFunction> fun,
                     LookupIterator::Configuration config) {
  LookupIterator iter =
      LookupIterator::PropertyOrElement(isolate, receiver, name, config);
  if (iter.state() == LookupIterator::DATA) {
    return iter.GetDataValue().is_identical_to(fun);
  } else if (iter.state() == LookupIterator::ACCESSOR) {
    Handle<Object> accessors = iter.GetAccessors();
    if (accessors->IsAccessorPair()) {
      Handle<AccessorPair> pair = Handle<AccessorPair>::cast(accessors);
      return pair->getter() == *fun || pair->setter() == *fun;
    }
  }
  return false;
}

Handle<Object> ScriptNameOrSourceUrl(Handle<Script> script, Isolate* isolate) {
  Object* name_or_url = script->source_url();
  if (!name_or_url->IsString()) name_or_url = script->name();
  return handle(name_or_url, isolate);
}

}  // namespace

Handle<Object> JSStackFrame::GetScriptNameOrSourceUrl() {
  if (!HasScript()) return isolate_->factory()->null_value();
  return ScriptNameOrSourceUrl(GetScript(), isolate_);
}

Handle<Object> JSStackFrame::GetMethodName() {
  if (receiver_->IsNullOrUndefined(isolate_)) {
    return isolate_->factory()->null_value();
  }

  Handle<JSReceiver> receiver;
  if (!Object::ToObject(isolate_, receiver_).ToHandle(&receiver)) {
    DCHECK(isolate_->has_pending_exception());
    isolate_->clear_pending_exception();
    isolate_->set_external_caught_exception(false);
    return isolate_->factory()->null_value();
  }

  Handle<String> name(function_->shared()->name(), isolate_);
  // ES2015 gives getters and setters name prefixes which must
  // be stripped to find the property name.
  if (name->IsUtf8EqualTo(CStrVector("get "), true) ||
      name->IsUtf8EqualTo(CStrVector("set "), true)) {
    name = isolate_->factory()->NewProperSubString(name, 4, name->length());
  }
  if (CheckMethodName(isolate_, receiver, name, function_,
                      LookupIterator::PROTOTYPE_CHAIN_SKIP_INTERCEPTOR)) {
    return name;
  }

  HandleScope outer_scope(isolate_);
  Handle<Object> result;
  for (PrototypeIterator iter(isolate_, receiver, kStartAtReceiver);
       !iter.IsAtEnd(); iter.Advance()) {
    Handle<Object> current = PrototypeIterator::GetCurrent(iter);
    if (!current->IsJSObject()) break;
    Handle<JSObject> current_obj = Handle<JSObject>::cast(current);
    if (current_obj->IsAccessCheckNeeded()) break;
    Handle<FixedArray> keys =
        KeyAccumulator::GetOwnEnumPropertyKeys(isolate_, current_obj);
    for (int i = 0; i < keys->length(); i++) {
      HandleScope inner_scope(isolate_);
      if (!keys->get(i)->IsName()) continue;
      Handle<Name> name_key(Name::cast(keys->get(i)), isolate_);
      if (!CheckMethodName(isolate_, current_obj, name_key, function_,
                           LookupIterator::OWN_SKIP_INTERCEPTOR))
        continue;
      // Return null in case of duplicates to avoid confusion.
      if (!result.is_null()) return isolate_->factory()->null_value();
      result = inner_scope.CloseAndEscape(name_key);
    }
  }

  if (!result.is_null()) return outer_scope.CloseAndEscape(result);
  return isolate_->factory()->null_value();
}

Handle<Object> JSStackFrame::GetTypeName() {
  // TODO(jgruber): Check for strict/constructor here as in
  // CallSitePrototypeGetThis.

  if (receiver_->IsNullOrUndefined(isolate_)) {
    return isolate_->factory()->null_value();
  } else if (receiver_->IsJSProxy()) {
    return isolate_->factory()->Proxy_string();
  }

  Handle<JSReceiver> receiver;
  if (!Object::ToObject(isolate_, receiver_).ToHandle(&receiver)) {
    DCHECK(isolate_->has_pending_exception());
    isolate_->clear_pending_exception();
    isolate_->set_external_caught_exception(false);
    return isolate_->factory()->null_value();
  }

  return JSReceiver::GetConstructorName(receiver);
}

int JSStackFrame::GetLineNumber() {
  DCHECK_LE(0, GetPosition());
  if (HasScript()) return Script::GetLineNumber(GetScript(), GetPosition()) + 1;
  return -1;
}

int JSStackFrame::GetColumnNumber() {
  DCHECK_LE(0, GetPosition());
  if (HasScript()) {
    return Script::GetColumnNumber(GetScript(), GetPosition()) + 1;
  }
  return -1;
}

bool JSStackFrame::IsNative() {
  return HasScript() && GetScript()->type() == Script::TYPE_NATIVE;
}

bool JSStackFrame::IsToplevel() {
  return receiver_->IsJSGlobalProxy() || receiver_->IsNullOrUndefined(isolate_);
}

namespace {

bool IsNonEmptyString(Handle<Object> object) {
  return (object->IsString() && String::cast(*object)->length() > 0);
}

void AppendFileLocation(Isolate* isolate, StackFrameBase* call_site,
                        IncrementalStringBuilder* builder) {
  if (call_site->IsNative()) {
    builder->AppendCString("native");
    return;
  }

  Handle<Object> file_name = call_site->GetScriptNameOrSourceUrl();
  if (!file_name->IsString() && call_site->IsEval()) {
    Handle<Object> eval_origin = call_site->GetEvalOrigin();
    DCHECK(eval_origin->IsString());
    builder->AppendString(Handle<String>::cast(eval_origin));
    builder->AppendCString(", ");  // Expecting source position to follow.
  }

  if (IsNonEmptyString(file_name)) {
    builder->AppendString(Handle<String>::cast(file_name));
  } else {
    // Source code does not originate from a file and is not native, but we
    // can still get the source position inside the source string, e.g. in
    // an eval string.
    builder->AppendCString("<anonymous>");
  }

  int line_number = call_site->GetLineNumber();
  if (line_number != -1) {
    builder->AppendCharacter(':');
    Handle<String> line_string = isolate->factory()->NumberToString(
        handle(Smi::FromInt(line_number), isolate), isolate);
    builder->AppendString(line_string);

    int column_number = call_site->GetColumnNumber();
    if (column_number != -1) {
      builder->AppendCharacter(':');
      Handle<String> column_string = isolate->factory()->NumberToString(
          handle(Smi::FromInt(column_number), isolate), isolate);
      builder->AppendString(column_string);
    }
  }
}

int StringIndexOf(Isolate* isolate, Handle<String> subject,
                  Handle<String> pattern) {
  if (pattern->length() > subject->length()) return -1;
  return String::IndexOf(isolate, subject, pattern, 0);
}

// Returns true iff
// 1. the subject ends with '.' + pattern, or
// 2. subject == pattern.
bool StringEndsWithMethodName(Isolate* isolate, Handle<String> subject,
                              Handle<String> pattern) {
  if (String::Equals(subject, pattern)) return true;

  FlatStringReader subject_reader(isolate, String::Flatten(subject));
  FlatStringReader pattern_reader(isolate, String::Flatten(pattern));

  int pattern_index = pattern_reader.length() - 1;
  int subject_index = subject_reader.length() - 1;
  for (int i = 0; i <= pattern_reader.length(); i++) {  // Iterate over len + 1.
    if (subject_index < 0) {
      return false;
    }

    const uc32 subject_char = subject_reader.Get(subject_index);
    if (i == pattern_reader.length()) {
      if (subject_char != '.') return false;
    } else if (subject_char != pattern_reader.Get(pattern_index)) {
      return false;
    }

    pattern_index--;
    subject_index--;
  }

  return true;
}

void AppendMethodCall(Isolate* isolate, JSStackFrame* call_site,
                      IncrementalStringBuilder* builder) {
  Handle<Object> type_name = call_site->GetTypeName();
  Handle<Object> method_name = call_site->GetMethodName();
  Handle<Object> function_name = call_site->GetFunctionName();

  if (IsNonEmptyString(function_name)) {
    Handle<String> function_string = Handle<String>::cast(function_name);
    if (IsNonEmptyString(type_name)) {
      Handle<String> type_string = Handle<String>::cast(type_name);
      bool starts_with_type_name =
          (StringIndexOf(isolate, function_string, type_string) == 0);
      if (!starts_with_type_name) {
        builder->AppendString(type_string);
        builder->AppendCharacter('.');
      }
    }
    builder->AppendString(function_string);

    if (IsNonEmptyString(method_name)) {
      Handle<String> method_string = Handle<String>::cast(method_name);
      if (!StringEndsWithMethodName(isolate, function_string, method_string)) {
        builder->AppendCString(" [as ");
        builder->AppendString(method_string);
        builder->AppendCharacter(']');
      }
    }
  } else {
    if (IsNonEmptyString(type_name)) {
      builder->AppendString(Handle<String>::cast(type_name));
      builder->AppendCharacter('.');
    }
    if (IsNonEmptyString(method_name)) {
      builder->AppendString(Handle<String>::cast(method_name));
    } else {
      builder->AppendCString("<anonymous>");
    }
  }
}

}  // namespace

MaybeHandle<String> JSStackFrame::ToString() {
  IncrementalStringBuilder builder(isolate_);

  Handle<Object> function_name = GetFunctionName();

  const bool is_toplevel = IsToplevel();
  const bool is_constructor = IsConstructor();
  const bool is_method_call = !(is_toplevel || is_constructor);

  if (is_method_call) {
    AppendMethodCall(isolate_, this, &builder);
  } else if (is_constructor) {
    builder.AppendCString("new ");
    if (IsNonEmptyString(function_name)) {
      builder.AppendString(Handle<String>::cast(function_name));
    } else {
      builder.AppendCString("<anonymous>");
    }
  } else if (IsNonEmptyString(function_name)) {
    builder.AppendString(Handle<String>::cast(function_name));
  } else {
    AppendFileLocation(isolate_, this, &builder);
    return builder.Finish();
  }

  builder.AppendCString(" (");
  AppendFileLocation(isolate_, this, &builder);
  builder.AppendCString(")");

  return builder.Finish();
}

int JSStackFrame::GetPosition() const { return code_->SourcePosition(offset_); }

bool JSStackFrame::HasScript() const {
  return function_->shared()->script()->IsScript();
}

Handle<Script> JSStackFrame::GetScript() const {
  return handle(Script::cast(function_->shared()->script()), isolate_);
}

WasmStackFrame::WasmStackFrame() {}

void WasmStackFrame::FromFrameArray(Isolate* isolate, Handle<FrameArray> array,
                                    int frame_ix) {
  // This function is called for compiled and interpreted wasm frames, and for
  // asm.js->wasm frames.
  DCHECK(array->IsWasmFrame(frame_ix) ||
         array->IsWasmInterpretedFrame(frame_ix) ||
         array->IsAsmJsWasmFrame(frame_ix));
  isolate_ = isolate;
  wasm_instance_ = handle(array->WasmInstance(frame_ix), isolate);
  wasm_func_index_ = array->WasmFunctionIndex(frame_ix)->value();
  if (array->IsWasmInterpretedFrame(frame_ix)) {
    code_ = {};
  } else {
    code_ =
        FLAG_wasm_jit_to_native
            ? WasmCodeWrapper(
                  wasm_instance_->compiled_module()->GetNativeModule()->GetCode(
                      wasm_func_index_))
            : WasmCodeWrapper(handle(
                  Code::cast(
                      wasm_instance_->compiled_module()->code_table()->get(
                          wasm_func_index_)),
                  isolate));
  }
  offset_ = array->Offset(frame_ix)->value();
}

Handle<Object> WasmStackFrame::GetReceiver() const { return wasm_instance_; }

Handle<Object> WasmStackFrame::GetFunction() const {
  return handle(Smi::FromInt(wasm_func_index_), isolate_);
}

Handle<Object> WasmStackFrame::GetFunctionName() {
  Handle<Object> name;
  Handle<WasmCompiledModule> compiled_module(wasm_instance_->compiled_module(),
                                             isolate_);
  if (!WasmCompiledModule::GetFunctionNameOrNull(isolate_, compiled_module,
                                                 wasm_func_index_)
           .ToHandle(&name)) {
    name = isolate_->factory()->null_value();
  }
  return name;
}

MaybeHandle<String> WasmStackFrame::ToString() {
  IncrementalStringBuilder builder(isolate_);

  Handle<WasmCompiledModule> compiled_module(wasm_instance_->compiled_module(),
                                             isolate_);
  MaybeHandle<String> module_name =
      WasmCompiledModule::GetModuleNameOrNull(isolate_, compiled_module);
  MaybeHandle<String> function_name = WasmCompiledModule::GetFunctionNameOrNull(
      isolate_, compiled_module, wasm_func_index_);
  bool has_name = !module_name.is_null() || !function_name.is_null();
  if (has_name) {
    if (module_name.is_null()) {
      builder.AppendString(function_name.ToHandleChecked());
    } else {
      builder.AppendString(module_name.ToHandleChecked());
      if (!function_name.is_null()) {
        builder.AppendCString(".");
        builder.AppendString(function_name.ToHandleChecked());
      }
    }
    builder.AppendCString(" (");
  }

  builder.AppendCString("wasm-function[");

  char buffer[16];
  SNPrintF(ArrayVector(buffer), "%u]", wasm_func_index_);
  builder.AppendCString(buffer);

  SNPrintF(ArrayVector(buffer), ":%d", GetPosition());
  builder.AppendCString(buffer);

  if (has_name) builder.AppendCString(")");

  return builder.Finish();
}

int WasmStackFrame::GetPosition() const {
  return IsInterpreted()
             ? offset_
             : (code_.IsCodeObject()
                    ? Handle<AbstractCode>::cast(code_.GetCode())
                          ->SourcePosition(offset_)
                    : FrameSummary::WasmCompiledFrameSummary::
                          GetWasmSourcePosition(code_.GetWasmCode(), offset_));
}

Handle<Object> WasmStackFrame::Null() const {
  return isolate_->factory()->null_value();
}

bool WasmStackFrame::HasScript() const { return true; }

Handle<Script> WasmStackFrame::GetScript() const {
  return handle(wasm_instance_->compiled_module()->script(), isolate_);
}

AsmJsWasmStackFrame::AsmJsWasmStackFrame() {}

void AsmJsWasmStackFrame::FromFrameArray(Isolate* isolate,
                                         Handle<FrameArray> array,
                                         int frame_ix) {
  DCHECK(array->IsAsmJsWasmFrame(frame_ix));
  WasmStackFrame::FromFrameArray(isolate, array, frame_ix);
  is_at_number_conversion_ =
      array->Flags(frame_ix)->value() & FrameArray::kAsmJsAtNumberConversion;
}

Handle<Object> AsmJsWasmStackFrame::GetReceiver() const {
  return isolate_->global_proxy();
}

Handle<Object> AsmJsWasmStackFrame::GetFunction() const {
  // TODO(clemensh): Return lazily created JSFunction.
  return Null();
}

Handle<Object> AsmJsWasmStackFrame::GetFileName() {
  Handle<Script> script(wasm_instance_->compiled_module()->script(), isolate_);
  DCHECK(script->IsUserJavaScript());
  return handle(script->name(), isolate_);
}

Handle<Object> AsmJsWasmStackFrame::GetScriptNameOrSourceUrl() {
  Handle<Script> script(wasm_instance_->compiled_module()->script(), isolate_);
  DCHECK_EQ(Script::TYPE_NORMAL, script->type());
  return ScriptNameOrSourceUrl(script, isolate_);
}

int AsmJsWasmStackFrame::GetPosition() const {
  DCHECK_LE(0, offset_);
  int byte_offset =
      code_.IsCodeObject()
          ? Handle<AbstractCode>::cast(code_.GetCode())->SourcePosition(offset_)
          : FrameSummary::WasmCompiledFrameSummary::GetWasmSourcePosition(
                code_.GetWasmCode(), offset_);
  Handle<WasmCompiledModule> compiled_module(wasm_instance_->compiled_module(),
                                             isolate_);
  DCHECK_LE(0, byte_offset);
  return WasmCompiledModule::GetSourcePosition(
      compiled_module, wasm_func_index_, static_cast<uint32_t>(byte_offset),
      is_at_number_conversion_);
}

int AsmJsWasmStackFrame::GetLineNumber() {
  DCHECK_LE(0, GetPosition());
  Handle<Script> script(wasm_instance_->compiled_module()->script(), isolate_);
  DCHECK(script->IsUserJavaScript());
  return Script::GetLineNumber(script, GetPosition()) + 1;
}

int AsmJsWasmStackFrame::GetColumnNumber() {
  DCHECK_LE(0, GetPosition());
  Handle<Script> script(wasm_instance_->compiled_module()->script(), isolate_);
  DCHECK(script->IsUserJavaScript());
  return Script::GetColumnNumber(script, GetPosition()) + 1;
}

MaybeHandle<String> AsmJsWasmStackFrame::ToString() {
  // The string should look exactly as the respective javascript frame string.
  // Keep this method in line to JSStackFrame::ToString().

  IncrementalStringBuilder builder(isolate_);

  Handle<Object> function_name = GetFunctionName();

  if (IsNonEmptyString(function_name)) {
    builder.AppendString(Handle<String>::cast(function_name));
    builder.AppendCString(" (");
  }

  AppendFileLocation(isolate_, this, &builder);

  if (IsNonEmptyString(function_name)) builder.AppendCString(")");

  return builder.Finish();
}

FrameArrayIterator::FrameArrayIterator(Isolate* isolate,
                                       Handle<FrameArray> array, int frame_ix)
    : isolate_(isolate), array_(array), next_frame_ix_(frame_ix) {}

bool FrameArrayIterator::HasNext() const {
  return (next_frame_ix_ < array_->FrameCount());
}

void FrameArrayIterator::Next() { next_frame_ix_++; }

StackFrameBase* FrameArrayIterator::Frame() {
  DCHECK(HasNext());
  const int flags = array_->Flags(next_frame_ix_)->value();
  int flag_mask = FrameArray::kIsWasmFrame |
                  FrameArray::kIsWasmInterpretedFrame |
                  FrameArray::kIsAsmJsWasmFrame;
  switch (flags & flag_mask) {
    case 0:
      // JavaScript Frame.
      js_frame_.FromFrameArray(isolate_, array_, next_frame_ix_);
      return &js_frame_;
    case FrameArray::kIsWasmFrame:
    case FrameArray::kIsWasmInterpretedFrame:
      // Wasm Frame:
      wasm_frame_.FromFrameArray(isolate_, array_, next_frame_ix_);
      return &wasm_frame_;
    case FrameArray::kIsAsmJsWasmFrame:
      // Asm.js Wasm Frame:
      asm_wasm_frame_.FromFrameArray(isolate_, array_, next_frame_ix_);
      return &asm_wasm_frame_;
    default:
      UNREACHABLE();
  }
}

namespace {

MaybeHandle<Object> ConstructCallSite(Isolate* isolate,
                                      Handle<FrameArray> frame_array,
                                      int frame_index) {
  Handle<JSFunction> target =
      handle(isolate->native_context()->callsite_function(), isolate);

  Handle<JSObject> obj;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, obj, JSObject::New(target, target),
                             Object);

  Handle<Symbol> key = isolate->factory()->call_site_frame_array_symbol();
  RETURN_ON_EXCEPTION(isolate, JSObject::SetOwnPropertyIgnoreAttributes(
                                   obj, key, frame_array, DONT_ENUM),
                      Object);

  key = isolate->factory()->call_site_frame_index_symbol();
  Handle<Object> value(Smi::FromInt(frame_index), isolate);
  RETURN_ON_EXCEPTION(isolate, JSObject::SetOwnPropertyIgnoreAttributes(
                                   obj, key, value, DONT_ENUM),
                      Object);

  return obj;
}

// Convert the raw frames as written by Isolate::CaptureSimpleStackTrace into
// a JSArray of JSCallSite objects.
MaybeHandle<JSArray> GetStackFrames(Isolate* isolate,
                                    Handle<FrameArray> elems) {
  const int frame_count = elems->FrameCount();

  Handle<FixedArray> frames = isolate->factory()->NewFixedArray(frame_count);
  for (int i = 0; i < frame_count; i++) {
    Handle<Object> site;
    ASSIGN_RETURN_ON_EXCEPTION(isolate, site,
                               ConstructCallSite(isolate, elems, i), JSArray);
    frames->set(i, *site);
  }

  return isolate->factory()->NewJSArrayWithElements(frames);
}

MaybeHandle<Object> AppendErrorString(Isolate* isolate, Handle<Object> error,
                                      IncrementalStringBuilder* builder) {
  MaybeHandle<String> err_str =
      ErrorUtils::ToString(isolate, Handle<Object>::cast(error));
  if (err_str.is_null()) {
    // Error.toString threw. Try to return a string representation of the thrown
    // exception instead.

    DCHECK(isolate->has_pending_exception());
    Handle<Object> pending_exception =
        handle(isolate->pending_exception(), isolate);
    isolate->clear_pending_exception();
    isolate->set_external_caught_exception(false);

    err_str = ErrorUtils::ToString(isolate, pending_exception);
    if (err_str.is_null()) {
      // Formatting the thrown exception threw again, give up.
      DCHECK(isolate->has_pending_exception());
      isolate->clear_pending_exception();
      isolate->set_external_caught_exception(false);
      builder->AppendCString("<error>");
    } else {
      // Formatted thrown exception successfully, append it.
      builder->AppendCString("<error: ");
      builder->AppendString(err_str.ToHandleChecked());
      builder->AppendCharacter('>');
    }
  } else {
    builder->AppendString(err_str.ToHandleChecked());
  }

  return error;
}

class PrepareStackTraceScope {
 public:
  explicit PrepareStackTraceScope(Isolate* isolate) : isolate_(isolate) {
    DCHECK(!isolate_->formatting_stack_trace());
    isolate_->set_formatting_stack_trace(true);
  }

  ~PrepareStackTraceScope() { isolate_->set_formatting_stack_trace(false); }

 private:
  Isolate* isolate_;

  DISALLOW_COPY_AND_ASSIGN(PrepareStackTraceScope);
};

}  // namespace

// static
MaybeHandle<Object> ErrorUtils::FormatStackTrace(Isolate* isolate,
                                                 Handle<JSObject> error,
                                                 Handle<Object> raw_stack) {
  DCHECK(raw_stack->IsJSArray());
  Handle<JSArray> raw_stack_array = Handle<JSArray>::cast(raw_stack);

  DCHECK(raw_stack_array->elements()->IsFixedArray());
  Handle<FrameArray> elems(FrameArray::cast(raw_stack_array->elements()));

  // If there's a user-specified "prepareStackFrames" function, call it on the
  // frames and use its result.

  Handle<JSFunction> global_error = isolate->error_function();
  Handle<Object> prepare_stack_trace;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, prepare_stack_trace,
      JSFunction::GetProperty(isolate, global_error, "prepareStackTrace"),
      Object);

  const bool in_recursion = isolate->formatting_stack_trace();
  if (prepare_stack_trace->IsJSFunction() && !in_recursion) {
    PrepareStackTraceScope scope(isolate);

    isolate->CountUsage(v8::Isolate::kErrorPrepareStackTrace);

    Handle<JSArray> sites;
    ASSIGN_RETURN_ON_EXCEPTION(isolate, sites, GetStackFrames(isolate, elems),
                               Object);

    const int argc = 2;
    ScopedVector<Handle<Object>> argv(argc);

    argv[0] = error;
    argv[1] = sites;

    Handle<Object> result;
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, result, Execution::Call(isolate, prepare_stack_trace,
                                         global_error, argc, argv.start()),
        Object);

    return result;
  }

  // Otherwise, run our internal formatting logic.

  IncrementalStringBuilder builder(isolate);

  RETURN_ON_EXCEPTION(isolate, AppendErrorString(isolate, error, &builder),
                      Object);

  for (FrameArrayIterator it(isolate, elems); it.HasNext(); it.Next()) {
    builder.AppendCString("\n    at ");

    StackFrameBase* frame = it.Frame();
    MaybeHandle<String> maybe_frame_string = frame->ToString();
    if (maybe_frame_string.is_null()) {
      // CallSite.toString threw. Try to return a string representation of the
      // thrown exception instead.

      DCHECK(isolate->has_pending_exception());
      Handle<Object> pending_exception =
          handle(isolate->pending_exception(), isolate);
      isolate->clear_pending_exception();
      isolate->set_external_caught_exception(false);

      maybe_frame_string = ErrorUtils::ToString(isolate, pending_exception);
      if (maybe_frame_string.is_null()) {
        // Formatting the thrown exception threw again, give up.

        builder.AppendCString("<error>");
      } else {
        // Formatted thrown exception successfully, append it.
        builder.AppendCString("<error: ");
        builder.AppendString(maybe_frame_string.ToHandleChecked());
        builder.AppendCString("<error>");
      }
    } else {
      // CallSite.toString completed without throwing.
      builder.AppendString(maybe_frame_string.ToHandleChecked());
    }
  }

  return builder.Finish();
}

Handle<String> MessageTemplate::FormatMessage(Isolate* isolate,
                                              int template_index,
                                              Handle<Object> arg) {
  Factory* factory = isolate->factory();
  Handle<String> result_string = Object::NoSideEffectsToString(isolate, arg);
  MaybeHandle<String> maybe_result_string = MessageTemplate::FormatMessage(
      template_index, result_string, factory->empty_string(),
      factory->empty_string());
  if (!maybe_result_string.ToHandle(&result_string)) {
    DCHECK(isolate->has_pending_exception());
    isolate->clear_pending_exception();
    return factory->InternalizeOneByteString(STATIC_CHAR_VECTOR("<error>"));
  }
  // A string that has been obtained from JS code in this way is
  // likely to be a complicated ConsString of some sort.  We flatten it
  // here to improve the efficiency of converting it to a C string and
  // other operations that are likely to take place (see GetLocalizedMessage
  // for example).
  return String::Flatten(result_string);
}


const char* MessageTemplate::TemplateString(int template_index) {
  switch (template_index) {
#define CASE(NAME, STRING) \
  case k##NAME:            \
    return STRING;
    MESSAGE_TEMPLATES(CASE)
#undef CASE
    case kLastMessage:
    default:
      return nullptr;
  }
}


MaybeHandle<String> MessageTemplate::FormatMessage(int template_index,
                                                   Handle<String> arg0,
                                                   Handle<String> arg1,
                                                   Handle<String> arg2) {
  Isolate* isolate = arg0->GetIsolate();
  const char* template_string = TemplateString(template_index);
  if (template_string == nullptr) {
    isolate->ThrowIllegalOperation();
    return MaybeHandle<String>();
  }

  IncrementalStringBuilder builder(isolate);

  unsigned int i = 0;
  Handle<String> args[] = {arg0, arg1, arg2};
  for (const char* c = template_string; *c != '\0'; c++) {
    if (*c == '%') {
      // %% results in verbatim %.
      if (*(c + 1) == '%') {
        c++;
        builder.AppendCharacter('%');
      } else {
        DCHECK(i < arraysize(args));
        Handle<String> arg = args[i++];
        builder.AppendString(arg);
      }
    } else {
      builder.AppendCharacter(*c);
    }
  }

  return builder.Finish();
}

MaybeHandle<Object> ErrorUtils::Construct(
    Isolate* isolate, Handle<JSFunction> target, Handle<Object> new_target,
    Handle<Object> message, FrameSkipMode mode, Handle<Object> caller,
    bool suppress_detailed_trace) {
  // 1. If NewTarget is undefined, let newTarget be the active function object,
  // else let newTarget be NewTarget.

  Handle<JSReceiver> new_target_recv =
      new_target->IsJSReceiver() ? Handle<JSReceiver>::cast(new_target)
                                 : Handle<JSReceiver>::cast(target);

  // 2. Let O be ? OrdinaryCreateFromConstructor(newTarget, "%ErrorPrototype%",
  //    « [[ErrorData]] »).
  Handle<JSObject> err;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, err,
                             JSObject::New(target, new_target_recv), Object);

  // 3. If message is not undefined, then
  //  a. Let msg be ? ToString(message).
  //  b. Let msgDesc be the PropertyDescriptor{[[Value]]: msg, [[Writable]]:
  //     true, [[Enumerable]]: false, [[Configurable]]: true}.
  //  c. Perform ! DefinePropertyOrThrow(O, "message", msgDesc).
  // 4. Return O.

  if (!message->IsUndefined(isolate)) {
    Handle<String> msg_string;
    ASSIGN_RETURN_ON_EXCEPTION(isolate, msg_string,
                               Object::ToString(isolate, message), Object);
    RETURN_ON_EXCEPTION(isolate, JSObject::SetOwnPropertyIgnoreAttributes(
                                     err, isolate->factory()->message_string(),
                                     msg_string, DONT_ENUM),
                        Object);
  }

  // Optionally capture a more detailed stack trace for the message.
  if (!suppress_detailed_trace) {
    RETURN_ON_EXCEPTION(isolate, isolate->CaptureAndSetDetailedStackTrace(err),
                        Object);
  }

  // Capture a simple stack trace for the stack property.
  RETURN_ON_EXCEPTION(isolate,
                      isolate->CaptureAndSetSimpleStackTrace(err, mode, caller),
                      Object);

  return err;
}

namespace {

MaybeHandle<String> GetStringPropertyOrDefault(Isolate* isolate,
                                               Handle<JSReceiver> recv,
                                               Handle<String> key,
                                               Handle<String> default_str) {
  Handle<Object> obj;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, obj, JSObject::GetProperty(recv, key),
                             String);

  Handle<String> str;
  if (obj->IsUndefined(isolate)) {
    str = default_str;
  } else {
    ASSIGN_RETURN_ON_EXCEPTION(isolate, str, Object::ToString(isolate, obj),
                               String);
  }

  return str;
}

}  // namespace

// ES6 section 19.5.3.4 Error.prototype.toString ( )
MaybeHandle<String> ErrorUtils::ToString(Isolate* isolate,
                                         Handle<Object> receiver) {
  // 1. Let O be the this value.
  // 2. If Type(O) is not Object, throw a TypeError exception.
  if (!receiver->IsJSReceiver()) {
    return isolate->Throw<String>(isolate->factory()->NewTypeError(
        MessageTemplate::kIncompatibleMethodReceiver,
        isolate->factory()->NewStringFromAsciiChecked(
            "Error.prototype.toString"),
        receiver));
  }
  Handle<JSReceiver> recv = Handle<JSReceiver>::cast(receiver);

  // 3. Let name be ? Get(O, "name").
  // 4. If name is undefined, let name be "Error"; otherwise let name be
  // ? ToString(name).
  Handle<String> name_key = isolate->factory()->name_string();
  Handle<String> name_default = isolate->factory()->Error_string();
  Handle<String> name;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, name,
      GetStringPropertyOrDefault(isolate, recv, name_key, name_default),
      String);

  // 5. Let msg be ? Get(O, "message").
  // 6. If msg is undefined, let msg be the empty String; otherwise let msg be
  // ? ToString(msg).
  Handle<String> msg_key = isolate->factory()->message_string();
  Handle<String> msg_default = isolate->factory()->empty_string();
  Handle<String> msg;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, msg,
      GetStringPropertyOrDefault(isolate, recv, msg_key, msg_default), String);

  // 7. If name is the empty String, return msg.
  // 8. If msg is the empty String, return name.
  if (name->length() == 0) return msg;
  if (msg->length() == 0) return name;

  // 9. Return the result of concatenating name, the code unit 0x003A (COLON),
  // the code unit 0x0020 (SPACE), and msg.
  IncrementalStringBuilder builder(isolate);
  builder.AppendString(name);
  builder.AppendCString(": ");
  builder.AppendString(msg);

  Handle<String> result;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, result, builder.Finish(), String);
  return result;
}

namespace {

Handle<String> FormatMessage(Isolate* isolate, int template_index,
                             Handle<Object> arg0, Handle<Object> arg1,
                             Handle<Object> arg2) {
  Handle<String> arg0_str = Object::NoSideEffectsToString(isolate, arg0);
  Handle<String> arg1_str = Object::NoSideEffectsToString(isolate, arg1);
  Handle<String> arg2_str = Object::NoSideEffectsToString(isolate, arg2);

  isolate->native_context()->IncrementErrorsThrown();

  Handle<String> msg;
  if (!MessageTemplate::FormatMessage(template_index, arg0_str, arg1_str,
                                      arg2_str)
           .ToHandle(&msg)) {
    DCHECK(isolate->has_pending_exception());
    isolate->clear_pending_exception();
    isolate->set_external_caught_exception(false);
    return isolate->factory()->NewStringFromAsciiChecked("<error>");
  }

  return msg;
}

}  // namespace

// static
MaybeHandle<Object> ErrorUtils::MakeGenericError(
    Isolate* isolate, Handle<JSFunction> constructor, int template_index,
    Handle<Object> arg0, Handle<Object> arg1, Handle<Object> arg2,
    FrameSkipMode mode) {
  if (FLAG_clear_exceptions_on_js_entry) {
    // This function used to be implemented in JavaScript, and JSEntryStub
    // clears
    // any pending exceptions - so whenever we'd call this from C++, pending
    // exceptions would be cleared. Preserve this behavior.
    isolate->clear_pending_exception();
  }

  DCHECK(mode != SKIP_UNTIL_SEEN);

  Handle<Object> no_caller;
  Handle<String> msg = FormatMessage(isolate, template_index, arg0, arg1, arg2);
  return ErrorUtils::Construct(isolate, constructor, constructor, msg, mode,
                               no_caller, false);
}

}  // namespace internal
}  // namespace v8
