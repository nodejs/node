// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/debug/debug-interface.h"

#include "src/api/api-inl.h"
#include "src/debug/debug-coverage.h"
#include "src/debug/debug-evaluate.h"
#include "src/debug/debug-property-iterator.h"
#include "src/debug/debug-type-profile.h"
#include "src/debug/debug-wasm-objects-inl.h"
#include "src/debug/debug.h"
#include "src/execution/vm-state-inl.h"
#include "src/objects/js-generator-inl.h"
#include "src/regexp/regexp-stack.h"

// Has to be the last include (doesn't have include guards):
#include "src/api/api-macros.h"

namespace v8 {
namespace debug {

void SetContextId(Local<Context> context, int id) {
  Utils::OpenHandle(*context)->set_debug_context_id(i::Smi::FromInt(id));
}

int GetContextId(Local<Context> context) {
  i::Object value = Utils::OpenHandle(*context)->debug_context_id();
  return (value.IsSmi()) ? i::Smi::ToInt(value) : 0;
}

void SetInspector(Isolate* isolate, v8_inspector::V8Inspector* inspector) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  i_isolate->set_inspector(inspector);
}

v8_inspector::V8Inspector* GetInspector(Isolate* isolate) {
  return reinterpret_cast<i::Isolate*>(isolate)->inspector();
}

void SetBreakOnNextFunctionCall(Isolate* isolate) {
  reinterpret_cast<i::Isolate*>(isolate)->debug()->SetBreakOnNextFunctionCall();
}

void ClearBreakOnNextFunctionCall(Isolate* isolate) {
  reinterpret_cast<i::Isolate*>(isolate)
      ->debug()
      ->ClearBreakOnNextFunctionCall();
}

MaybeLocal<Array> GetInternalProperties(Isolate* v8_isolate,
                                        Local<Value> value) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate);
  i::Handle<i::Object> val = Utils::OpenHandle(*value);
  i::Handle<i::JSArray> result;
  if (!i::Runtime::GetInternalProperties(isolate, val).ToHandle(&result))
    return MaybeLocal<Array>();
  return Utils::ToLocal(result);
}

namespace {

void CollectPrivateMethodsAndAccessorsFromContext(
    i::Isolate* isolate, i::Handle<i::Context> context,
    i::IsStaticFlag is_static_flag, std::vector<Local<Value>>* names_out,
    std::vector<Local<Value>>* values_out) {
  i::Handle<i::ScopeInfo> scope_info(context->scope_info(), isolate);
  int local_count = scope_info->ContextLocalCount();
  for (int j = 0; j < local_count; ++j) {
    i::VariableMode mode = scope_info->ContextLocalMode(j);
    i::IsStaticFlag flag = scope_info->ContextLocalIsStaticFlag(j);
    if (!i::IsPrivateMethodOrAccessorVariableMode(mode) ||
        flag != is_static_flag) {
      continue;
    }

    i::Handle<i::String> name(scope_info->ContextLocalName(j), isolate);
    int context_index = scope_info->ContextHeaderLength() + j;
    i::Handle<i::Object> slot_value(context->get(context_index), isolate);
    DCHECK_IMPLIES(mode == i::VariableMode::kPrivateMethod,
                   slot_value->IsJSFunction());
    DCHECK_IMPLIES(mode != i::VariableMode::kPrivateMethod,
                   slot_value->IsAccessorPair());
    names_out->push_back(Utils::ToLocal(name));
    values_out->push_back(Utils::ToLocal(slot_value));
  }
}

}  // namespace

bool GetPrivateMembers(Local<Context> context, Local<Object> value,
                       std::vector<Local<Value>>* names_out,
                       std::vector<Local<Value>>* values_out) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(context->GetIsolate());
  LOG_API(isolate, debug, GetPrivateMembers);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate);
  i::Handle<i::JSReceiver> receiver = Utils::OpenHandle(*value);
  i::Handle<i::JSArray> names;
  i::Handle<i::FixedArray> values;

  i::PropertyFilter key_filter =
      static_cast<i::PropertyFilter>(i::PropertyFilter::PRIVATE_NAMES_ONLY);
  i::Handle<i::FixedArray> keys;
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, keys,
      i::KeyAccumulator::GetKeys(receiver, i::KeyCollectionMode::kOwnOnly,
                                 key_filter,
                                 i::GetKeysConversion::kConvertToString),
      false);

  // Estimate number of private fields and private instance methods/accessors.
  int private_entries_count = 0;
  for (int i = 0; i < keys->length(); ++i) {
    // Exclude the private brand symbols.
    i::Handle<i::Symbol> key(i::Symbol::cast(keys->get(i)), isolate);
    if (key->is_private_brand()) {
      i::Handle<i::Object> value;
      ASSIGN_RETURN_ON_EXCEPTION_VALUE(
          isolate, value, i::Object::GetProperty(isolate, receiver, key),
          false);

      i::Handle<i::Context> context(i::Context::cast(*value), isolate);
      i::Handle<i::ScopeInfo> scope_info(context->scope_info(), isolate);
      // At least one slot contains the brand symbol so it does not count.
      private_entries_count += (scope_info->ContextLocalCount() - 1);
    } else {
      private_entries_count++;
    }
  }

  // Estimate number of static private methods/accessors for classes.
  bool has_static_private_methods_or_accessors = false;
  if (receiver->IsJSFunction()) {
    i::Handle<i::JSFunction> func(i::JSFunction::cast(*receiver), isolate);
    i::Handle<i::SharedFunctionInfo> shared(func->shared(), isolate);
    if (shared->is_class_constructor() &&
        shared->has_static_private_methods_or_accessors()) {
      has_static_private_methods_or_accessors = true;
      i::Handle<i::Context> context(func->context(), isolate);
      i::Handle<i::ScopeInfo> scope_info(context->scope_info(), isolate);
      int local_count = scope_info->ContextLocalCount();
      for (int j = 0; j < local_count; ++j) {
        i::VariableMode mode = scope_info->ContextLocalMode(j);
        i::IsStaticFlag is_static_flag =
            scope_info->ContextLocalIsStaticFlag(j);
        if (i::IsPrivateMethodOrAccessorVariableMode(mode) &&
            is_static_flag == i::IsStaticFlag::kStatic) {
          private_entries_count += local_count;
          break;
        }
      }
    }
  }

  DCHECK(names_out->empty());
  names_out->reserve(private_entries_count);
  DCHECK(values_out->empty());
  values_out->reserve(private_entries_count);

  if (has_static_private_methods_or_accessors) {
    i::Handle<i::Context> context(i::JSFunction::cast(*receiver).context(),
                                  isolate);
    CollectPrivateMethodsAndAccessorsFromContext(
        isolate, context, i::IsStaticFlag::kStatic, names_out, values_out);
  }

  for (int i = 0; i < keys->length(); ++i) {
    i::Handle<i::Object> obj_key(keys->get(i), isolate);
    i::Handle<i::Symbol> key(i::Symbol::cast(*obj_key), isolate);
    CHECK(key->is_private_name());
    i::Handle<i::Object> value;
    ASSIGN_RETURN_ON_EXCEPTION_VALUE(
        isolate, value, i::Object::GetProperty(isolate, receiver, key), false);

    if (key->is_private_brand()) {
      DCHECK(value->IsContext());
      i::Handle<i::Context> context(i::Context::cast(*value), isolate);
      CollectPrivateMethodsAndAccessorsFromContext(
          isolate, context, i::IsStaticFlag::kNotStatic, names_out, values_out);
    } else {  // Private fields
      i::Handle<i::String> name(
          i::String::cast(i::Symbol::cast(*key).description()), isolate);
      names_out->push_back(Utils::ToLocal(name));
      values_out->push_back(Utils::ToLocal(value));
    }
  }

  DCHECK_EQ(names_out->size(), values_out->size());
  DCHECK_LE(names_out->size(), private_entries_count);
  return true;
}

