// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/execution/messages.h"

#include <memory>

#include "src/api/api-inl.h"
#include "src/ast/ast.h"
#include "src/ast/prettyprinter.h"
#include "src/base/v8-fallthrough.h"
#include "src/execution/execution.h"
#include "src/execution/frames-inl.h"
#include "src/execution/frames.h"
#include "src/execution/isolate-inl.h"
#include "src/logging/counters.h"
#include "src/objects/foreign-inl.h"
#include "src/objects/frame-array-inl.h"
#include "src/objects/js-array-inl.h"
#include "src/objects/keys.h"
#include "src/objects/stack-frame-info-inl.h"
#include "src/objects/struct-inl.h"
#include "src/parsing/parse-info.h"
#include "src/parsing/parsing.h"
#include "src/roots/roots.h"
#include "src/strings/string-builder-inl.h"
#include "src/wasm/wasm-code-manager.h"
#include "src/wasm/wasm-objects.h"

namespace v8 {
namespace internal {

MessageLocation::MessageLocation(Handle<Script> script, int start_pos,
                                 int end_pos)
    : script_(script),
      start_pos_(start_pos),
      end_pos_(end_pos),
      bytecode_offset_(-1) {}

MessageLocation::MessageLocation(Handle<Script> script, int start_pos,
                                 int end_pos, Handle<SharedFunctionInfo> shared)
    : script_(script),
      start_pos_(start_pos),
      end_pos_(end_pos),
      bytecode_offset_(-1),
      shared_(shared) {}

MessageLocation::MessageLocation(Handle<Script> script,
                                 Handle<SharedFunctionInfo> shared,
                                 int bytecode_offset)
    : script_(script),
      start_pos_(-1),
      end_pos_(-1),
      bytecode_offset_(bytecode_offset),
      shared_(shared) {}

MessageLocation::MessageLocation()
    : start_pos_(-1), end_pos_(-1), bytecode_offset_(-1) {}

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
    Isolate* isolate, MessageTemplate message, const MessageLocation* location,
    Handle<Object> argument, Handle<FixedArray> stack_frames) {
  Factory* factory = isolate->factory();

  int start = -1;
  int end = -1;
  int bytecode_offset = -1;
  Handle<Script> script_handle = isolate->factory()->empty_script();
  Handle<SharedFunctionInfo> shared_info;
  if (location != nullptr && !FLAG_correctness_fuzzer_suppressions) {
    start = location->start_pos();
    end = location->end_pos();
    script_handle = location->script();
    bytecode_offset = location->bytecode_offset();
    shared_info = location->shared();
  }

  Handle<Object> stack_frames_handle =
      stack_frames.is_null() ? Handle<Object>::cast(factory->undefined_value())
                             : Handle<Object>::cast(stack_frames);

  Handle<JSMessageObject> message_obj = factory->NewJSMessageObject(
      message, argument, start, end, shared_info, bytecode_offset,
      script_handle, stack_frames_handle);

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
    Object exception_object = ReadOnlyRoots(isolate).undefined_value();
    if (isolate->has_pending_exception()) {
      exception_object = isolate->pending_exception();
    }
    Handle<Object> exception(exception_object, isolate);

    Isolate::ExceptionScope exception_scope(isolate);
    isolate->clear_pending_exception();
    isolate->set_external_caught_exception(false);

    // Turn the exception on the message into a string if it is an object.
    if (message->argument().IsJSObject()) {
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
      if (global_listeners->get(i).IsUndefined(isolate)) continue;
      FixedArray listener = FixedArray::cast(global_listeners->get(i));
      Foreign callback_obj = Foreign::cast(listener.get(0));
      int32_t message_levels =
          static_cast<int32_t>(Smi::ToInt(listener.get(2)));
      if (!(message_levels & error_level)) {
        continue;
      }
      v8::MessageCallback callback =
          FUNCTION_CAST<v8::MessageCallback>(callback_obj.foreign_address());
      Handle<Object> callback_data(listener.get(1), isolate);
      {
        RuntimeCallTimerScope timer(
            isolate, RuntimeCallCounterId::kMessageListenerCallback);
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
  return MessageFormatter::Format(isolate, message->type(), arg);
}

std::unique_ptr<char[]> MessageHandler::GetLocalizedMessage(
    Isolate* isolate, Handle<Object> data) {
  HandleScope scope(isolate);
  return GetMessage(isolate, data)->ToCString(DISALLOW_NULLS);
}

namespace {

Object EvalFromFunctionName(Isolate* isolate, Handle<Script> script) {
  if (!script->has_eval_from_shared()) {
    return ReadOnlyRoots(isolate).undefined_value();
  }

  Handle<SharedFunctionInfo> shared(script->eval_from_shared(), isolate);
  // Find the name of the function calling eval.
  if (shared->Name().BooleanValue(isolate)) {
    return shared->Name();
  }

  return shared->inferred_name();
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
  if (eval_from_function_name->BooleanValue(isolate)) {
    Handle<String> str;
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, str, Object::ToString(isolate, eval_from_function_name),
        String);
    builder.AppendString(str);
  } else {
    builder.AppendCString("<anonymous>");
  }

  if (script->has_eval_from_shared()) {
    Handle<SharedFunctionInfo> eval_from_shared(script->eval_from_shared(),
                                                isolate);
    if (eval_from_shared->script().IsScript()) {
      Handle<Script> eval_from_script =
          handle(Script::cast(eval_from_shared->script()), isolate);
      builder.AppendCString(" (");
      if (eval_from_script->compilation_type() ==
          Script::COMPILATION_TYPE_EVAL) {
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
        if (eval_from_script->name().IsString()) {
          builder.AppendString(Handle<String>::cast(name_obj));

          Script::PositionInfo info;

          if (Script::GetPositionInfo(eval_from_script,
                                      Script::GetEvalPosition(isolate, script),
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
          DCHECK(!eval_from_script->name().IsString());
          builder.AppendCString("unknown source");
        }
      }
    }
    builder.AppendCString(")");
  }

  Handle<String> result;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, result, builder.Finish(), String);
  return result;
}

}  // namespace

Handle<PrimitiveHeapObject> StackFrameBase::GetEvalOrigin() {
  if (!HasScript() || !IsEval()) return isolate_->factory()->undefined_value();
  return FormatEvalOrigin(isolate_, GetScript()).ToHandleChecked();
}

Handle<PrimitiveHeapObject> StackFrameBase::GetWasmModuleName() {
  return isolate_->factory()->undefined_value();
}

int StackFrameBase::GetWasmFunctionIndex() { return StackFrameBase::kNone; }

Handle<HeapObject> StackFrameBase::GetWasmInstance() {
  return isolate_->factory()->undefined_value();
}

int StackFrameBase::GetScriptId() const {
  if (!HasScript()) return kNone;
  return GetScript()->id();
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
  offset_ = array->Offset(frame_ix).value();
  cached_position_ = base::nullopt;

  const int flags = array->Flags(frame_ix).value();
  is_constructor_ = (flags & FrameArray::kIsConstructor) != 0;
  is_strict_ = (flags & FrameArray::kIsStrict) != 0;
  is_async_ = (flags & FrameArray::kIsAsync) != 0;
  is_promise_all_ = (flags & FrameArray::kIsPromiseAll) != 0;
  is_promise_any_ = (flags & FrameArray::kIsPromiseAny) != 0;
}

JSStackFrame::JSStackFrame(Isolate* isolate, Handle<Object> receiver,
                           Handle<JSFunction> function,
                           Handle<AbstractCode> code, int offset)
    : StackFrameBase(isolate),
      receiver_(receiver),
      function_(function),
      code_(code),
      offset_(offset),
      cached_position_(base::nullopt),
      is_async_(false),
      is_constructor_(false),
      is_strict_(false) {}

Handle<Object> JSStackFrame::GetFunction() const {
  return Handle<Object>::cast(function_);
}

Handle<Object> JSStackFrame::GetFileName() {
  if (!HasScript()) return isolate_->factory()->null_value();
  return handle(GetScript()->name(), isolate_);
}

Handle<PrimitiveHeapObject> JSStackFrame::GetFunctionName() {
  Handle<String> result = JSFunction::GetDebugName(function_);
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
  LookupIterator::Key key(isolate, name);
  LookupIterator iter(isolate, receiver, key, config);
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
  Object name_or_url = script->source_url();
  if (!name_or_url.IsString()) name_or_url = script->name();
  return handle(name_or_url, isolate);
}

}  // namespace

Handle<Object> JSStackFrame::GetScriptNameOrSourceUrl() {
  if (!HasScript()) return isolate_->factory()->null_value();
  return ScriptNameOrSourceUrl(GetScript(), isolate_);
}

Handle<PrimitiveHeapObject> JSStackFrame::GetMethodName() {
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

  Handle<String> name(function_->shared().Name(), isolate_);
  name = String::Flatten(isolate_, name);

  // The static initializer function is not a method, so don't add a
  // class name, just return the function name.
  if (name->HasOneBytePrefix(CStrVector("<static_fields_initializer>"))) {
    return name;
  }

  // ES2015 gives getters and setters name prefixes which must
  // be stripped to find the property name.
  if (name->HasOneBytePrefix(CStrVector("get ")) ||
      name->HasOneBytePrefix(CStrVector("set "))) {
    name = isolate_->factory()->NewProperSubString(name, 4, name->length());
  }
  if (CheckMethodName(isolate_, receiver, name, function_,
                      LookupIterator::PROTOTYPE_CHAIN_SKIP_INTERCEPTOR)) {
    return name;
  }

  HandleScope outer_scope(isolate_);
  Handle<PrimitiveHeapObject> result;
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
      if (!keys->get(i).IsName()) continue;
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

Handle<PrimitiveHeapObject> JSStackFrame::GetTypeName() {
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
  return kNone;
}

int JSStackFrame::GetColumnNumber() {
  DCHECK_LE(0, GetPosition());
  if (HasScript()) {
    return Script::GetColumnNumber(GetScript(), GetPosition()) + 1;
  }
  return kNone;
}

int JSStackFrame::GetEnclosingLineNumber() {
  if (HasScript()) {
    Handle<SharedFunctionInfo> shared = handle(function_->shared(), isolate_);
    return Script::GetLineNumber(GetScript(),
                                 shared->function_token_position()) + 1;
  } else {
    return kNone;
  }
}

int JSStackFrame::GetEnclosingColumnNumber() {
  if (HasScript()) {
    Handle<SharedFunctionInfo> shared = handle(function_->shared(), isolate_);
    return Script::GetColumnNumber(GetScript(),
                                   shared->function_token_position()) + 1;
  } else {
    return kNone;
  }
}

int JSStackFrame::GetPromiseIndex() const {
  return (is_promise_all_ || is_promise_any_) ? offset_ : kNone;
}

bool JSStackFrame::IsNative() {
  return HasScript() && GetScript()->type() == Script::TYPE_NATIVE;
}

bool JSStackFrame::IsToplevel() {
  return receiver_->IsJSGlobalProxy() || receiver_->IsNullOrUndefined(isolate_);
}

int JSStackFrame::GetPosition() const {
  if (cached_position_) return *cached_position_;

  Handle<SharedFunctionInfo> shared = handle(function_->shared(), isolate_);
  SharedFunctionInfo::EnsureSourcePositionsAvailable(isolate_, shared);
  cached_position_ = code_->SourcePosition(offset_);
  return *cached_position_;
}

bool JSStackFrame::HasScript() const {
  return function_->shared().script().IsScript();
}

Handle<Script> JSStackFrame::GetScript() const {
  return handle(Script::cast(function_->shared().script()), isolate_);
}

void WasmStackFrame::FromFrameArray(Isolate* isolate, Handle<FrameArray> array,
                                    int frame_ix) {
  // This function is called for compiled and interpreted wasm frames, and for
  // asm.js->wasm frames.
  DCHECK(array->IsWasmFrame(frame_ix) ||
         array->IsAsmJsWasmFrame(frame_ix));
  isolate_ = isolate;
  wasm_instance_ = handle(array->WasmInstance(frame_ix), isolate);
  wasm_func_index_ = array->WasmFunctionIndex(frame_ix).value();
  // The {WasmCode*} is held alive by the {GlobalWasmCodeRef}.
  auto global_wasm_code_ref =
      Managed<wasm::GlobalWasmCodeRef>::cast(array->WasmCodeObject(frame_ix));
  code_ = global_wasm_code_ref.get()->code();
  offset_ = array->Offset(frame_ix).value();
}

Handle<Object> WasmStackFrame::GetReceiver() const { return wasm_instance_; }

Handle<Object> WasmStackFrame::GetFunction() const {
  return handle(Smi::FromInt(wasm_func_index_), isolate_);
}

Handle<PrimitiveHeapObject> WasmStackFrame::GetFunctionName() {
  Handle<PrimitiveHeapObject> name;
  Handle<WasmModuleObject> module_object(wasm_instance_->module_object(),
                                         isolate_);
  if (!WasmModuleObject::GetFunctionNameOrNull(isolate_, module_object,
                                               wasm_func_index_)
           .ToHandle(&name)) {
    name = isolate_->factory()->null_value();
  }
  return name;
}

Handle<Object> WasmStackFrame::GetScriptNameOrSourceUrl() {
  Handle<Script> script = GetScript();
  DCHECK_EQ(Script::TYPE_WASM, script->type());
  return ScriptNameOrSourceUrl(script, isolate_);
}

Handle<PrimitiveHeapObject> WasmStackFrame::GetWasmModuleName() {
  Handle<PrimitiveHeapObject> module_name;
  Handle<WasmModuleObject> module_object(wasm_instance_->module_object(),
                                         isolate_);
  if (!WasmModuleObject::GetModuleNameOrNull(isolate_, module_object)
           .ToHandle(&module_name)) {
    module_name = isolate_->factory()->null_value();
  }
  return module_name;
}

Handle<HeapObject> WasmStackFrame::GetWasmInstance() { return wasm_instance_; }

int WasmStackFrame::GetPosition() const {
  return IsInterpreted() ? offset_ : code_->GetSourcePositionBefore(offset_);
}

int WasmStackFrame::GetColumnNumber() { return GetModuleOffset(); }

int WasmStackFrame::GetEnclosingColumnNumber() {
  const int function_offset =
      GetWasmFunctionOffset(wasm_instance_->module(), wasm_func_index_);
  return function_offset;
}

int WasmStackFrame::GetModuleOffset() const {
  const int function_offset =
      GetWasmFunctionOffset(wasm_instance_->module(), wasm_func_index_);
  return function_offset + GetPosition();
}

Handle<Object> WasmStackFrame::GetFileName() { return Null(); }

Handle<PrimitiveHeapObject> WasmStackFrame::Null() const {
  return isolate_->factory()->null_value();
}

bool WasmStackFrame::HasScript() const { return true; }

Handle<Script> WasmStackFrame::GetScript() const {
  return handle(wasm_instance_->module_object().script(), isolate_);
}

void AsmJsWasmStackFrame::FromFrameArray(Isolate* isolate,
                                         Handle<FrameArray> array,
                                         int frame_ix) {
  DCHECK(array->IsAsmJsWasmFrame(frame_ix));
  WasmStackFrame::FromFrameArray(isolate, array, frame_ix);
  is_at_number_conversion_ =
      array->Flags(frame_ix).value() & FrameArray::kAsmJsAtNumberConversion;
}

Handle<Object> AsmJsWasmStackFrame::GetReceiver() const {
  return isolate_->global_proxy();
}

Handle<Object> AsmJsWasmStackFrame::GetFunction() const {
  // TODO(clemensb): Return lazily created JSFunction.
  return Null();
}

Handle<Object> AsmJsWasmStackFrame::GetFileName() {
  Handle<Script> script(wasm_instance_->module_object().script(), isolate_);
  DCHECK(script->IsUserJavaScript());
  return handle(script->name(), isolate_);
}

Handle<Object> AsmJsWasmStackFrame::GetScriptNameOrSourceUrl() {
  Handle<Script> script(wasm_instance_->module_object().script(), isolate_);
  DCHECK_EQ(Script::TYPE_NORMAL, script->type());
  return ScriptNameOrSourceUrl(script, isolate_);
}

int AsmJsWasmStackFrame::GetPosition() const {
  DCHECK_LE(0, offset_);
  int byte_offset = code_->GetSourcePositionBefore(offset_);
  const wasm::WasmModule* module = wasm_instance_->module();
  return GetSourcePosition(module, wasm_func_index_, byte_offset,
                           is_at_number_conversion_);
}

int AsmJsWasmStackFrame::GetLineNumber() {
  DCHECK_LE(0, GetPosition());
  Handle<Script> script(wasm_instance_->module_object().script(), isolate_);
  DCHECK(script->IsUserJavaScript());
  return Script::GetLineNumber(script, GetPosition()) + 1;
}

int AsmJsWasmStackFrame::GetColumnNumber() {
  DCHECK_LE(0, GetPosition());
  Handle<Script> script(wasm_instance_->module_object().script(), isolate_);
  DCHECK(script->IsUserJavaScript());
  return Script::GetColumnNumber(script, GetPosition()) + 1;
}

int AsmJsWasmStackFrame::GetEnclosingLineNumber() {
  DCHECK_LE(0, GetPosition());
  Handle<Script> script(wasm_instance_->module_object().script(), isolate_);
  DCHECK(script->IsUserJavaScript());
  int byte_offset = GetSourcePosition(wasm_instance_->module(),
                                      wasm_func_index_, 0,
                                      is_at_number_conversion_);
  return Script::GetLineNumber(script, byte_offset) + 1;
}

int AsmJsWasmStackFrame::GetEnclosingColumnNumber() {
  DCHECK_LE(0, GetPosition());
  Handle<Script> script(wasm_instance_->module_object().script(), isolate_);
  DCHECK(script->IsUserJavaScript());
  int byte_offset = GetSourcePosition(wasm_instance_->module(),
                                      wasm_func_index_, 0,
                                      is_at_number_conversion_);
  return Script::GetColumnNumber(script, byte_offset) + 1;
}

FrameArrayIterator::FrameArrayIterator(Isolate* isolate,
                                       Handle<FrameArray> array, int frame_ix)
    : isolate_(isolate), array_(array), frame_ix_(frame_ix) {}

bool FrameArrayIterator::HasFrame() const {
  return (frame_ix_ < array_->FrameCount());
}

void FrameArrayIterator::Advance() { frame_ix_++; }

StackFrameBase* FrameArrayIterator::Frame() {
  DCHECK(HasFrame());
  const int flags = array_->Flags(frame_ix_).value();
  int flag_mask = FrameArray::kIsWasmFrame | FrameArray::kIsAsmJsWasmFrame;
  switch (flags & flag_mask) {
    case 0:
      js_frame_.FromFrameArray(isolate_, array_, frame_ix_);
      return &js_frame_;
    case FrameArray::kIsWasmFrame:
      wasm_frame_.FromFrameArray(isolate_, array_, frame_ix_);
      return &wasm_frame_;
    case FrameArray::kIsAsmJsWasmFrame:
      asm_wasm_frame_.FromFrameArray(isolate_, array_, frame_ix_);
      return &asm_wasm_frame_;
    default:
      UNREACHABLE();
  }
}

namespace {

MaybeHandle<Object> ConstructCallSite(Isolate* isolate,
                                      Handle<StackTraceFrame> frame) {
  Handle<JSFunction> target =
      handle(isolate->native_context()->callsite_function(), isolate);

  Handle<JSObject> obj;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, obj,
      JSObject::New(target, target, Handle<AllocationSite>::null()), Object);

  // TODO(szuend): Introduce a new symbol "call_site_frame_symbol" and set
  //               it to the StackTraceFrame. The CallSite API builtins can then
  //               be implemented using StackFrameInfo objects.

  Handle<FrameArray> frame_array(FrameArray::cast(frame->frame_array()),
                                 isolate);
  int frame_index = frame->frame_index();

  Handle<Symbol> key = isolate->factory()->call_site_frame_array_symbol();
  RETURN_ON_EXCEPTION(isolate,
                      JSObject::SetOwnPropertyIgnoreAttributes(
                          obj, key, frame_array, DONT_ENUM),
                      Object);

  key = isolate->factory()->call_site_frame_index_symbol();
  Handle<Object> value(Smi::FromInt(frame_index), isolate);
  RETURN_ON_EXCEPTION(
      isolate,
      JSObject::SetOwnPropertyIgnoreAttributes(obj, key, value, DONT_ENUM),
      Object);

  return obj;
}

// Convert the raw frames as written by Isolate::CaptureSimpleStackTrace into
// a JSArray of JSCallSite objects.
MaybeHandle<JSArray> GetStackFrames(Isolate* isolate,
                                    Handle<FixedArray> elems) {
  const int frame_count = elems->length();

  Handle<FixedArray> frames = isolate->factory()->NewFixedArray(frame_count);
  for (int i = 0; i < frame_count; i++) {
    Handle<Object> site;
    Handle<StackTraceFrame> frame(StackTraceFrame::cast(elems->get(i)),
                                  isolate);
    ASSIGN_RETURN_ON_EXCEPTION(isolate, site, ConstructCallSite(isolate, frame),
                               JSArray);
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

class V8_NODISCARD PrepareStackTraceScope {
 public:
  explicit PrepareStackTraceScope(Isolate* isolate) : isolate_(isolate) {
    DCHECK(!isolate_->formatting_stack_trace());
    isolate_->set_formatting_stack_trace(true);
  }

  ~PrepareStackTraceScope() { isolate_->set_formatting_stack_trace(false); }

  PrepareStackTraceScope(const PrepareStackTraceScope&) = delete;
  PrepareStackTraceScope& operator=(const PrepareStackTraceScope&) = delete;

 private:
  Isolate* isolate_;
};

}  // namespace

// static
MaybeHandle<Object> ErrorUtils::FormatStackTrace(Isolate* isolate,
                                                 Handle<JSObject> error,
                                                 Handle<Object> raw_stack) {
  DCHECK(raw_stack->IsFixedArray());
  Handle<FixedArray> elems = Handle<FixedArray>::cast(raw_stack);

  const bool in_recursion = isolate->formatting_stack_trace();
  if (!in_recursion) {
    Handle<Context> error_context = error->GetCreationContext();
    DCHECK(error_context->IsNativeContext());

    if (isolate->HasPrepareStackTraceCallback()) {
      PrepareStackTraceScope scope(isolate);

      Handle<JSArray> sites;
      ASSIGN_RETURN_ON_EXCEPTION(isolate, sites, GetStackFrames(isolate, elems),
                                 Object);

      Handle<Object> result;
      ASSIGN_RETURN_ON_EXCEPTION(
          isolate, result,
          isolate->RunPrepareStackTraceCallback(error_context, error, sites),
          Object);
      return result;
    } else {
      Handle<JSFunction> global_error =
          handle(error_context->error_function(), isolate);

      // If there's a user-specified "prepareStackTrace" function, call it on
      // the frames and use its result.

      Handle<Object> prepare_stack_trace;
      ASSIGN_RETURN_ON_EXCEPTION(
          isolate, prepare_stack_trace,
          JSFunction::GetProperty(isolate, global_error, "prepareStackTrace"),
          Object);

      if (prepare_stack_trace->IsJSFunction()) {
        PrepareStackTraceScope scope(isolate);

        isolate->CountUsage(v8::Isolate::kErrorPrepareStackTrace);

        Handle<JSArray> sites;
        ASSIGN_RETURN_ON_EXCEPTION(isolate, sites,
                                   GetStackFrames(isolate, elems), Object);

        const int argc = 2;
        ScopedVector<Handle<Object>> argv(argc);
        argv[0] = error;
        argv[1] = sites;

        Handle<Object> result;

        ASSIGN_RETURN_ON_EXCEPTION(
            isolate, result,
            Execution::Call(isolate, prepare_stack_trace, global_error, argc,
                            argv.begin()),
            Object);

        return result;
      }
    }
  }

  // Otherwise, run our internal formatting logic.

  IncrementalStringBuilder builder(isolate);

  RETURN_ON_EXCEPTION(isolate, AppendErrorString(isolate, error, &builder),
                      Object);

  wasm::WasmCodeRefScope wasm_code_ref_scope;

  for (int i = 0; i < elems->length(); ++i) {
    builder.AppendCString("\n    at ");

    Handle<StackTraceFrame> frame(StackTraceFrame::cast(elems->get(i)),
                                  isolate);
    SerializeStackTraceFrame(isolate, frame, &builder);

    if (isolate->has_pending_exception()) {
      // CallSite.toString threw. Parts of the current frame might have been
      // stringified already regardless. Still, try to append a string
      // representation of the thrown exception.

      Handle<Object> pending_exception =
          handle(isolate->pending_exception(), isolate);
      isolate->clear_pending_exception();
      isolate->set_external_caught_exception(false);

      MaybeHandle<String> exception_string =
          ErrorUtils::ToString(isolate, pending_exception);
      if (exception_string.is_null()) {
        // Formatting the thrown exception threw again, give up.

        builder.AppendCString("<error>");
      } else {
        // Formatted thrown exception successfully, append it.
        builder.AppendCString("<error: ");
        builder.AppendString(exception_string.ToHandleChecked());
        builder.AppendCString("<error>");
      }
    }
  }

  return builder.Finish();
}

Handle<String> MessageFormatter::Format(Isolate* isolate, MessageTemplate index,
                                        Handle<Object> arg0,
                                        Handle<Object> arg1,
                                        Handle<Object> arg2) {
  Factory* factory = isolate->factory();
  Handle<String> arg0_string = factory->empty_string();
  if (!arg0.is_null()) {
    arg0_string = Object::NoSideEffectsToString(isolate, arg0);
  }
  Handle<String> arg1_string = factory->empty_string();
  if (!arg1.is_null()) {
    arg1_string = Object::NoSideEffectsToString(isolate, arg1);
  }
  Handle<String> arg2_string = factory->empty_string();
  if (!arg2.is_null()) {
    arg2_string = Object::NoSideEffectsToString(isolate, arg2);
  }
  MaybeHandle<String> maybe_result_string = MessageFormatter::Format(
      isolate, index, arg0_string, arg1_string, arg2_string);
  Handle<String> result_string;
  if (!maybe_result_string.ToHandle(&result_string)) {
    DCHECK(isolate->has_pending_exception());
    isolate->clear_pending_exception();
    return factory->InternalizeString(StaticCharVector("<error>"));
  }
  // A string that has been obtained from JS code in this way is
  // likely to be a complicated ConsString of some sort.  We flatten it
  // here to improve the efficiency of converting it to a C string and
  // other operations that are likely to take place (see GetLocalizedMessage
  // for example).
  return String::Flatten(isolate, result_string);
}

const char* MessageFormatter::TemplateString(MessageTemplate index) {
  switch (index) {
#define CASE(NAME, STRING)       \
  case MessageTemplate::k##NAME: \
    return STRING;
    MESSAGE_TEMPLATES(CASE)
#undef CASE
    case MessageTemplate::kMessageCount:
    default:
      return nullptr;
  }
}

MaybeHandle<String> MessageFormatter::Format(Isolate* isolate,
                                             MessageTemplate index,
                                             Handle<String> arg0,
                                             Handle<String> arg1,
                                             Handle<String> arg2) {
  const char* template_string = TemplateString(index);
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

MaybeHandle<JSObject> ErrorUtils::Construct(Isolate* isolate,
                                            Handle<JSFunction> target,
                                            Handle<Object> new_target,
                                            Handle<Object> message) {
  FrameSkipMode mode = SKIP_FIRST;
  Handle<Object> caller;

  // When we're passed a JSFunction as new target, we can skip frames until that
  // specific function is seen instead of unconditionally skipping the first
  // frame.
  if (new_target->IsJSFunction()) {
    mode = SKIP_UNTIL_SEEN;
    caller = new_target;
  }

  return ErrorUtils::Construct(isolate, target, new_target, message, mode,
                               caller,
                               ErrorUtils::StackTraceCollection::kDetailed);
}

MaybeHandle<JSObject> ErrorUtils::Construct(
    Isolate* isolate, Handle<JSFunction> target, Handle<Object> new_target,
    Handle<Object> message, FrameSkipMode mode, Handle<Object> caller,
    StackTraceCollection stack_trace_collection) {
  if (FLAG_correctness_fuzzer_suppressions) {
    // Abort range errors in correctness fuzzing, as their causes differ
    // accross correctness-fuzzing scenarios.
    if (target.is_identical_to(isolate->range_error_function())) {
      FATAL("Aborting on range error");
    }
    // Ignore error messages in correctness fuzzing, because the spec leaves
    // room for undefined behavior.
    message = isolate->factory()->InternalizeUtf8String(
        "Message suppressed for fuzzers (--correctness-fuzzer-suppressions)");
  }

  // 1. If NewTarget is undefined, let newTarget be the active function object,
  // else let newTarget be NewTarget.
  Handle<JSReceiver> new_target_recv =
      new_target->IsJSReceiver() ? Handle<JSReceiver>::cast(new_target)
                                 : Handle<JSReceiver>::cast(target);

  // 2. Let O be ? OrdinaryCreateFromConstructor(newTarget, "%ErrorPrototype%",
  //    « [[ErrorData]] »).
  Handle<JSObject> err;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, err,
      JSObject::New(target, new_target_recv, Handle<AllocationSite>::null()),
      JSObject);

  // 3. If message is not undefined, then
  //  a. Let msg be ? ToString(message).
  //  b. Let msgDesc be the PropertyDescriptor{[[Value]]: msg, [[Writable]]:
  //     true, [[Enumerable]]: false, [[Configurable]]: true}.
  //  c. Perform ! DefinePropertyOrThrow(O, "message", msgDesc).
  // 4. Return O.

  if (!message->IsUndefined(isolate)) {
    Handle<String> msg_string;
    ASSIGN_RETURN_ON_EXCEPTION(isolate, msg_string,
                               Object::ToString(isolate, message), JSObject);
    RETURN_ON_EXCEPTION(
        isolate,
        JSObject::SetOwnPropertyIgnoreAttributes(
            err, isolate->factory()->message_string(), msg_string, DONT_ENUM),
        JSObject);
  }

  switch (stack_trace_collection) {
    case StackTraceCollection::kDetailed:
      RETURN_ON_EXCEPTION(
          isolate, isolate->CaptureAndSetDetailedStackTrace(err), JSObject);
      V8_FALLTHROUGH;
    case StackTraceCollection::kSimple:
      RETURN_ON_EXCEPTION(
          isolate, isolate->CaptureAndSetSimpleStackTrace(err, mode, caller),
          JSObject);
      break;
    case StackTraceCollection::kNone:
      break;
  }
  return err;
}

namespace {

MaybeHandle<String> GetStringPropertyOrDefault(Isolate* isolate,
                                               Handle<JSReceiver> recv,
                                               Handle<String> key,
                                               Handle<String> default_str) {
  Handle<Object> obj;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, obj,
                             JSObject::GetProperty(isolate, recv, key), String);

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

Handle<String> DoFormatMessage(Isolate* isolate, MessageTemplate index,
                               Handle<Object> arg0, Handle<Object> arg1,
                               Handle<Object> arg2) {
  Handle<String> arg0_str = Object::NoSideEffectsToString(isolate, arg0);
  Handle<String> arg1_str = Object::NoSideEffectsToString(isolate, arg1);
  Handle<String> arg2_str = Object::NoSideEffectsToString(isolate, arg2);

  isolate->native_context()->IncrementErrorsThrown();

  Handle<String> msg;
  if (!MessageFormatter::Format(isolate, index, arg0_str, arg1_str, arg2_str)
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
Handle<JSObject> ErrorUtils::MakeGenericError(
    Isolate* isolate, Handle<JSFunction> constructor, MessageTemplate index,
    Handle<Object> arg0, Handle<Object> arg1, Handle<Object> arg2,
    FrameSkipMode mode) {
  if (FLAG_clear_exceptions_on_js_entry) {
    // This function used to be implemented in JavaScript, and JSEntry
    // clears any pending exceptions - so whenever we'd call this from C++,
    // pending exceptions would be cleared. Preserve this behavior.
    isolate->clear_pending_exception();
  }
  Handle<String> msg = DoFormatMessage(isolate, index, arg0, arg1, arg2);

  DCHECK(mode != SKIP_UNTIL_SEEN);

  Handle<Object> no_caller;
  // The call below can't fail because constructor is a builtin.
  DCHECK(constructor->shared().HasBuiltinId());
  return ErrorUtils::Construct(isolate, constructor, constructor, msg, mode,
                               no_caller, StackTraceCollection::kDetailed)
      .ToHandleChecked();
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
    Handle<SharedFunctionInfo> shared(summary.function()->shared(), isolate);
    Handle<Object> script(shared->script(), isolate);
    SharedFunctionInfo::EnsureSourcePositionsAvailable(isolate, shared);
    int pos = summary.abstract_code()->SourcePosition(summary.code_offset());
    if (script->IsScript() &&
        !(Handle<Script>::cast(script)->source().IsUndefined(isolate))) {
      Handle<Script> casted_script = Handle<Script>::cast(script);
      *target = MessageLocation(casted_script, pos, pos + 1, shared);
      return true;
    }
  }
  return false;
}

Handle<String> BuildDefaultCallSite(Isolate* isolate, Handle<Object> object) {
  IncrementalStringBuilder builder(isolate);

  builder.AppendString(Object::TypeOf(isolate, object));
  if (object->IsString()) {
    builder.AppendCString(" \"");
    Handle<String> string = Handle<String>::cast(object);
    // This threshold must be sufficiently far below String::kMaxLength that
    // the {builder}'s result can never exceed that limit.
    constexpr int kMaxPrintedStringLength = 100;
    if (string->length() <= kMaxPrintedStringLength) {
      builder.AppendString(string);
    } else {
      string = isolate->factory()->NewProperSubString(string, 0,
                                                      kMaxPrintedStringLength);
      builder.AppendString(string);
      builder.AppendCString("<...>");
    }
    builder.AppendCString("\"");
  } else if (object->IsNull(isolate)) {
    builder.AppendCString(" ");
    builder.AppendString(isolate->factory()->null_string());
  } else if (object->IsTrue(isolate)) {
    builder.AppendCString(" ");
    builder.AppendString(isolate->factory()->true_string());
  } else if (object->IsFalse(isolate)) {
    builder.AppendCString(" ");
    builder.AppendString(isolate->factory()->false_string());
  } else if (object->IsNumber()) {
    builder.AppendCString(" ");
    builder.AppendString(isolate->factory()->NumberToString(object));
  }

  return builder.Finish().ToHandleChecked();
}

Handle<String> RenderCallSite(Isolate* isolate, Handle<Object> object,
                              MessageLocation* location,
                              CallPrinter::ErrorHint* hint) {
  if (ComputeLocation(isolate, location)) {
    UnoptimizedCompileFlags flags = UnoptimizedCompileFlags::ForFunctionCompile(
        isolate, *location->shared());
    UnoptimizedCompileState compile_state(isolate);
    ParseInfo info(isolate, flags, &compile_state);
    if (parsing::ParseAny(&info, location->shared(), isolate,
                          parsing::ReportStatisticsMode::kNo)) {
      info.ast_value_factory()->Internalize(isolate);
      CallPrinter printer(isolate, location->shared()->IsUserJavaScript());
      Handle<String> str = printer.Print(info.literal(), location->start_pos());
      *hint = printer.GetErrorHint();
      if (str->length() > 0) return str;
    }
  }
  return BuildDefaultCallSite(isolate, object);
}

MessageTemplate UpdateErrorTemplate(CallPrinter::ErrorHint hint,
                                    MessageTemplate default_id) {
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
}

}  // namespace

Handle<JSObject> ErrorUtils::NewIteratorError(Isolate* isolate,
                                              Handle<Object> source) {
  MessageLocation location;
  CallPrinter::ErrorHint hint = CallPrinter::kNone;
  Handle<String> callsite = RenderCallSite(isolate, source, &location, &hint);
  MessageTemplate id = MessageTemplate::kNotIterableNoSymbolLoad;

  if (hint == CallPrinter::kNone) {
    Handle<Symbol> iterator_symbol = isolate->factory()->iterator_symbol();
    return isolate->factory()->NewTypeError(id, callsite, iterator_symbol);
  }

  id = UpdateErrorTemplate(hint, id);
  return isolate->factory()->NewTypeError(id, callsite);
}

Object ErrorUtils::ThrowSpreadArgError(Isolate* isolate, MessageTemplate id,
                                       Handle<Object> object) {
  MessageLocation location;
  Handle<String> callsite;
  if (ComputeLocation(isolate, &location)) {
    UnoptimizedCompileFlags flags = UnoptimizedCompileFlags::ForFunctionCompile(
        isolate, *location.shared());
    UnoptimizedCompileState compile_state(isolate);
    ParseInfo info(isolate, flags, &compile_state);
    if (parsing::ParseAny(&info, location.shared(), isolate,
                          parsing::ReportStatisticsMode::kNo)) {
      info.ast_value_factory()->Internalize(isolate);
      CallPrinter printer(isolate, location.shared()->IsUserJavaScript(),
                          CallPrinter::SpreadErrorInArgsHint::kErrorInArgs);
      Handle<String> str = printer.Print(info.literal(), location.start_pos());
      callsite =
          str->length() > 0 ? str : BuildDefaultCallSite(isolate, object);

      if (printer.spread_arg() != nullptr) {
        // Change the message location to point at the property name.
        int pos = printer.spread_arg()->position();
        location =
            MessageLocation(location.script(), pos, pos + 1, location.shared());
      }
    } else {
      callsite = BuildDefaultCallSite(isolate, object);
    }
  }

  isolate->ThrowAt(isolate->factory()->NewTypeError(id, callsite, object),
                   &location);
  return ReadOnlyRoots(isolate).exception();
}

Handle<JSObject> ErrorUtils::NewCalledNonCallableError(Isolate* isolate,
                                                       Handle<Object> source) {
  MessageLocation location;
  CallPrinter::ErrorHint hint = CallPrinter::kNone;
  Handle<String> callsite = RenderCallSite(isolate, source, &location, &hint);
  MessageTemplate id = MessageTemplate::kCalledNonCallable;
  id = UpdateErrorTemplate(hint, id);
  return isolate->factory()->NewTypeError(id, callsite);
}

Handle<JSObject> ErrorUtils::NewConstructedNonConstructable(
    Isolate* isolate, Handle<Object> source) {
  MessageLocation location;
  CallPrinter::ErrorHint hint = CallPrinter::kNone;
  Handle<String> callsite = RenderCallSite(isolate, source, &location, &hint);
  MessageTemplate id = MessageTemplate::kNotConstructor;
  return isolate->factory()->NewTypeError(id, callsite);
}

Object ErrorUtils::ThrowLoadFromNullOrUndefined(Isolate* isolate,
                                                Handle<Object> object,
                                                MaybeHandle<Object> key) {
  DCHECK(object->IsNullOrUndefined());

  MaybeHandle<String> maybe_property_name;

  // Try to extract the property name from the given key, if any.
  Handle<Object> key_handle;
  if (key.ToHandle(&key_handle)) {
    if (key_handle->IsString()) {
      maybe_property_name = Handle<String>::cast(key_handle);
    }
  }

  Handle<String> callsite;

  // Inline the RenderCallSite logic here so that we can additonally access the
  // destructuring property.
  bool location_computed = false;
  bool is_destructuring = false;
  MessageLocation location;
  if (ComputeLocation(isolate, &location)) {
    location_computed = true;

    UnoptimizedCompileFlags flags = UnoptimizedCompileFlags::ForFunctionCompile(
        isolate, *location.shared());
    UnoptimizedCompileState compile_state(isolate);
    ParseInfo info(isolate, flags, &compile_state);
    if (parsing::ParseAny(&info, location.shared(), isolate,
                          parsing::ReportStatisticsMode::kNo)) {
      info.ast_value_factory()->Internalize(isolate);
      CallPrinter printer(isolate, location.shared()->IsUserJavaScript());
      Handle<String> str = printer.Print(info.literal(), location.start_pos());

      int pos = -1;
      is_destructuring = printer.destructuring_assignment() != nullptr;

      if (is_destructuring) {
        // If we don't have one yet, try to extract the property name from the
        // destructuring property in the AST.
        ObjectLiteralProperty* destructuring_prop =
            printer.destructuring_prop();
        if (maybe_property_name.is_null() && destructuring_prop != nullptr &&
            destructuring_prop->key()->IsPropertyName()) {
          maybe_property_name = destructuring_prop->key()
                                    ->AsLiteral()
                                    ->AsRawPropertyName()
                                    ->string();
          // Change the message location to point at the property name.
          pos = destructuring_prop->key()->position();
        }
        if (maybe_property_name.is_null()) {
          // Change the message location to point at the destructured value.
          pos = printer.destructuring_assignment()->value()->position();
        }

        // If we updated the pos to a valid pos, rewrite the location.
        if (pos != -1) {
          location = MessageLocation(location.script(), pos, pos + 1,
                                     location.shared());
        }
      }

      if (str->length() > 0) callsite = str;
    }
  }

  if (callsite.is_null()) {
    callsite = BuildDefaultCallSite(isolate, object);
  }

  Handle<JSObject> error;
  Handle<String> property_name;
  if (is_destructuring) {
    if (maybe_property_name.ToHandle(&property_name)) {
      error = isolate->factory()->NewTypeError(
          MessageTemplate::kNonCoercibleWithProperty, property_name, callsite,
          object);
    } else {
      error = isolate->factory()->NewTypeError(MessageTemplate::kNonCoercible,
                                               callsite, object);
    }
  } else {
    Handle<Object> key_handle;
    if (!key.ToHandle(&key_handle)) {
      key_handle = ReadOnlyRoots(isolate).undefined_value_handle();
    }
    if (*key_handle == ReadOnlyRoots(isolate).iterator_symbol()) {
      error = NewIteratorError(isolate, object);
    } else {
      error = isolate->factory()->NewTypeError(
          MessageTemplate::kNonObjectPropertyLoad, key_handle, object);
    }
  }

  if (location_computed) {
    isolate->ThrowAt(error, &location);
  } else {
    isolate->Throw(*error);
  }
  return ReadOnlyRoots(isolate).exception();
}

}  // namespace internal
}  // namespace v8
