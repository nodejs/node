// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/objects/stack-frame-info.h"

#include "src/objects/shared-function-info.h"
#include "src/objects/stack-frame-info-inl.h"
#include "src/strings/string-builder-inl.h"

namespace v8 {
namespace internal {

bool StackFrameInfo::IsPromiseAll() const {
  if (!IsAsync()) return false;
  JSFunction fun = JSFunction::cast(function());
  return fun == fun.native_context().promise_all();
}

bool StackFrameInfo::IsPromiseAny() const {
  if (!IsAsync()) return false;
  JSFunction fun = JSFunction::cast(function());
  return fun == fun.native_context().promise_any();
}

bool StackFrameInfo::IsNative() const {
  if (auto script = GetScript()) {
    return script->type() == Script::TYPE_NATIVE;
  }
  return false;
}

bool StackFrameInfo::IsEval() const {
  if (auto script = GetScript()) {
    return script->compilation_type() == Script::COMPILATION_TYPE_EVAL;
  }
  return false;
}

bool StackFrameInfo::IsUserJavaScript() const {
  return !IsWasm() && GetSharedFunctionInfo().IsUserJavaScript();
}

bool StackFrameInfo::IsMethodCall() const {
  return !IsWasm() && !IsToplevel() && !IsConstructor();
}

bool StackFrameInfo::IsToplevel() const {
  return receiver_or_instance().IsJSGlobalProxy() ||
         receiver_or_instance().IsNullOrUndefined();
}

// static
int StackFrameInfo::GetLineNumber(Handle<StackFrameInfo> info) {
  Isolate* isolate = info->GetIsolate();
  if (info->IsWasm() && !info->IsAsmJsWasm()) {
    return 1;
  }
  Handle<Script> script;
  if (GetScript(isolate, info).ToHandle(&script)) {
    int position = GetSourcePosition(info);
    return Script::GetLineNumber(script, position) + 1;
  }
  return Message::kNoLineNumberInfo;
}

// static
int StackFrameInfo::GetColumnNumber(Handle<StackFrameInfo> info) {
  Isolate* isolate = info->GetIsolate();
  int position = GetSourcePosition(info);
  if (info->IsWasm() && !info->IsAsmJsWasm()) {
    return position + 1;
  }
  Handle<Script> script;
  if (GetScript(isolate, info).ToHandle(&script)) {
    return Script::GetColumnNumber(script, position) + 1;
  }
  return Message::kNoColumnInfo;
}

// static
int StackFrameInfo::GetEnclosingLineNumber(Handle<StackFrameInfo> info) {
  Isolate* isolate = info->GetIsolate();
  if (info->IsWasm() && !info->IsAsmJsWasm()) {
    return 1;
  }
  Handle<Script> script;
  if (GetScript(isolate, info).ToHandle(&script)) {
    int position;
    if (info->IsAsmJsWasm()) {
      auto module = info->GetWasmInstance().module();
      auto func_index = info->GetWasmFunctionIndex();
      position = wasm::GetSourcePosition(module, func_index, 0,
                                         info->IsAsmJsAtNumberConversion());
    } else {
      position = info->GetSharedFunctionInfo().function_token_position();
    }
    return Script::GetLineNumber(script, position) + 1;
  }
  return Message::kNoLineNumberInfo;
}

// static
int StackFrameInfo::GetEnclosingColumnNumber(Handle<StackFrameInfo> info) {
  Isolate* isolate = info->GetIsolate();
  if (info->IsWasm() && !info->IsAsmJsWasm()) {
    auto module = info->GetWasmInstance().module();
    auto func_index = info->GetWasmFunctionIndex();
    return GetWasmFunctionOffset(module, func_index);
  }
  Handle<Script> script;
  if (GetScript(isolate, info).ToHandle(&script)) {
    int position;
    if (info->IsAsmJsWasm()) {
      auto module = info->GetWasmInstance().module();
      auto func_index = info->GetWasmFunctionIndex();
      position = wasm::GetSourcePosition(module, func_index, 0,
                                         info->IsAsmJsAtNumberConversion());
    } else {
      position = info->GetSharedFunctionInfo().function_token_position();
    }
    return Script::GetColumnNumber(script, position) + 1;
  }
  return Message::kNoColumnInfo;
}

int StackFrameInfo::GetScriptId() const {
  if (auto script = GetScript()) {
    return script->id();
  }
  return Message::kNoScriptIdInfo;
}

Object StackFrameInfo::GetScriptName() const {
  if (auto script = GetScript()) {
    return script->name();
  }
  return ReadOnlyRoots(GetIsolate()).null_value();
}

Object StackFrameInfo::GetScriptNameOrSourceURL() const {
  if (auto script = GetScript()) {
    return script->GetNameOrSourceURL();
  }
  return ReadOnlyRoots(GetIsolate()).null_value();
}

namespace {

MaybeHandle<String> FormatEvalOrigin(Isolate* isolate, Handle<Script> script) {
  Handle<Object> sourceURL(script->GetNameOrSourceURL(), isolate);
  if (sourceURL->IsString()) return Handle<String>::cast(sourceURL);

  IncrementalStringBuilder builder(isolate);
  builder.AppendCString("eval at ");
  if (script->has_eval_from_shared()) {
    Handle<SharedFunctionInfo> eval_shared(script->eval_from_shared(), isolate);
    auto eval_name = SharedFunctionInfo::DebugName(eval_shared);
    if (eval_name->length() != 0) {
      builder.AppendString(eval_name);
    } else {
      builder.AppendCString("<anonymous>");
    }
    if (eval_shared->script().IsScript()) {
      Handle<Script> eval_script(Script::cast(eval_shared->script()), isolate);
      builder.AppendCString(" (");
      if (eval_script->compilation_type() == Script::COMPILATION_TYPE_EVAL) {
        // Eval script originated from another eval.
        Handle<String> str;
        ASSIGN_RETURN_ON_EXCEPTION(
            isolate, str, FormatEvalOrigin(isolate, eval_script), String);
        builder.AppendString(str);
      } else {
        // eval script originated from "real" source.
        Handle<Object> eval_script_name(eval_script->name(), isolate);
        if (eval_script_name->IsString()) {
          builder.AppendString(Handle<String>::cast(eval_script_name));
          Script::PositionInfo info;
          if (Script::GetPositionInfo(eval_script,
                                      Script::GetEvalPosition(isolate, script),
                                      &info, Script::NO_OFFSET)) {
            builder.AppendCString(":");
            builder.AppendInt(info.line + 1);
            builder.AppendCString(":");
            builder.AppendInt(info.column + 1);
          }
        } else {
          builder.AppendCString("unknown source");
        }
      }
      builder.AppendCString(")");
    }
  } else {
    builder.AppendCString("<anonymous>");
  }
  return builder.Finish().ToHandleChecked();
}

}  // namespace

// static
Handle<PrimitiveHeapObject> StackFrameInfo::GetEvalOrigin(
    Handle<StackFrameInfo> info) {
  auto isolate = info->GetIsolate();
  Handle<Script> script;
  if (!GetScript(isolate, info).ToHandle(&script) ||
      script->compilation_type() != Script::COMPILATION_TYPE_EVAL) {
    return isolate->factory()->undefined_value();
  }
  return FormatEvalOrigin(isolate, script).ToHandleChecked();
}

// static
Handle<Object> StackFrameInfo::GetFunctionName(Handle<StackFrameInfo> info) {
  Isolate* isolate = info->GetIsolate();
  if (info->IsWasm()) {
    Handle<WasmModuleObject> module_object(
        info->GetWasmInstance().module_object(), isolate);
    uint32_t func_index = info->GetWasmFunctionIndex();
    Handle<String> name;
    if (WasmModuleObject::GetFunctionNameOrNull(isolate, module_object,
                                                func_index)
            .ToHandle(&name)) {
      return name;
    }
  } else {
    Handle<JSFunction> function(JSFunction::cast(info->function()), isolate);
    Handle<String> name = JSFunction::GetDebugName(function);
    if (name->length() != 0) return name;
    if (info->IsEval()) return isolate->factory()->eval_string();
  }
  return isolate->factory()->null_value();
}

namespace {

PrimitiveHeapObject InferMethodNameFromFastObject(Isolate* isolate,
                                                  JSObject receiver,
                                                  JSFunction fun,
                                                  PrimitiveHeapObject name) {
  ReadOnlyRoots roots(isolate);
  Map map = receiver.map();
  DescriptorArray descriptors = map.instance_descriptors(kRelaxedLoad);
  for (auto i : map.IterateOwnDescriptors()) {
    PrimitiveHeapObject key = descriptors.GetKey(i);
    if (key.IsSymbol()) continue;
    auto details = descriptors.GetDetails(i);
    if (details.IsDontEnum()) continue;
    Object value;
    if (details.location() == kField) {
      auto field_index = FieldIndex::ForPropertyIndex(
          map, details.field_index(), details.representation());
      if (field_index.is_double()) continue;
      value = receiver.RawFastPropertyAt(isolate, field_index);
    } else {
      value = descriptors.GetStrongValue(i);
    }
    if (value != fun) {
      if (!value.IsAccessorPair()) continue;
      auto pair = AccessorPair::cast(value);
      if (pair.getter() != fun && pair.setter() != fun) continue;
    }
    if (name != key) {
      name = name.IsUndefined(isolate) ? key : roots.null_value();
    }
  }
  return name;
}

template <typename Dictionary>
PrimitiveHeapObject InferMethodNameFromDictionary(Isolate* isolate,
                                                  Dictionary dictionary,
                                                  JSFunction fun,
                                                  PrimitiveHeapObject name) {
  ReadOnlyRoots roots(isolate);
  for (auto i : dictionary.IterateEntries()) {
    Object key;
    if (!dictionary.ToKey(roots, i, &key)) continue;
    if (key.IsSymbol()) continue;
    auto details = dictionary.DetailsAt(i);
    if (details.IsDontEnum()) continue;
    auto value = dictionary.ValueAt(i);
    if (value != fun) {
      if (!value.IsAccessorPair()) continue;
      auto pair = AccessorPair::cast(value);
      if (pair.getter() != fun && pair.setter() != fun) continue;
    }
    if (name != key) {
      name = name.IsUndefined(isolate) ? PrimitiveHeapObject::cast(key)
                                       : roots.null_value();
    }
  }
  return name;
}

PrimitiveHeapObject InferMethodName(Isolate* isolate, JSReceiver receiver,
                                    JSFunction fun) {
  DisallowGarbageCollection no_gc;
  ReadOnlyRoots roots(isolate);
  PrimitiveHeapObject name = roots.undefined_value();
  for (PrototypeIterator it(isolate, receiver, kStartAtReceiver); !it.IsAtEnd();
       it.Advance()) {
    auto current = it.GetCurrent();
    if (!current.IsJSObject()) break;
    auto object = JSObject::cast(current);
    if (object.IsAccessCheckNeeded()) break;
    if (object.HasFastProperties()) {
      name = InferMethodNameFromFastObject(isolate, object, fun, name);
    } else if (object.IsJSGlobalObject()) {
      name = InferMethodNameFromDictionary(
          isolate, JSGlobalObject::cast(object).global_dictionary(kAcquireLoad),
          fun, name);
    } else if (V8_DICT_MODE_PROTOTYPES_BOOL) {
      name = InferMethodNameFromDictionary(
          isolate, object.property_dictionary_ordered(), fun, name);
    } else {
      name = InferMethodNameFromDictionary(
          isolate, object.property_dictionary(), fun, name);
    }
  }
  if (name.IsUndefined(isolate)) return roots.null_value();
  return name;
}

}  // namespace

// static
Handle<Object> StackFrameInfo::GetMethodName(Handle<StackFrameInfo> info) {
  Isolate* isolate = info->GetIsolate();
  Handle<Object> receiver_or_instance(info->receiver_or_instance(), isolate);
  if (info->IsWasm() || receiver_or_instance->IsNullOrUndefined(isolate)) {
    return isolate->factory()->null_value();
  }

  Handle<JSReceiver> receiver =
      JSReceiver::ToObject(isolate, receiver_or_instance).ToHandleChecked();
  Handle<JSFunction> function =
      handle(JSFunction::cast(info->function()), isolate);
  Handle<String> name(function->shared().Name(), isolate);
  name = String::Flatten(isolate, name);

  // The static initializer function is not a method, so don't add a
  // class name, just return the function name.
  if (name->HasOneBytePrefix(CStrVector("<static_fields_initializer>"))) {
    return name;
  }

  // ES2015 gives getters and setters name prefixes which must
  // be stripped to find the property name.
  if (name->HasOneBytePrefix(CStrVector("get ")) ||
      name->HasOneBytePrefix(CStrVector("set "))) {
    name = isolate->factory()->NewProperSubString(name, 4, name->length());
  } else if (name->length() == 0) {
    // The function doesn't have a meaningful "name" property, however
    // the parser does store an inferred name "o.foo" for the common
    // case of `o.foo = function() {...}`, so see if we can derive a
    // property name to guess from that.
    name = handle(function->shared().inferred_name(), isolate);
    for (int index = name->length(); --index >= 0;) {
      if (name->Get(index, isolate) == '.') {
        name = isolate->factory()->NewProperSubString(name, index + 1,
                                                      name->length());
        break;
      }
    }
  }

  if (name->length() != 0) {
    LookupIterator::Key key(isolate, Handle<Name>::cast(name));
    LookupIterator it(isolate, receiver, key,
                      LookupIterator::PROTOTYPE_CHAIN_SKIP_INTERCEPTOR);
    if (it.state() == LookupIterator::DATA) {
      if (it.GetDataValue().is_identical_to(function)) {
        return name;
      }
    } else if (it.state() == LookupIterator::ACCESSOR) {
      Handle<Object> accessors = it.GetAccessors();
      if (accessors->IsAccessorPair()) {
        Handle<AccessorPair> pair = Handle<AccessorPair>::cast(accessors);
        if (pair->getter() == *function || pair->setter() == *function) {
          return name;
        }
      }
    }
  }

  return handle(InferMethodName(isolate, *receiver, *function), isolate);
}

// static
Handle<Object> StackFrameInfo::GetTypeName(Handle<StackFrameInfo> info) {
  Isolate* isolate = info->GetIsolate();
  if (!info->IsMethodCall()) {
    return isolate->factory()->null_value();
  }
  Handle<JSReceiver> receiver =
      JSReceiver::ToObject(isolate,
                           handle(info->receiver_or_instance(), isolate))
          .ToHandleChecked();
  if (receiver->IsJSProxy()) {
    return isolate->factory()->Proxy_string();
  }
  return JSReceiver::GetConstructorName(receiver);
}

uint32_t StackFrameInfo::GetWasmFunctionIndex() const {
  DCHECK(IsWasm());
  return Smi::ToInt(Smi::cast(function()));
}

WasmInstanceObject StackFrameInfo::GetWasmInstance() const {
  DCHECK(IsWasm());
  return WasmInstanceObject::cast(receiver_or_instance());
}

// static
int StackFrameInfo::GetSourcePosition(Handle<StackFrameInfo> info) {
  if (info->flags() & kIsSourcePositionComputed) {
    return info->code_offset_or_source_position();
  }
  DCHECK(!info->IsPromiseAll());
  DCHECK(!info->IsPromiseAny());
  int source_position =
      ComputeSourcePosition(info, info->code_offset_or_source_position());
  info->set_code_offset_or_source_position(source_position);
  info->set_flags(info->flags() | kIsSourcePositionComputed);
  return source_position;
}

// static
bool StackFrameInfo::ComputeLocation(Handle<StackFrameInfo> info,
                                     MessageLocation* location) {
  Isolate* isolate = info->GetIsolate();
  if (info->IsWasm()) {
    int pos = GetSourcePosition(info);
    Handle<Script> script(info->GetWasmInstance().module_object().script(),
                          isolate);
    *location = MessageLocation(script, pos, pos + 1);
    return true;
  }

  Handle<SharedFunctionInfo> shared(info->GetSharedFunctionInfo(), isolate);
  if (!shared->IsSubjectToDebugging()) return false;
  Handle<Script> script(Script::cast(shared->script()), isolate);
  if (script->source().IsUndefined()) return false;
  if (info->flags() & kIsSourcePositionComputed ||
      (shared->HasBytecodeArray() &&
       shared->GetBytecodeArray(isolate).HasSourcePositionTable())) {
    int pos = GetSourcePosition(info);
    *location = MessageLocation(script, pos, pos + 1, shared);
  } else {
    int code_offset = info->code_offset_or_source_position();
    *location = MessageLocation(script, shared, code_offset);
  }
  return true;
}

// static
int StackFrameInfo::ComputeSourcePosition(Handle<StackFrameInfo> info,
                                          int offset) {
  Isolate* isolate = info->GetIsolate();
  if (info->IsWasm()) {
    auto code_ref = Managed<wasm::GlobalWasmCodeRef>::cast(info->code_object());
    int byte_offset = code_ref.get()->code()->GetSourcePositionBefore(offset);
    auto module = info->GetWasmInstance().module();
    uint32_t func_index = info->GetWasmFunctionIndex();
    return wasm::GetSourcePosition(module, func_index, byte_offset,
                                   info->IsAsmJsAtNumberConversion());
  }
  Handle<SharedFunctionInfo> shared(info->GetSharedFunctionInfo(), isolate);
  SharedFunctionInfo::EnsureSourcePositionsAvailable(isolate, shared);
  return AbstractCode::cast(info->code_object()).SourcePosition(offset);
}

// static
Handle<Object> StackFrameInfo::GetWasmModuleName(Handle<StackFrameInfo> info) {
  Isolate* isolate = info->GetIsolate();
  if (info->IsWasm()) {
    Handle<String> name;
    auto module_object =
        handle(info->GetWasmInstance().module_object(), isolate);
    if (WasmModuleObject::GetModuleNameOrNull(isolate, module_object)
            .ToHandle(&name)) {
      return name;
    }
  }
  return isolate->factory()->null_value();
}

base::Optional<Script> StackFrameInfo::GetScript() const {
  if (IsWasm()) {
    return GetWasmInstance().module_object().script();
  }
  Object script = GetSharedFunctionInfo().script();
  if (script.IsScript()) return Script::cast(script);
  return base::nullopt;
}

SharedFunctionInfo StackFrameInfo::GetSharedFunctionInfo() const {
  DCHECK(!IsWasm());
  return JSFunction::cast(function()).shared();
}

// static
MaybeHandle<Script> StackFrameInfo::GetScript(Isolate* isolate,
                                              Handle<StackFrameInfo> info) {
  if (auto script = info->GetScript()) {
    return handle(*script, isolate);
  }
  return kNullMaybeHandle;
}

namespace {

bool IsNonEmptyString(Handle<Object> object) {
  return (object->IsString() && String::cast(*object).length() > 0);
}

void AppendFileLocation(Isolate* isolate, Handle<StackFrameInfo> frame,
                        IncrementalStringBuilder* builder) {
  Handle<Object> script_name_or_source_url(frame->GetScriptNameOrSourceURL(),
                                           isolate);
  if (!script_name_or_source_url->IsString() && frame->IsEval()) {
    builder->AppendString(
        Handle<String>::cast(StackFrameInfo::GetEvalOrigin(frame)));
    builder->AppendCString(", ");  // Expecting source position to follow.
  }

  if (IsNonEmptyString(script_name_or_source_url)) {
    builder->AppendString(Handle<String>::cast(script_name_or_source_url));
  } else {
    // Source code does not originate from a file and is not native, but we
    // can still get the source position inside the source string, e.g. in
    // an eval string.
    builder->AppendCString("<anonymous>");
  }

  int line_number = StackFrameInfo::GetLineNumber(frame);
  if (line_number != Message::kNoLineNumberInfo) {
    builder->AppendCharacter(':');
    builder->AppendInt(line_number);

    int column_number = StackFrameInfo::GetColumnNumber(frame);
    if (column_number != Message::kNoColumnInfo) {
      builder->AppendCharacter(':');
      builder->AppendInt(column_number);
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
  if (String::Equals(isolate, subject, pattern)) return true;

  FlatStringReader subject_reader(isolate, String::Flatten(isolate, subject));
  FlatStringReader pattern_reader(isolate, String::Flatten(isolate, pattern));

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

void AppendMethodCall(Isolate* isolate, Handle<StackFrameInfo> frame,
                      IncrementalStringBuilder* builder) {
  Handle<Object> type_name = StackFrameInfo::GetTypeName(frame);
  Handle<Object> method_name = StackFrameInfo::GetMethodName(frame);
  Handle<Object> function_name = StackFrameInfo::GetFunctionName(frame);

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

void SerializeJSStackFrame(Isolate* isolate, Handle<StackFrameInfo> frame,
                           IncrementalStringBuilder* builder) {
  Handle<Object> function_name = StackFrameInfo::GetFunctionName(frame);
  if (frame->IsAsync()) {
    builder->AppendCString("async ");
    if (frame->IsPromiseAll() || frame->IsPromiseAny()) {
      builder->AppendCString("Promise.");
      builder->AppendString(Handle<String>::cast(function_name));
      builder->AppendCString(" (index ");
      builder->AppendInt(StackFrameInfo::GetSourcePosition(frame));
      builder->AppendCString(")");
      return;
    }
  }
  if (frame->IsMethodCall()) {
    AppendMethodCall(isolate, frame, builder);
  } else if (frame->IsConstructor()) {
    builder->AppendCString("new ");
    if (IsNonEmptyString(function_name)) {
      builder->AppendString(Handle<String>::cast(function_name));
    } else {
      builder->AppendCString("<anonymous>");
    }
  } else if (IsNonEmptyString(function_name)) {
    builder->AppendString(Handle<String>::cast(function_name));
  } else {
    AppendFileLocation(isolate, frame, builder);
    return;
  }
  builder->AppendCString(" (");
  AppendFileLocation(isolate, frame, builder);
  builder->AppendCString(")");
}

bool IsAnonymousWasmScript(Isolate* isolate, Handle<Object> url) {
  Handle<String> prefix =
      isolate->factory()->NewStringFromStaticChars("wasm://wasm/");
  return StringIndexOf(isolate, Handle<String>::cast(url), prefix) == 0;
}

void SerializeWasmStackFrame(Isolate* isolate, Handle<StackFrameInfo> frame,
                             IncrementalStringBuilder* builder) {
  Handle<Object> module_name = StackFrameInfo::GetWasmModuleName(frame);
  Handle<Object> function_name = StackFrameInfo::GetFunctionName(frame);
  const bool has_name = !module_name->IsNull() || !function_name->IsNull();
  if (has_name) {
    if (module_name->IsNull()) {
      builder->AppendString(Handle<String>::cast(function_name));
    } else {
      builder->AppendString(Handle<String>::cast(module_name));
      if (!function_name->IsNull()) {
        builder->AppendCString(".");
        builder->AppendString(Handle<String>::cast(function_name));
      }
    }
    builder->AppendCString(" (");
  }

  Handle<Object> url(frame->GetScriptNameOrSourceURL(), isolate);
  if (IsNonEmptyString(url) && !IsAnonymousWasmScript(isolate, url)) {
    builder->AppendString(Handle<String>::cast(url));
  } else {
    builder->AppendCString("<anonymous>");
  }
  builder->AppendCString(":");

  const int wasm_func_index = frame->GetWasmFunctionIndex();
  builder->AppendCString("wasm-function[");
  builder->AppendInt(wasm_func_index);
  builder->AppendCString("]:");

  char buffer[16];
  SNPrintF(ArrayVector(buffer), "0x%x",
           StackFrameInfo::GetColumnNumber(frame) - 1);
  builder->AppendCString(buffer);

  if (has_name) builder->AppendCString(")");
}

}  // namespace

void SerializeStackFrameInfo(Isolate* isolate, Handle<StackFrameInfo> frame,
                             IncrementalStringBuilder* builder) {
  // Ordering here is important, as AsmJs frames are also marked as Wasm.
  if (frame->IsAsmJsWasm()) {
    SerializeJSStackFrame(isolate, frame, builder);
  } else if (frame->IsWasm()) {
    SerializeWasmStackFrame(isolate, frame, builder);
  } else {
    SerializeJSStackFrame(isolate, frame, builder);
  }
}

MaybeHandle<String> SerializeStackFrameInfo(Isolate* isolate,
                                            Handle<StackFrameInfo> frame) {
  IncrementalStringBuilder builder(isolate);
  SerializeStackFrameInfo(isolate, frame, &builder);
  return builder.Finish();
}

}  // namespace internal
}  // namespace v8