MaybeLocal<Context> GetCreationContext(Local<Object> value) {
  i::Handle<i::Object> val = Utils::OpenHandle(*value);
  if (val->IsJSGlobalProxy()) {
    return MaybeLocal<Context>();
  }
  return value->GetCreationContext();
}

void ChangeBreakOnException(Isolate* isolate, ExceptionBreakState type) {
  i::Isolate* internal_isolate = reinterpret_cast<i::Isolate*>(isolate);
  internal_isolate->debug()->ChangeBreakOnException(
      i::BreakException, type == BreakOnAnyException);
  internal_isolate->debug()->ChangeBreakOnException(i::BreakUncaughtException,
                                                    type != NoBreakOnException);
}

void SetBreakPointsActive(Isolate* v8_isolate, bool is_active) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate);
  isolate->debug()->set_break_points_active(is_active);
}

void PrepareStep(Isolate* v8_isolate, StepAction action) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  ENTER_V8_DO_NOT_USE(isolate);
  CHECK(isolate->debug()->CheckExecutionState());
  // Clear all current stepping setup.
  isolate->debug()->ClearStepping();
  // Prepare step.
  isolate->debug()->PrepareStep(static_cast<i::StepAction>(action));
}

void ClearStepping(Isolate* v8_isolate) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate);
  // Clear all current stepping setup.
  isolate->debug()->ClearStepping();
}

void BreakRightNow(Isolate* v8_isolate) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  ENTER_V8_DO_NOT_USE(isolate);
  isolate->debug()->HandleDebugBreak(i::kIgnoreIfAllFramesBlackboxed);
}

void SetTerminateOnResume(Isolate* v8_isolate) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate);
  isolate->debug()->SetTerminateOnResume();
}

bool CanBreakProgram(Isolate* v8_isolate) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  ENTER_V8_DO_NOT_USE(isolate);
  // We cannot break a program if we are currently running a regexp.
  // TODO(yangguo): fix this exception.
  return !isolate->regexp_stack()->is_in_use() &&
         isolate->debug()->AllFramesOnStackAreBlackboxed();
}

Isolate* Script::GetIsolate() const {
  return reinterpret_cast<Isolate*>(Utils::OpenHandle(this)->GetIsolate());
}

ScriptOriginOptions Script::OriginOptions() const {
  return Utils::OpenHandle(this)->origin_options();
}

bool Script::WasCompiled() const {
  return Utils::OpenHandle(this)->compilation_state() ==
         i::Script::COMPILATION_STATE_COMPILED;
}

bool Script::IsEmbedded() const {
  i::Handle<i::Script> script = Utils::OpenHandle(this);
  return script->context_data() ==
         script->GetReadOnlyRoots().uninitialized_symbol();
}

int Script::Id() const { return Utils::OpenHandle(this)->id(); }

int Script::LineOffset() const {
  return Utils::OpenHandle(this)->line_offset();
}

int Script::ColumnOffset() const {
  return Utils::OpenHandle(this)->column_offset();
}

std::vector<int> Script::LineEnds() const {
  i::Handle<i::Script> script = Utils::OpenHandle(this);
  if (script->type() == i::Script::TYPE_WASM) return std::vector<int>();

  i::Isolate* isolate = script->GetIsolate();
  i::HandleScope scope(isolate);
  i::Script::InitLineEnds(isolate, script);
  CHECK(script->line_ends().IsFixedArray());
  i::Handle<i::FixedArray> line_ends(i::FixedArray::cast(script->line_ends()),
                                     isolate);
  std::vector<int> result(line_ends->length());
  for (int i = 0; i < line_ends->length(); ++i) {
    i::Smi line_end = i::Smi::cast(line_ends->get(i));
    result[i] = line_end.value();
  }
  return result;
}

MaybeLocal<String> Script::Name() const {
  i::Handle<i::Script> script = Utils::OpenHandle(this);
  i::Isolate* isolate = script->GetIsolate();
  i::HandleScope handle_scope(isolate);
  i::Handle<i::Object> value(script->name(), isolate);
  if (!value->IsString()) return MaybeLocal<String>();
  return Utils::ToLocal(
      handle_scope.CloseAndEscape(i::Handle<i::String>::cast(value)));
}

MaybeLocal<String> Script::SourceURL() const {
  i::Handle<i::Script> script = Utils::OpenHandle(this);
  i::Isolate* isolate = script->GetIsolate();
  i::HandleScope handle_scope(isolate);
  i::Handle<i::Object> value(script->source_url(), isolate);
  if (!value->IsString()) return MaybeLocal<String>();
  return Utils::ToLocal(
      handle_scope.CloseAndEscape(i::Handle<i::String>::cast(value)));
}

MaybeLocal<String> Script::SourceMappingURL() const {
  i::Handle<i::Script> script = Utils::OpenHandle(this);
  i::Isolate* isolate = script->GetIsolate();
  i::HandleScope handle_scope(isolate);
  i::Handle<i::Object> value(script->source_mapping_url(), isolate);
  if (!value->IsString()) return MaybeLocal<String>();
  return Utils::ToLocal(
      handle_scope.CloseAndEscape(i::Handle<i::String>::cast(value)));
}

Maybe<int> Script::ContextId() const {
  i::Handle<i::Script> script = Utils::OpenHandle(this);
  i::Isolate* isolate = script->GetIsolate();
  i::HandleScope handle_scope(isolate);
  i::Object value = script->context_data();
  if (value.IsSmi()) return Just(i::Smi::ToInt(value));
  return Nothing<int>();
}

MaybeLocal<String> Script::Source() const {
  i::Handle<i::Script> script = Utils::OpenHandle(this);
  i::Isolate* isolate = script->GetIsolate();
  i::HandleScope handle_scope(isolate);
  i::Handle<i::Object> value(script->source(), isolate);
  if (!value->IsString()) return MaybeLocal<String>();
  return Utils::ToLocal(
      handle_scope.CloseAndEscape(i::Handle<i::String>::cast(value)));
}

bool Script::IsWasm() const {
  return Utils::OpenHandle(this)->type() == i::Script::TYPE_WASM;
}

bool Script::IsModule() const {
  return Utils::OpenHandle(this)->origin_options().IsModule();
}

namespace {

int GetSmiValue(i::Handle<i::FixedArray> array, int index) {
  return i::Smi::ToInt(array->get(index));
}

bool CompareBreakLocation(const i::BreakLocation& loc1,
                          const i::BreakLocation& loc2) {
  return loc1.position() < loc2.position();
}

}  // namespace

bool Script::GetPossibleBreakpoints(
    const Location& start, const Location& end, bool restrict_to_function,
    std::vector<BreakLocation>* locations) const {
  CHECK(!start.IsEmpty());
  i::Handle<i::Script> script = Utils::OpenHandle(this);
  if (script->type() == i::Script::TYPE_WASM) {
    i::wasm::NativeModule* native_module = script->wasm_native_module();
    return i::WasmScript::GetPossibleBreakpoints(native_module, start, end,
                                                 locations);
  }

  i::Isolate* isolate = script->GetIsolate();
  i::Script::InitLineEnds(isolate, script);
  CHECK(script->line_ends().IsFixedArray());
  i::Handle<i::FixedArray> line_ends =
      i::Handle<i::FixedArray>::cast(i::handle(script->line_ends(), isolate));
  CHECK(line_ends->length());

  int start_offset = GetSourceOffset(start);
  int end_offset = end.IsEmpty()
                       ? GetSmiValue(line_ends, line_ends->length() - 1) + 1
                       : GetSourceOffset(end);
  if (start_offset >= end_offset) return true;

  std::vector<i::BreakLocation> v8_locations;
  if (!isolate->debug()->GetPossibleBreakpoints(
          script, start_offset, end_offset, restrict_to_function,
          &v8_locations)) {
    return false;
  }

  std::sort(v8_locations.begin(), v8_locations.end(), CompareBreakLocation);
  int current_line_end_index = 0;
  for (const auto& v8_location : v8_locations) {
    int offset = v8_location.position();
    while (offset > GetSmiValue(line_ends, current_line_end_index)) {
      ++current_line_end_index;
      CHECK(current_line_end_index < line_ends->length());
    }
    int line_offset = 0;

    if (current_line_end_index > 0) {
      line_offset = GetSmiValue(line_ends, current_line_end_index - 1) + 1;
    }
    locations->emplace_back(
        current_line_end_index + script->line_offset(),
        offset - line_offset +
            (current_line_end_index == 0 ? script->column_offset() : 0),
        v8_location.type());
  }
  return true;
}

int Script::GetSourceOffset(const Location& location) const {
  i::Handle<i::Script> script = Utils::OpenHandle(this);
  if (script->type() == i::Script::TYPE_WASM) {
    DCHECK_EQ(0, location.GetLineNumber());
    return location.GetColumnNumber();
  }

  int line = std::max(location.GetLineNumber() - script->line_offset(), 0);
  int column = location.GetColumnNumber();
  if (line == 0) {
    column = std::max(0, column - script->column_offset());
  }

  i::Script::InitLineEnds(script->GetIsolate(), script);
  CHECK(script->line_ends().IsFixedArray());
  i::Handle<i::FixedArray> line_ends = i::Handle<i::FixedArray>::cast(
      i::handle(script->line_ends(), script->GetIsolate()));
  CHECK(line_ends->length());
  if (line >= line_ends->length())
    return GetSmiValue(line_ends, line_ends->length() - 1);
  int line_offset = GetSmiValue(line_ends, line);
  if (line == 0) return std::min(column, line_offset);
  int prev_line_offset = GetSmiValue(line_ends, line - 1);
  return std::min(prev_line_offset + column + 1, line_offset);
}

Location Script::GetSourceLocation(int offset) const {
  i::Handle<i::Script> script = Utils::OpenHandle(this);
  i::Script::PositionInfo info;
  i::Script::GetPositionInfo(script, offset, &info, i::Script::WITH_OFFSET);
  return Location(info.line, info.column);
}

bool Script::SetScriptSource(Local<String> newSource, bool preview,
                             LiveEditResult* result) const {
  i::Handle<i::Script> script = Utils::OpenHandle(this);
  i::Isolate* isolate = script->GetIsolate();
  return isolate->debug()->SetScriptSource(
      script, Utils::OpenHandle(*newSource), preview, result);
}

bool Script::SetBreakpoint(Local<String> condition, Location* location,
                           BreakpointId* id) const {
  i::Handle<i::Script> script = Utils::OpenHandle(this);
  i::Isolate* isolate = script->GetIsolate();
  int offset = GetSourceOffset(*location);
  if (!isolate->debug()->SetBreakPointForScript(
          script, Utils::OpenHandle(*condition), &offset, id)) {
    return false;
  }
  *location = GetSourceLocation(offset);
  return true;
}

bool Script::SetBreakpointOnScriptEntry(BreakpointId* id) const {
  i::Handle<i::Script> script = Utils::OpenHandle(this);
  i::Isolate* isolate = script->GetIsolate();
  if (script->type() == i::Script::TYPE_WASM) {
    int position = i::WasmScript::kOnEntryBreakpointPosition;
    return isolate->debug()->SetBreakPointForScript(
        script, isolate->factory()->empty_string(), &position, id);
  }
  i::SharedFunctionInfo::ScriptIterator it(isolate, *script);
  for (i::SharedFunctionInfo sfi = it.Next(); !sfi.is_null(); sfi = it.Next()) {
    if (sfi.is_toplevel()) {
      return isolate->debug()->SetBreakpointForFunction(
          handle(sfi, isolate), isolate->factory()->empty_string(), id);
    }
  }
  return false;
}

void Script::RemoveWasmBreakpoint(BreakpointId id) {
  i::Handle<i::Script> script = Utils::OpenHandle(this);
  i::Isolate* isolate = script->GetIsolate();
  isolate->debug()->RemoveBreakpointForWasmScript(script, id);
}

void RemoveBreakpoint(Isolate* v8_isolate, BreakpointId id) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  i::HandleScope handle_scope(isolate);
  isolate->debug()->RemoveBreakpoint(id);
}

Platform* GetCurrentPlatform() { return i::V8::GetCurrentPlatform(); }

void ForceGarbageCollection(
    Isolate* isolate,
    EmbedderHeapTracer::EmbedderStackState embedder_stack_state) {
  i::Heap* heap = reinterpret_cast<i::Isolate*>(isolate)->heap();
  heap->SetEmbedderStackStateForNextFinalization(embedder_stack_state);
  isolate->LowMemoryNotification();
}

WasmScript* WasmScript::Cast(Script* script) {
  CHECK(script->IsWasm());
  return static_cast<WasmScript*>(script);
}

WasmScript::DebugSymbolsType WasmScript::GetDebugSymbolType() const {
  i::Handle<i::Script> script = Utils::OpenHandle(this);
  DCHECK_EQ(i::Script::TYPE_WASM, script->type());
  switch (script->wasm_native_module()->module()->debug_symbols.type) {
    case i::wasm::WasmDebugSymbols::Type::None:
      return WasmScript::DebugSymbolsType::None;
    case i::wasm::WasmDebugSymbols::Type::EmbeddedDWARF:
      return WasmScript::DebugSymbolsType::EmbeddedDWARF;
    case i::wasm::WasmDebugSymbols::Type::ExternalDWARF:
      return WasmScript::DebugSymbolsType::ExternalDWARF;
    case i::wasm::WasmDebugSymbols::Type::SourceMap:
      return WasmScript::DebugSymbolsType::SourceMap;
  }
}

MemorySpan<const char> WasmScript::ExternalSymbolsURL() const {
  i::Handle<i::Script> script = Utils::OpenHandle(this);
  DCHECK_EQ(i::Script::TYPE_WASM, script->type());

  const i::wasm::WasmDebugSymbols& symbols =
      script->wasm_native_module()->module()->debug_symbols;
  if (symbols.external_url.is_empty()) return {};

  internal::wasm::ModuleWireBytes wire_bytes(
      script->wasm_native_module()->wire_bytes());
  i::wasm::WasmName external_url =
      wire_bytes.GetNameOrNull(symbols.external_url);
  return {external_url.data(), external_url.size()};
}

int WasmScript::NumFunctions() const {
  i::DisallowGarbageCollection no_gc;
  i::Handle<i::Script> script = Utils::OpenHandle(this);
  DCHECK_EQ(i::Script::TYPE_WASM, script->type());
  i::wasm::NativeModule* native_module = script->wasm_native_module();
  const i::wasm::WasmModule* module = native_module->module();
  DCHECK_GE(i::kMaxInt, module->functions.size());
  return static_cast<int>(module->functions.size());
}

int WasmScript::NumImportedFunctions() const {
  i::DisallowGarbageCollection no_gc;
  i::Handle<i::Script> script = Utils::OpenHandle(this);
  DCHECK_EQ(i::Script::TYPE_WASM, script->type());
  i::wasm::NativeModule* native_module = script->wasm_native_module();
  const i::wasm::WasmModule* module = native_module->module();
  DCHECK_GE(i::kMaxInt, module->num_imported_functions);
  return static_cast<int>(module->num_imported_functions);
}

MemorySpan<const uint8_t> WasmScript::Bytecode() const {
  i::Handle<i::Script> script = Utils::OpenHandle(this);
  i::Vector<const uint8_t> wire_bytes =
      script->wasm_native_module()->wire_bytes();
  return {wire_bytes.begin(), wire_bytes.size()};
}

std::pair<int, int> WasmScript::GetFunctionRange(int function_index) const {
  i::DisallowGarbageCollection no_gc;
  i::Handle<i::Script> script = Utils::OpenHandle(this);
  DCHECK_EQ(i::Script::TYPE_WASM, script->type());
  i::wasm::NativeModule* native_module = script->wasm_native_module();
  const i::wasm::WasmModule* module = native_module->module();
  DCHECK_LE(0, function_index);
  DCHECK_GT(module->functions.size(), function_index);
  const i::wasm::WasmFunction& func = module->functions[function_index];
  DCHECK_GE(i::kMaxInt, func.code.offset());
  DCHECK_GE(i::kMaxInt, func.code.end_offset());
  return std::make_pair(static_cast<int>(func.code.offset()),
                        static_cast<int>(func.code.end_offset()));
}

int WasmScript::GetContainingFunction(int byte_offset) const {
  i::DisallowGarbageCollection no_gc;
  i::Handle<i::Script> script = Utils::OpenHandle(this);
  DCHECK_EQ(i::Script::TYPE_WASM, script->type());
  i::wasm::NativeModule* native_module = script->wasm_native_module();
  const i::wasm::WasmModule* module = native_module->module();
  DCHECK_LE(0, byte_offset);

  return i::wasm::GetContainingWasmFunction(module, byte_offset);
}

uint32_t WasmScript::GetFunctionHash(int function_index) {
  i::DisallowGarbageCollection no_gc;
  i::Handle<i::Script> script = Utils::OpenHandle(this);
  DCHECK_EQ(i::Script::TYPE_WASM, script->type());
  i::wasm::NativeModule* native_module = script->wasm_native_module();
  const i::wasm::WasmModule* module = native_module->module();
  DCHECK_LE(0, function_index);
  DCHECK_GT(module->functions.size(), function_index);
  const i::wasm::WasmFunction& func = module->functions[function_index];
  i::wasm::ModuleWireBytes wire_bytes(native_module->wire_bytes());
  i::Vector<const i::byte> function_bytes = wire_bytes.GetFunctionBytes(&func);
  // TODO(herhut): Maybe also take module, name and signature into account.
  return i::StringHasher::HashSequentialString(function_bytes.begin(),
                                               function_bytes.length(), 0);
}

int WasmScript::CodeOffset() const {
  i::Handle<i::Script> script = Utils::OpenHandle(this);
  DCHECK_EQ(i::Script::TYPE_WASM, script->type());
  i::wasm::NativeModule* native_module = script->wasm_native_module();
  const i::wasm::WasmModule* module = native_module->module();

  // If the module contains at least one function, the code offset must have
  // been initialized, and it cannot be zero.
  DCHECK_IMPLIES(module->num_declared_functions > 0,
                 module->code.offset() != 0);
  return module->code.offset();
}

Location::Location(int line_number, int column_number)
    : line_number_(line_number),
      column_number_(column_number),
      is_empty_(false) {}

Location::Location()
    : line_number_(Function::kLineOffsetNotFound),
      column_number_(Function::kLineOffsetNotFound),
      is_empty_(true) {}

int Location::GetLineNumber() const {
  DCHECK(!IsEmpty());
  return line_number_;
}

int Location::GetColumnNumber() const {
  DCHECK(!IsEmpty());
  return column_number_;
}

bool Location::IsEmpty() const { return is_empty_; }

void GetLoadedScripts(Isolate* v8_isolate,
                      PersistentValueVector<Script>& scripts) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate);
  {
    i::DisallowGarbageCollection no_gc;
    i::Script::Iterator iterator(isolate);
    for (i::Script script = iterator.Next(); !script.is_null();
         script = iterator.Next()) {
      if (script.type() == i::Script::TYPE_NORMAL ||
          script.type() == i::Script::TYPE_WASM) {
        if (script.HasValidSource()) {
          i::HandleScope handle_scope(isolate);
          i::Handle<i::Script> script_handle(script, isolate);
          scripts.Append(ToApiHandle<Script>(script_handle));
        }
      }
    }
  }
}

MaybeLocal<UnboundScript> CompileInspectorScript(Isolate* v8_isolate,
                                                 Local<String> source) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  PREPARE_FOR_DEBUG_INTERFACE_EXECUTION_WITH_ISOLATE(isolate, UnboundScript);
  i::Handle<i::String> str = Utils::OpenHandle(*source);
  i::Handle<i::SharedFunctionInfo> result;
  {
    ScriptOriginOptions origin_options;
    i::ScriptData* script_data = nullptr;
    i::MaybeHandle<i::SharedFunctionInfo> maybe_function_info =
        i::Compiler::GetSharedFunctionInfoForScript(
            isolate, str, i::Compiler::ScriptDetails(), origin_options, nullptr,
            script_data, ScriptCompiler::kNoCompileOptions,
            ScriptCompiler::kNoCacheBecauseInspector,
            i::FLAG_expose_inspector_scripts ? i::NOT_NATIVES_CODE
                                             : i::INSPECTOR_CODE);
    has_pending_exception = !maybe_function_info.ToHandle(&result);
    RETURN_ON_FAILED_EXECUTION(UnboundScript);
  }
  RETURN_ESCAPED(ToApiHandle<UnboundScript>(result));
}

void TierDownAllModulesPerIsolate(Isolate* v8_isolate) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  isolate->wasm_engine()->TierDownAllModulesPerIsolate(isolate);
}

void TierUpAllModulesPerIsolate(Isolate* v8_isolate) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  isolate->wasm_engine()->TierUpAllModulesPerIsolate(isolate);
}

void SetDebugDelegate(Isolate* v8_isolate, DebugDelegate* delegate) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  isolate->debug()->SetDebugDelegate(delegate);
}

void SetAsyncEventDelegate(Isolate* v8_isolate, AsyncEventDelegate* delegate) {
  reinterpret_cast<i::Isolate*>(v8_isolate)->set_async_event_delegate(delegate);
}

void ResetBlackboxedStateCache(Isolate* v8_isolate, Local<Script> script) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate);
  i::DisallowGarbageCollection no_gc;
  i::SharedFunctionInfo::ScriptIterator iter(isolate,
                                             *Utils::OpenHandle(*script));
  for (i::SharedFunctionInfo info = iter.Next(); !info.is_null();
       info = iter.Next()) {
    if (info.HasDebugInfo()) {
      info.GetDebugInfo().set_computed_debug_is_blackboxed(false);
    }
  }
}

int EstimatedValueSize(Isolate* v8_isolate, Local<Value> value) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate);
  i::Handle<i::Object> object = Utils::OpenHandle(*value);
  if (object->IsSmi()) return i::kTaggedSize;
  CHECK(object->IsHeapObject());
  return i::Handle<i::HeapObject>::cast(object)->Size();
}

void AccessorPair::CheckCast(Value* that) {
  i::Handle<i::Object> obj = Utils::OpenHandle(that);
  Utils::ApiCheck(obj->IsAccessorPair(), "v8::debug::AccessorPair::Cast",
                  "Value is not a v8::debug::AccessorPair");
}

void WasmValueObject::CheckCast(Value* that) {
  i::Handle<i::Object> obj = Utils::OpenHandle(that);
  Utils::ApiCheck(obj->IsWasmValueObject(), "v8::debug::WasmValueObject::Cast",
                  "Value is not a v8::debug::WasmValueObject");
}

Local<Function> GetBuiltin(Isolate* v8_isolate, Builtin builtin) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate);
  i::HandleScope handle_scope(isolate);

  CHECK_EQ(builtin, kStringToLowerCase);
  i::Builtins::Name builtin_id = i::Builtins::kStringPrototypeToLocaleLowerCase;

  i::Factory* factory = isolate->factory();
  i::Handle<i::String> name = isolate->factory()->empty_string();
  i::Handle<i::NativeContext> context(isolate->native_context());
  i::Handle<i::SharedFunctionInfo> info =
      factory->NewSharedFunctionInfoForBuiltin(name, builtin_id);
  info->set_language_mode(i::LanguageMode::kStrict);
  i::Handle<i::JSFunction> fun =
      i::Factory::JSFunctionBuilder{isolate, info, context}
          .set_map(isolate->strict_function_without_prototype_map())
          .Build();

  fun->shared().set_internal_formal_parameter_count(0);
  fun->shared().set_length(0);
  return Utils::ToLocal(handle_scope.CloseAndEscape(fun));
}

void SetConsoleDelegate(Isolate* v8_isolate, ConsoleDelegate* delegate) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate);
  isolate->set_console_delegate(delegate);
}

ConsoleCallArguments::ConsoleCallArguments(
    const v8::FunctionCallbackInfo<v8::Value>& info)
    : v8::FunctionCallbackInfo<v8::Value>(nullptr, info.values_, info.length_) {
}

ConsoleCallArguments::ConsoleCallArguments(
    const internal::BuiltinArguments& args)
    : v8::FunctionCallbackInfo<v8::Value>(
          nullptr,
          // Drop the first argument (receiver, i.e. the "console" object).
          args.length() > 1 ? args.address_of_first_argument() : nullptr,
          args.length() - 1) {}

// Marked V8_DEPRECATED.
int GetStackFrameId(v8::Local<v8::StackFrame> frame) { return 0; }

v8::Local<v8::StackTrace> GetDetailedStackTrace(
    Isolate* v8_isolate, v8::Local<v8::Object> v8_error) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  i::Handle<i::JSReceiver> error = Utils::OpenHandle(*v8_error);
  if (!error->IsJSObject()) {
    return v8::Local<v8::StackTrace>();
  }
  i::Handle<i::FixedArray> stack_trace =
      isolate->GetDetailedStackTrace(i::Handle<i::JSObject>::cast(error));
  return Utils::StackTraceToLocal(stack_trace);
}

MaybeLocal<Script> GeneratorObject::Script() {
  i::Handle<i::JSGeneratorObject> obj = Utils::OpenHandle(this);
  i::Object maybe_script = obj->function().shared().script();
  if (!maybe_script.IsScript()) return {};
  i::Handle<i::Script> script(i::Script::cast(maybe_script), obj->GetIsolate());
  return ToApiHandle<v8::debug::Script>(script);
}

Local<Function> GeneratorObject::Function() {
  i::Handle<i::JSGeneratorObject> obj = Utils::OpenHandle(this);
  return Utils::ToLocal(handle(obj->function(), obj->GetIsolate()));
}

Location GeneratorObject::SuspendedLocation() {
  i::Handle<i::JSGeneratorObject> obj = Utils::OpenHandle(this);
  CHECK(obj->is_suspended());
  i::Object maybe_script = obj->function().shared().script();
  if (!maybe_script.IsScript()) return Location();
  i::Isolate* isolate = obj->GetIsolate();
  i::Handle<i::Script> script(i::Script::cast(maybe_script), isolate);
  i::Script::PositionInfo info;
  i::SharedFunctionInfo::EnsureSourcePositionsAvailable(
      isolate, i::handle(obj->function().shared(), isolate));
  i::Script::GetPositionInfo(script, obj->source_position(), &info,
                             i::Script::WITH_OFFSET);
  return Location(info.line, info.column);
}

bool GeneratorObject::IsSuspended() {
  return Utils::OpenHandle(this)->is_suspended();
}

v8::Local<GeneratorObject> GeneratorObject::Cast(v8::Local<v8::Value> value) {
  CHECK(value->IsGeneratorObject());
  return ToApiHandle<GeneratorObject>(Utils::OpenHandle(*value));
}

MaybeLocal<v8::Value> EvaluateGlobal(v8::Isolate* isolate,
                                     v8::Local<v8::String> source,
                                     EvaluateGlobalMode mode, bool repl) {
  i::Isolate* internal_isolate = reinterpret_cast<i::Isolate*>(isolate);
  PREPARE_FOR_DEBUG_INTERFACE_EXECUTION_WITH_ISOLATE(internal_isolate, Value);
  i::REPLMode repl_mode = repl ? i::REPLMode::kYes : i::REPLMode::kNo;
  Local<Value> result;
  has_pending_exception = !ToLocal<Value>(
      i::DebugEvaluate::Global(internal_isolate, Utils::OpenHandle(*source),
                               mode, repl_mode),
      &result);
  RETURN_ON_FAILED_EXECUTION(Value);
  RETURN_ESCAPED(result);
}

void QueryObjects(v8::Local<v8::Context> v8_context,
                  QueryObjectPredicate* predicate,
                  PersistentValueVector<v8::Object>* objects) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(v8_context->GetIsolate());
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate);
  isolate->heap_profiler()->QueryObjects(Utils::OpenHandle(*v8_context),
                                         predicate, objects);
}

void GlobalLexicalScopeNames(v8::Local<v8::Context> v8_context,
                             v8::PersistentValueVector<v8::String>* names) {
  i::Handle<i::Context> context = Utils::OpenHandle(*v8_context);
  i::Isolate* isolate = context->GetIsolate();
  i::Handle<i::ScriptContextTable> table(
      context->global_object().native_context().script_context_table(),
      isolate);
  for (int i = 0; i < table->synchronized_used(); i++) {
    i::Handle<i::Context> context =
        i::ScriptContextTable::GetContext(isolate, table, i);
    DCHECK(context->IsScriptContext());
    i::Handle<i::ScopeInfo> scope_info(context->scope_info(), isolate);
    int local_count = scope_info->ContextLocalCount();
    for (int j = 0; j < local_count; ++j) {
      i::String name = scope_info->ContextLocalName(j);
      if (i::ScopeInfo::VariableIsSynthetic(name)) continue;
      names->Append(Utils::ToLocal(handle(name, isolate)));
    }
  }
}

void SetReturnValue(v8::Isolate* v8_isolate, v8::Local<v8::Value> value) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  isolate->debug()->set_return_value(*Utils::OpenHandle(*value));
}

int64_t GetNextRandomInt64(v8::Isolate* v8_isolate) {
  return reinterpret_cast<i::Isolate*>(v8_isolate)
      ->random_number_generator()
      ->NextInt64();
}

void EnumerateRuntimeCallCounters(v8::Isolate* v8_isolate,
                                  RuntimeCallCounterCallback callback) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(v8_isolate);
  if (isolate->counters()) {
    isolate->counters()->runtime_call_stats()->EnumerateCounters(callback);
  }
}

int GetDebuggingId(v8::Local<v8::Function> function) {
  i::Handle<i::JSReceiver> callable = v8::Utils::OpenHandle(*function);
  if (!callable->IsJSFunction()) return i::DebugInfo::kNoDebuggingId;
  i::Handle<i::JSFunction> func = i::Handle<i::JSFunction>::cast(callable);
  int id = func->GetIsolate()->debug()->GetFunctionDebuggingId(func);
  DCHECK_NE(i::DebugInfo::kNoDebuggingId, id);
  return id;
}

bool SetFunctionBreakpoint(v8::Local<v8::Function> function,
                           v8::Local<v8::String> condition, BreakpointId* id) {
  i::Handle<i::JSReceiver> callable = Utils::OpenHandle(*function);
  if (!callable->IsJSFunction()) return false;
  i::Handle<i::JSFunction> jsfunction =
      i::Handle<i::JSFunction>::cast(callable);
  i::Isolate* isolate = jsfunction->GetIsolate();
  i::Handle<i::String> condition_string =
      condition.IsEmpty() ? isolate->factory()->empty_string()
                          : Utils::OpenHandle(*condition);
  return isolate->debug()->SetBreakpointForFunction(
      handle(jsfunction->shared(), isolate), condition_string, id);
}

PostponeInterruptsScope::PostponeInterruptsScope(v8::Isolate* isolate)
    : scope_(
          new i::PostponeInterruptsScope(reinterpret_cast<i::Isolate*>(isolate),
                                         i::StackGuard::API_INTERRUPT)) {}

PostponeInterruptsScope::~PostponeInterruptsScope() = default;

DisableBreakScope::DisableBreakScope(v8::Isolate* isolate)
    : scope_(std::make_unique<i::DisableBreak>(
          reinterpret_cast<i::Isolate*>(isolate)->debug())) {}

DisableBreakScope::~DisableBreakScope() = default;

int Coverage::BlockData::StartOffset() const { return block_->start; }

int Coverage::BlockData::EndOffset() const { return block_->end; }

uint32_t Coverage::BlockData::Count() const { return block_->count; }

int Coverage::FunctionData::StartOffset() const { return function_->start; }

int Coverage::FunctionData::EndOffset() const { return function_->end; }

uint32_t Coverage::FunctionData::Count() const { return function_->count; }

MaybeLocal<String> Coverage::FunctionData::Name() const {
  return ToApiHandle<String>(function_->name);
}

size_t Coverage::FunctionData::BlockCount() const {
  return function_->blocks.size();
}

bool Coverage::FunctionData::HasBlockCoverage() const {
  return function_->has_block_coverage;
}

Coverage::BlockData Coverage::FunctionData::GetBlockData(size_t i) const {
  return BlockData(&function_->blocks.at(i), coverage_);
}

Local<Script> Coverage::ScriptData::GetScript() const {
  return ToApiHandle<Script>(script_->script);
}

size_t Coverage::ScriptData::FunctionCount() const {
  return script_->functions.size();
}

Coverage::FunctionData Coverage::ScriptData::GetFunctionData(size_t i) const {
  return FunctionData(&script_->functions.at(i), coverage_);
}

Coverage::ScriptData::ScriptData(size_t index,
                                 std::shared_ptr<i::Coverage> coverage)
    : script_(&coverage->at(index)), coverage_(std::move(coverage)) {}

size_t Coverage::ScriptCount() const { return coverage_->size(); }

Coverage::ScriptData Coverage::GetScriptData(size_t i) const {
  return ScriptData(i, coverage_);
}

Coverage Coverage::CollectPrecise(Isolate* isolate) {
  return Coverage(
      i::Coverage::CollectPrecise(reinterpret_cast<i::Isolate*>(isolate)));
}

Coverage Coverage::CollectBestEffort(Isolate* isolate) {
  return Coverage(
      i::Coverage::CollectBestEffort(reinterpret_cast<i::Isolate*>(isolate)));
}

void Coverage::SelectMode(Isolate* isolate, CoverageMode mode) {
  i::Coverage::SelectMode(reinterpret_cast<i::Isolate*>(isolate), mode);
}

int TypeProfile::Entry::SourcePosition() const { return entry_->position; }

std::vector<MaybeLocal<String>> TypeProfile::Entry::Types() const {
  std::vector<MaybeLocal<String>> result;
  for (const internal::Handle<internal::String>& type : entry_->types) {
    result.emplace_back(ToApiHandle<String>(type));
  }
  return result;
}

TypeProfile::ScriptData::ScriptData(
    size_t index, std::shared_ptr<i::TypeProfile> type_profile)
    : script_(&type_profile->at(index)),
      type_profile_(std::move(type_profile)) {}

Local<Script> TypeProfile::ScriptData::GetScript() const {
  return ToApiHandle<Script>(script_->script);
}

std::vector<TypeProfile::Entry> TypeProfile::ScriptData::Entries() const {
  std::vector<TypeProfile::Entry> result;
  for (const internal::TypeProfileEntry& entry : script_->entries) {
    result.push_back(TypeProfile::Entry(&entry, type_profile_));
  }
  return result;
}

TypeProfile TypeProfile::Collect(Isolate* isolate) {
  return TypeProfile(
      i::TypeProfile::Collect(reinterpret_cast<i::Isolate*>(isolate)));
}

void TypeProfile::SelectMode(Isolate* isolate, TypeProfileMode mode) {
  i::TypeProfile::SelectMode(reinterpret_cast<i::Isolate*>(isolate), mode);
}

size_t TypeProfile::ScriptCount() const { return type_profile_->size(); }

TypeProfile::ScriptData TypeProfile::GetScriptData(size_t i) const {
  return ScriptData(i, type_profile_);
}

v8::MaybeLocal<v8::Value> WeakMap::Get(v8::Local<v8::Context> context,
                                       v8::Local<v8::Value> key) {
  PREPARE_FOR_EXECUTION(context, WeakMap, Get, Value);
  auto self = Utils::OpenHandle(this);
  Local<Value> result;
  i::Handle<i::Object> argv[] = {Utils::OpenHandle(*key)};
  has_pending_exception =
      !ToLocal<Value>(i::Execution::CallBuiltin(isolate, isolate->weakmap_get(),
                                                self, arraysize(argv), argv),
                      &result);
  RETURN_ON_FAILED_EXECUTION(Value);
  RETURN_ESCAPED(result);
}

v8::MaybeLocal<WeakMap> WeakMap::Set(v8::Local<v8::Context> context,
                                     v8::Local<v8::Value> key,
                                     v8::Local<v8::Value> value) {
  PREPARE_FOR_EXECUTION(context, WeakMap, Set, WeakMap);
  auto self = Utils::OpenHandle(this);
  i::Handle<i::Object> result;
  i::Handle<i::Object> argv[] = {Utils::OpenHandle(*key),
                                 Utils::OpenHandle(*value)};
  has_pending_exception =
      !i::Execution::CallBuiltin(isolate, isolate->weakmap_set(), self,
                                 arraysize(argv), argv)
           .ToHandle(&result);
  RETURN_ON_FAILED_EXECUTION(WeakMap);
  RETURN_ESCAPED(Local<WeakMap>::Cast(Utils::ToLocal(result)));
}

Local<WeakMap> WeakMap::New(v8::Isolate* isolate) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  LOG_API(i_isolate, WeakMap, New);
  ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate);
  i::Handle<i::JSWeakMap> obj = i_isolate->factory()->NewJSWeakMap();
  return ToApiHandle<WeakMap>(obj);
}

WeakMap* WeakMap::Cast(v8::Value* value) {
  return static_cast<WeakMap*>(value);
}

Local<Value> AccessorPair::getter() {
  i::Handle<i::AccessorPair> accessors = Utils::OpenHandle(this);
  i::Isolate* isolate = accessors->GetIsolate();
  i::Handle<i::Object> getter(accessors->getter(), isolate);
  return Utils::ToLocal(getter);
}

Local<Value> AccessorPair::setter() {
  i::Handle<i::AccessorPair> accessors = Utils::OpenHandle(this);
  i::Isolate* isolate = accessors->GetIsolate();
  i::Handle<i::Object> setter(accessors->setter(), isolate);
  return Utils::ToLocal(setter);
}

bool AccessorPair::IsAccessorPair(Local<Value> that) {
  i::Handle<i::Object> obj = Utils::OpenHandle(*that);
  return obj->IsAccessorPair();
}

bool WasmValueObject::IsWasmValueObject(Local<Value> that) {
  i::Handle<i::Object> obj = Utils::OpenHandle(*that);
  return obj->IsWasmValueObject();
}

MaybeLocal<Message> GetMessageFromPromise(Local<Promise> p) {
  i::Handle<i::JSPromise> promise = Utils::OpenHandle(*p);
  i::Isolate* isolate = promise->GetIsolate();

  i::Handle<i::Symbol> key = isolate->factory()->promise_debug_message_symbol();
  i::Handle<i::Object> maybeMessage =
      i::JSReceiver::GetDataProperty(promise, key);

  if (!maybeMessage->IsJSMessageObject(isolate)) return MaybeLocal<Message>();
  return ToApiHandle<Message>(
      i::Handle<i::JSMessageObject>::cast(maybeMessage));
}

std::unique_ptr<PropertyIterator> PropertyIterator::Create(
    Local<Context> context, Local<Object> object) {
  internal::Isolate* isolate =
      reinterpret_cast<i::Isolate*>(object->GetIsolate());
  if (IsExecutionTerminatingCheck(isolate)) {
    return nullptr;
  }
  CallDepthScope<false> call_depth_scope(isolate, context);

  auto result =
      i::DebugPropertyIterator::Create(isolate, Utils::OpenHandle(*object));
  if (!result) {
    DCHECK(isolate->has_pending_exception());
    call_depth_scope.Escape();
  }
  return result;
}

}  // namespace debug

namespace internal {

Maybe<bool> DebugPropertyIterator::Advance() {
  if (IsExecutionTerminatingCheck(isolate_)) {
    return Nothing<bool>();
  }
  Local<v8::Context> context =
      Utils::ToLocal(handle(isolate_->context(), isolate_));
  CallDepthScope<false> call_depth_scope(isolate_, context);

  if (!AdvanceInternal()) {
    DCHECK(isolate_->has_pending_exception());
    call_depth_scope.Escape();
    return Nothing<bool>();
  }
  return Just(true);
}

}  // namespace internal
}  // namespace v8

#include "src/api/api-macros-undef.h"
