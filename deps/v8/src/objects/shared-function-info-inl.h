// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_SHARED_FUNCTION_INFO_INL_H_
#define V8_OBJECTS_SHARED_FUNCTION_INFO_INL_H_

#include "src/base/macros.h"
#include "src/base/platform/mutex.h"
#include "src/handles/handles-inl.h"
#include "src/heap/heap-write-barrier-inl.h"
#include "src/heap/local-heap-inl.h"
#include "src/objects/debug-objects-inl.h"
#include "src/objects/feedback-vector-inl.h"
#include "src/objects/scope-info.h"
#include "src/objects/shared-function-info.h"
#include "src/objects/templates.h"

#if V8_ENABLE_WEBASSEMBLY
#include "src/wasm/wasm-module.h"
#include "src/wasm/wasm-objects-inl.h"
#endif  // V8_ENABLE_WEBASSEMBLY

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

#include "torque-generated/src/objects/shared-function-info-tq-inl.inc"

TQ_OBJECT_CONSTRUCTORS_IMPL(PreparseData)

int PreparseData::inner_start_offset() const {
  return InnerOffset(data_length());
}

ObjectSlot PreparseData::inner_data_start() const {
  return RawField(inner_start_offset());
}

void PreparseData::clear_padding() {
  int data_end_offset = kDataStartOffset + data_length();
  int padding_size = inner_start_offset() - data_end_offset;
  DCHECK_LE(0, padding_size);
  if (padding_size == 0) return;
  memset(reinterpret_cast<void*>(address() + data_end_offset), 0, padding_size);
}

byte PreparseData::get(int index) const {
  DCHECK_LE(0, index);
  DCHECK_LT(index, data_length());
  int offset = kDataStartOffset + index * kByteSize;
  return ReadField<byte>(offset);
}

void PreparseData::set(int index, byte value) {
  DCHECK_LE(0, index);
  DCHECK_LT(index, data_length());
  int offset = kDataStartOffset + index * kByteSize;
  WriteField<byte>(offset, value);
}

void PreparseData::copy_in(int index, const byte* buffer, int length) {
  DCHECK(index >= 0 && length >= 0 && length <= kMaxInt - index &&
         index + length <= this->data_length());
  Address dst_addr = field_address(kDataStartOffset + index * kByteSize);
  base::Memcpy(reinterpret_cast<void*>(dst_addr), buffer, length);
}

PreparseData PreparseData::get_child(int index) const {
  return PreparseData::cast(get_child_raw(index));
}

Object PreparseData::get_child_raw(int index) const {
  DCHECK_LE(0, index);
  DCHECK_LT(index, this->children_length());
  int offset = inner_start_offset() + index * kTaggedSize;
  return RELAXED_READ_FIELD(*this, offset);
}

void PreparseData::set_child(int index, PreparseData value,
                             WriteBarrierMode mode) {
  DCHECK_LE(0, index);
  DCHECK_LT(index, this->children_length());
  int offset = inner_start_offset() + index * kTaggedSize;
  RELAXED_WRITE_FIELD(*this, offset, value);
  CONDITIONAL_WRITE_BARRIER(*this, offset, value, mode);
}

TQ_OBJECT_CONSTRUCTORS_IMPL(UncompiledData)
TQ_OBJECT_CONSTRUCTORS_IMPL(UncompiledDataWithoutPreparseData)
TQ_OBJECT_CONSTRUCTORS_IMPL(UncompiledDataWithPreparseData)

TQ_OBJECT_CONSTRUCTORS_IMPL(BaselineData)

OBJECT_CONSTRUCTORS_IMPL(InterpreterData, Struct)

CAST_ACCESSOR(InterpreterData)
ACCESSORS(InterpreterData, bytecode_array, BytecodeArray, kBytecodeArrayOffset)
ACCESSORS(InterpreterData, interpreter_trampoline, Code,
          kInterpreterTrampolineOffset)

TQ_OBJECT_CONSTRUCTORS_IMPL(SharedFunctionInfo)
NEVER_READ_ONLY_SPACE_IMPL(SharedFunctionInfo)
DEFINE_DEOPT_ELEMENT_ACCESSORS(SharedFunctionInfo, Object)

RELEASE_ACQUIRE_ACCESSORS(SharedFunctionInfo, function_data, Object,
                          kFunctionDataOffset)
RELEASE_ACQUIRE_ACCESSORS(SharedFunctionInfo, name_or_scope_info, Object,
                          kNameOrScopeInfoOffset)
RELEASE_ACQUIRE_ACCESSORS(SharedFunctionInfo, script_or_debug_info, HeapObject,
                          kScriptOrDebugInfoOffset)

RENAME_TORQUE_ACCESSORS(SharedFunctionInfo,
                        raw_outer_scope_info_or_feedback_metadata,
                        outer_scope_info_or_feedback_metadata, HeapObject)
RENAME_UINT16_TORQUE_ACCESSORS(SharedFunctionInfo,
                               internal_formal_parameter_count,
                               formal_parameter_count)
RENAME_UINT16_TORQUE_ACCESSORS(SharedFunctionInfo, raw_function_token_offset,
                               function_token_offset)

RELAXED_INT32_ACCESSORS(SharedFunctionInfo, flags, kFlagsOffset)
UINT8_ACCESSORS(SharedFunctionInfo, flags2, kFlags2Offset)

bool SharedFunctionInfo::HasSharedName() const {
  Object value = name_or_scope_info(kAcquireLoad);
  if (value.IsScopeInfo()) {
    return ScopeInfo::cast(value).HasSharedFunctionName();
  }
  return value != kNoSharedNameSentinel;
}

String SharedFunctionInfo::Name() const {
  if (!HasSharedName()) return GetReadOnlyRoots().empty_string();
  Object value = name_or_scope_info(kAcquireLoad);
  if (value.IsScopeInfo()) {
    if (ScopeInfo::cast(value).HasFunctionName()) {
      return String::cast(ScopeInfo::cast(value).FunctionName());
    }
    return GetReadOnlyRoots().empty_string();
  }
  return String::cast(value);
}

void SharedFunctionInfo::SetName(String name) {
  Object maybe_scope_info = name_or_scope_info(kAcquireLoad);
  if (maybe_scope_info.IsScopeInfo()) {
    ScopeInfo::cast(maybe_scope_info).SetFunctionName(name);
  } else {
    DCHECK(maybe_scope_info.IsString() ||
           maybe_scope_info == kNoSharedNameSentinel);
    set_name_or_scope_info(name, kReleaseStore);
  }
  UpdateFunctionMapIndex();
}

bool SharedFunctionInfo::is_script() const {
  return scope_info().is_script_scope() &&
         Script::cast(script()).compilation_type() ==
             Script::COMPILATION_TYPE_HOST;
}

bool SharedFunctionInfo::needs_script_context() const {
  return is_script() && scope_info().ContextLocalCount() > 0;
}

template <typename LocalIsolate>
AbstractCode SharedFunctionInfo::abstract_code(LocalIsolate* isolate) {
  // TODO(v8:11429): Decide if this return bytecode or baseline code, when the
  // latter is present.
  if (HasBytecodeArray()) {
    return AbstractCode::cast(GetBytecodeArray(isolate));
  } else {
    return AbstractCode::cast(GetCode());
  }
}

int SharedFunctionInfo::function_token_position() const {
  int offset = raw_function_token_offset();
  if (offset == kFunctionTokenOutOfRange) {
    return kNoSourcePosition;
  } else {
    return StartPosition() - offset;
  }
}

template <typename LocalIsolate>
bool SharedFunctionInfo::AreSourcePositionsAvailable(
    LocalIsolate* isolate) const {
  if (FLAG_enable_lazy_source_positions) {
    return !HasBytecodeArray() ||
           GetBytecodeArray(isolate).HasSourcePositionTable();
  }
  return true;
}

template <typename LocalIsolate>
SharedFunctionInfo::Inlineability SharedFunctionInfo::GetInlineability(
    LocalIsolate* isolate) const {
  if (!script().IsScript()) return kHasNoScript;

  if (GetIsolate()->is_precise_binary_code_coverage() &&
      !has_reported_binary_coverage()) {
    // We may miss invocations if this function is inlined.
    return kNeedsBinaryCoverage;
  }

  if (optimization_disabled()) return kHasOptimizationDisabled;

  // Built-in functions are handled by the JSCallReducer.
  if (HasBuiltinId()) return kIsBuiltin;

  if (!IsUserJavaScript()) return kIsNotUserCode;

  // If there is no bytecode array, it is either not compiled or it is compiled
  // with WebAssembly for the asm.js pipeline. In either case we don't want to
  // inline.
  if (!HasBytecodeArray()) return kHasNoBytecode;

  if (GetBytecodeArray(isolate).length() > FLAG_max_inlined_bytecode_size) {
    return kExceedsBytecodeLimit;
  }

  if (HasBreakInfo()) return kMayContainBreakPoints;

  return kIsInlineable;
}

BIT_FIELD_ACCESSORS(SharedFunctionInfo, flags2, class_scope_has_private_brand,
                    SharedFunctionInfo::ClassScopeHasPrivateBrandBit)

BIT_FIELD_ACCESSORS(SharedFunctionInfo, flags2,
                    has_static_private_methods_or_accessors,
                    SharedFunctionInfo::HasStaticPrivateMethodsOrAccessorsBit)

BIT_FIELD_ACCESSORS(SharedFunctionInfo, flags2, may_have_cached_code,
                    SharedFunctionInfo::MayHaveCachedCodeBit)

BIT_FIELD_ACCESSORS(SharedFunctionInfo, flags, syntax_kind,
                    SharedFunctionInfo::FunctionSyntaxKindBits)

BIT_FIELD_ACCESSORS(SharedFunctionInfo, flags, allows_lazy_compilation,
                    SharedFunctionInfo::AllowLazyCompilationBit)
BIT_FIELD_ACCESSORS(SharedFunctionInfo, flags, has_duplicate_parameters,
                    SharedFunctionInfo::HasDuplicateParametersBit)

BIT_FIELD_ACCESSORS(SharedFunctionInfo, flags, native,
                    SharedFunctionInfo::IsNativeBit)
#if V8_ENABLE_WEBASSEMBLY
BIT_FIELD_ACCESSORS(SharedFunctionInfo, flags, is_asm_wasm_broken,
                    SharedFunctionInfo::IsAsmWasmBrokenBit)
#endif  // V8_ENABLE_WEBASSEMBLY
BIT_FIELD_ACCESSORS(SharedFunctionInfo, flags,
                    requires_instance_members_initializer,
                    SharedFunctionInfo::RequiresInstanceMembersInitializerBit)

BIT_FIELD_ACCESSORS(SharedFunctionInfo, flags, name_should_print_as_anonymous,
                    SharedFunctionInfo::NameShouldPrintAsAnonymousBit)
BIT_FIELD_ACCESSORS(SharedFunctionInfo, flags, has_reported_binary_coverage,
                    SharedFunctionInfo::HasReportedBinaryCoverageBit)

BIT_FIELD_ACCESSORS(SharedFunctionInfo, flags, is_toplevel,
                    SharedFunctionInfo::IsTopLevelBit)
BIT_FIELD_ACCESSORS(SharedFunctionInfo, flags,
                    is_oneshot_iife_or_properties_are_final,
                    SharedFunctionInfo::IsOneshotIifeOrPropertiesAreFinalBit)
BIT_FIELD_ACCESSORS(SharedFunctionInfo, flags,
                    private_name_lookup_skips_outer_class,
                    SharedFunctionInfo::PrivateNameLookupSkipsOuterClassBit)

bool SharedFunctionInfo::optimization_disabled() const {
  return disable_optimization_reason() != BailoutReason::kNoReason;
}

BailoutReason SharedFunctionInfo::disable_optimization_reason() const {
  return DisabledOptimizationReasonBits::decode(flags());
}

LanguageMode SharedFunctionInfo::language_mode() const {
  STATIC_ASSERT(LanguageModeSize == 2);
  return construct_language_mode(IsStrictBit::decode(flags()));
}

void SharedFunctionInfo::set_language_mode(LanguageMode language_mode) {
  STATIC_ASSERT(LanguageModeSize == 2);
  // We only allow language mode transitions that set the same language mode
  // again or go up in the chain:
  DCHECK(is_sloppy(this->language_mode()) || is_strict(language_mode));
  int hints = flags();
  hints = IsStrictBit::update(hints, is_strict(language_mode));
  set_flags(hints);
  UpdateFunctionMapIndex();
}

FunctionKind SharedFunctionInfo::kind() const {
  STATIC_ASSERT(FunctionKindBits::kSize == kFunctionKindBitSize);
  return FunctionKindBits::decode(flags());
}

void SharedFunctionInfo::set_kind(FunctionKind kind) {
  int hints = flags();
  hints = FunctionKindBits::update(hints, kind);
  hints = IsClassConstructorBit::update(hints, IsClassConstructor(kind));
  set_flags(hints);
  UpdateFunctionMapIndex();
}

bool SharedFunctionInfo::is_wrapped() const {
  return syntax_kind() == FunctionSyntaxKind::kWrapped;
}

bool SharedFunctionInfo::construct_as_builtin() const {
  return ConstructAsBuiltinBit::decode(flags());
}

void SharedFunctionInfo::CalculateConstructAsBuiltin() {
  bool uses_builtins_construct_stub = false;
  if (HasBuiltinId()) {
    int id = builtin_id();
    if (id != Builtins::kCompileLazy && id != Builtins::kEmptyFunction) {
      uses_builtins_construct_stub = true;
    }
  } else if (IsApiFunction()) {
    uses_builtins_construct_stub = true;
  }

  int f = flags();
  f = ConstructAsBuiltinBit::update(f, uses_builtins_construct_stub);
  set_flags(f);
}

int SharedFunctionInfo::function_map_index() const {
  // Note: Must be kept in sync with the FastNewClosure builtin.
  int index =
      Context::FIRST_FUNCTION_MAP_INDEX + FunctionMapIndexBits::decode(flags());
  DCHECK_LE(index, Context::LAST_FUNCTION_MAP_INDEX);
  return index;
}

void SharedFunctionInfo::set_function_map_index(int index) {
  STATIC_ASSERT(Context::LAST_FUNCTION_MAP_INDEX <=
                Context::FIRST_FUNCTION_MAP_INDEX + FunctionMapIndexBits::kMax);
  DCHECK_LE(Context::FIRST_FUNCTION_MAP_INDEX, index);
  DCHECK_LE(index, Context::LAST_FUNCTION_MAP_INDEX);
  index -= Context::FIRST_FUNCTION_MAP_INDEX;
  set_flags(FunctionMapIndexBits::update(flags(), index));
}

void SharedFunctionInfo::clear_padding() {
  memset(reinterpret_cast<void*>(this->address() + kSize), 0,
         kAlignedSize - kSize);
}

void SharedFunctionInfo::UpdateFunctionMapIndex() {
  int map_index =
      Context::FunctionMapIndex(language_mode(), kind(), HasSharedName());
  set_function_map_index(map_index);
}

void SharedFunctionInfo::DontAdaptArguments() {
#if V8_ENABLE_WEBASSEMBLY
  // TODO(leszeks): Revise this DCHECK now that the code field is gone.
  DCHECK(!HasWasmExportedFunctionData());
#endif  // V8_ENABLE_WEBASSEMBLY
  set_internal_formal_parameter_count(kDontAdaptArgumentsSentinel);
}

bool SharedFunctionInfo::IsInterpreted() const { return HasBytecodeArray(); }

ScopeInfo SharedFunctionInfo::scope_info() const {
  Object maybe_scope_info = name_or_scope_info(kAcquireLoad);
  if (maybe_scope_info.IsScopeInfo()) {
    return ScopeInfo::cast(maybe_scope_info);
  }
  return GetReadOnlyRoots().empty_scope_info();
}

void SharedFunctionInfo::SetScopeInfo(ScopeInfo scope_info,
                                      WriteBarrierMode mode) {
  // Move the existing name onto the ScopeInfo.
  Object name = name_or_scope_info(kAcquireLoad);
  if (name.IsScopeInfo()) {
    name = ScopeInfo::cast(name).FunctionName();
  }
  DCHECK(name.IsString() || name == kNoSharedNameSentinel);
  // Only set the function name for function scopes.
  scope_info.SetFunctionName(name);
  if (HasInferredName() && inferred_name().length() != 0) {
    scope_info.SetInferredFunctionName(inferred_name());
  }
  set_name_or_scope_info(scope_info, kReleaseStore, mode);
}

void SharedFunctionInfo::set_raw_scope_info(ScopeInfo scope_info,
                                            WriteBarrierMode mode) {
  WRITE_FIELD(*this, kNameOrScopeInfoOffset, scope_info);
  CONDITIONAL_WRITE_BARRIER(*this, kNameOrScopeInfoOffset, scope_info, mode);
}

HeapObject SharedFunctionInfo::outer_scope_info() const {
  DCHECK(!is_compiled());
  DCHECK(!HasFeedbackMetadata());
  return raw_outer_scope_info_or_feedback_metadata();
}

bool SharedFunctionInfo::HasOuterScopeInfo() const {
  ScopeInfo outer_info;
  if (!is_compiled()) {
    if (!outer_scope_info().IsScopeInfo()) return false;
    outer_info = ScopeInfo::cast(outer_scope_info());
  } else {
    if (!scope_info().HasOuterScopeInfo()) return false;
    outer_info = scope_info().OuterScopeInfo();
  }
  return !outer_info.IsEmpty();
}

ScopeInfo SharedFunctionInfo::GetOuterScopeInfo() const {
  DCHECK(HasOuterScopeInfo());
  if (!is_compiled()) return ScopeInfo::cast(outer_scope_info());
  return scope_info().OuterScopeInfo();
}

void SharedFunctionInfo::set_outer_scope_info(HeapObject value,
                                              WriteBarrierMode mode) {
  DCHECK(!is_compiled());
  DCHECK(raw_outer_scope_info_or_feedback_metadata().IsTheHole());
  DCHECK(value.IsScopeInfo() || value.IsTheHole());
  set_raw_outer_scope_info_or_feedback_metadata(value, mode);
}

bool SharedFunctionInfo::HasFeedbackMetadata() const {
  return raw_outer_scope_info_or_feedback_metadata().IsFeedbackMetadata();
}

FeedbackMetadata SharedFunctionInfo::feedback_metadata() const {
  DCHECK(HasFeedbackMetadata());
  return FeedbackMetadata::cast(raw_outer_scope_info_or_feedback_metadata());
}

void SharedFunctionInfo::set_feedback_metadata(FeedbackMetadata value,
                                               WriteBarrierMode mode) {
  DCHECK(!HasFeedbackMetadata());
  DCHECK(value.IsFeedbackMetadata());
  set_raw_outer_scope_info_or_feedback_metadata(value, mode);
}

bool SharedFunctionInfo::is_compiled() const {
  Object data = function_data(kAcquireLoad);
  return data != Smi::FromEnum(Builtins::kCompileLazy) &&
         !data.IsUncompiledData();
}

template <typename LocalIsolate>
IsCompiledScope SharedFunctionInfo::is_compiled_scope(
    LocalIsolate* isolate) const {
  return IsCompiledScope(*this, isolate);
}

IsCompiledScope::IsCompiledScope(const SharedFunctionInfo shared,
                                 Isolate* isolate)
    : retain_bytecode_(shared.HasBytecodeArray()
                           ? handle(shared.GetBytecodeArray(isolate), isolate)
                           : MaybeHandle<BytecodeArray>()),
      is_compiled_(shared.is_compiled()) {
  DCHECK_IMPLIES(!retain_bytecode_.is_null(), is_compiled());
}

IsCompiledScope::IsCompiledScope(const SharedFunctionInfo shared,
                                 LocalIsolate* isolate)
    : retain_bytecode_(shared.HasBytecodeArray()
                           ? isolate->heap()->NewPersistentHandle(
                                 shared.GetBytecodeArray(isolate))
                           : MaybeHandle<BytecodeArray>()),
      is_compiled_(shared.is_compiled()) {
  DCHECK_IMPLIES(!retain_bytecode_.is_null(), is_compiled());
}

bool SharedFunctionInfo::has_simple_parameters() {
  return scope_info().HasSimpleParameters();
}

bool SharedFunctionInfo::IsApiFunction() const {
  return function_data(kAcquireLoad).IsFunctionTemplateInfo();
}

FunctionTemplateInfo SharedFunctionInfo::get_api_func_data() const {
  DCHECK(IsApiFunction());
  return FunctionTemplateInfo::cast(function_data(kAcquireLoad));
}

bool SharedFunctionInfo::HasBytecodeArray() const {
  Object data = function_data(kAcquireLoad);
  return data.IsBytecodeArray() || data.IsInterpreterData() ||
         data.IsBaselineData();
}

template <typename LocalIsolate>
BytecodeArray SharedFunctionInfo::GetBytecodeArray(
    LocalIsolate* isolate) const {
  SharedMutexGuardIfOffThread<LocalIsolate, base::kShared> mutex_guard(
      GetIsolate()->shared_function_info_access(), isolate);

  DCHECK(HasBytecodeArray());
  if (HasDebugInfo() && GetDebugInfo().HasInstrumentedBytecodeArray()) {
    return GetDebugInfo().OriginalBytecodeArray();
  }

  return GetActiveBytecodeArray();
}

BytecodeArray BaselineData::GetActiveBytecodeArray() const {
  Object data = this->data();
  if (data.IsBytecodeArray()) {
    return BytecodeArray::cast(data);
  } else {
    DCHECK(data.IsInterpreterData());
    return InterpreterData::cast(data).bytecode_array();
  }
}

void BaselineData::SetActiveBytecodeArray(BytecodeArray bytecode) {
  Object data = this->data();
  if (data.IsBytecodeArray()) {
    set_data(bytecode);
  } else {
    DCHECK(data.IsInterpreterData());
    InterpreterData::cast(data).set_bytecode_array(bytecode);
  }
}

BytecodeArray SharedFunctionInfo::GetActiveBytecodeArray() const {
  Object data = function_data(kAcquireLoad);
  if (data.IsBytecodeArray()) {
    return BytecodeArray::cast(data);
  } else if (data.IsBaselineData()) {
    return baseline_data().GetActiveBytecodeArray();
  } else {
    DCHECK(data.IsInterpreterData());
    return InterpreterData::cast(data).bytecode_array();
  }
}

void SharedFunctionInfo::SetActiveBytecodeArray(BytecodeArray bytecode) {
  Object data = function_data(kAcquireLoad);
  if (data.IsBytecodeArray()) {
    set_function_data(bytecode, kReleaseStore);
  } else if (data.IsBaselineData()) {
    baseline_data().SetActiveBytecodeArray(bytecode);
  } else {
    DCHECK(data.IsInterpreterData());
    interpreter_data().set_bytecode_array(bytecode);
  }
}

void SharedFunctionInfo::set_bytecode_array(BytecodeArray bytecode) {
  DCHECK(function_data(kAcquireLoad) == Smi::FromEnum(Builtins::kCompileLazy) ||
         HasUncompiledData());
  set_function_data(bytecode, kReleaseStore);
}

bool SharedFunctionInfo::ShouldFlushBytecode(BytecodeFlushMode mode) {
  if (mode == BytecodeFlushMode::kDoNotFlushBytecode) return false;

  // TODO(rmcilroy): Enable bytecode flushing for resumable functions.
  if (IsResumableFunction(kind()) || !allows_lazy_compilation()) {
    return false;
  }

  // Get a snapshot of the function data field, and if it is a bytecode array,
  // check if it is old. Note, this is done this way since this function can be
  // called by the concurrent marker.
  Object data = function_data(kAcquireLoad);
  if (!data.IsBytecodeArray()) return false;

  if (mode == BytecodeFlushMode::kStressFlushBytecode) return true;

  BytecodeArray bytecode = BytecodeArray::cast(data);

  return bytecode.IsOld();
}

Code SharedFunctionInfo::InterpreterTrampoline() const {
  DCHECK(HasInterpreterData());
  return interpreter_data().interpreter_trampoline();
}

bool SharedFunctionInfo::HasInterpreterData() const {
  Object data = function_data(kAcquireLoad);
  if (data.IsBaselineData()) data = BaselineData::cast(data).data();
  return data.IsInterpreterData();
}

InterpreterData SharedFunctionInfo::interpreter_data() const {
  DCHECK(HasInterpreterData());
  Object data = function_data(kAcquireLoad);
  if (data.IsBaselineData()) data = BaselineData::cast(data).data();
  return InterpreterData::cast(data);
}

void SharedFunctionInfo::set_interpreter_data(
    InterpreterData interpreter_data) {
  DCHECK(FLAG_interpreted_frames_native_stack);
  DCHECK(!HasBaselineData());
  set_function_data(interpreter_data, kReleaseStore);
}

bool SharedFunctionInfo::HasBaselineData() const {
  return function_data(kAcquireLoad).IsBaselineData();
}

BaselineData SharedFunctionInfo::baseline_data() const {
  DCHECK(HasBaselineData());
  return BaselineData::cast(function_data(kAcquireLoad));
}

void SharedFunctionInfo::set_baseline_data(BaselineData baseline_data) {
  set_function_data(baseline_data, kReleaseStore);
}

void SharedFunctionInfo::flush_baseline_data() {
  DCHECK(HasBaselineData());
  set_function_data(baseline_data().data(), kReleaseStore);
}

#if V8_ENABLE_WEBASSEMBLY
bool SharedFunctionInfo::HasAsmWasmData() const {
  return function_data(kAcquireLoad).IsAsmWasmData();
}

bool SharedFunctionInfo::HasWasmExportedFunctionData() const {
  return function_data(kAcquireLoad).IsWasmExportedFunctionData();
}

bool SharedFunctionInfo::HasWasmJSFunctionData() const {
  return function_data(kAcquireLoad).IsWasmJSFunctionData();
}

bool SharedFunctionInfo::HasWasmCapiFunctionData() const {
  return function_data(kAcquireLoad).IsWasmCapiFunctionData();
}

AsmWasmData SharedFunctionInfo::asm_wasm_data() const {
  DCHECK(HasAsmWasmData());
  return AsmWasmData::cast(function_data(kAcquireLoad));
}

void SharedFunctionInfo::set_asm_wasm_data(AsmWasmData data) {
  DCHECK(function_data(kAcquireLoad) == Smi::FromEnum(Builtins::kCompileLazy) ||
         HasUncompiledData() || HasAsmWasmData());
  set_function_data(data, kReleaseStore);
}

const wasm::WasmModule* SharedFunctionInfo::wasm_module() const {
  if (!HasWasmExportedFunctionData()) return nullptr;
  const WasmExportedFunctionData& function_data = wasm_exported_function_data();
  const WasmInstanceObject& wasm_instance = function_data.instance();
  const WasmModuleObject& wasm_module_object = wasm_instance.module_object();
  return wasm_module_object.module();
}

const wasm::FunctionSig* SharedFunctionInfo::wasm_function_signature() const {
  const wasm::WasmModule* module = wasm_module();
  if (!module) return nullptr;
  const WasmExportedFunctionData& function_data = wasm_exported_function_data();
  DCHECK_LT(function_data.function_index(), module->functions.size());
  return module->functions[function_data.function_index()].sig;
}
#endif  // V8_ENABLE_WEBASSEMBLY

bool SharedFunctionInfo::HasBuiltinId() const {
  return function_data(kAcquireLoad).IsSmi();
}

int SharedFunctionInfo::builtin_id() const {
  DCHECK(HasBuiltinId());
  int id = Smi::ToInt(function_data(kAcquireLoad));
  DCHECK(Builtins::IsBuiltinId(id));
  return id;
}

void SharedFunctionInfo::set_builtin_id(int builtin_id) {
  DCHECK(Builtins::IsBuiltinId(builtin_id));
  set_function_data(Smi::FromInt(builtin_id), kReleaseStore,
                    SKIP_WRITE_BARRIER);
}

bool SharedFunctionInfo::HasUncompiledData() const {
  return function_data(kAcquireLoad).IsUncompiledData();
}

UncompiledData SharedFunctionInfo::uncompiled_data() const {
  DCHECK(HasUncompiledData());
  return UncompiledData::cast(function_data(kAcquireLoad));
}

void SharedFunctionInfo::set_uncompiled_data(UncompiledData uncompiled_data) {
  DCHECK(function_data(kAcquireLoad) == Smi::FromEnum(Builtins::kCompileLazy) ||
         HasUncompiledData());
  DCHECK(uncompiled_data.IsUncompiledData());
  set_function_data(uncompiled_data, kReleaseStore);
}

bool SharedFunctionInfo::HasUncompiledDataWithPreparseData() const {
  return function_data(kAcquireLoad).IsUncompiledDataWithPreparseData();
}

UncompiledDataWithPreparseData
SharedFunctionInfo::uncompiled_data_with_preparse_data() const {
  DCHECK(HasUncompiledDataWithPreparseData());
  return UncompiledDataWithPreparseData::cast(function_data(kAcquireLoad));
}

void SharedFunctionInfo::set_uncompiled_data_with_preparse_data(
    UncompiledDataWithPreparseData uncompiled_data_with_preparse_data) {
  DCHECK(function_data(kAcquireLoad) == Smi::FromEnum(Builtins::kCompileLazy));
  DCHECK(uncompiled_data_with_preparse_data.IsUncompiledDataWithPreparseData());
  set_function_data(uncompiled_data_with_preparse_data, kReleaseStore);
}

bool SharedFunctionInfo::HasUncompiledDataWithoutPreparseData() const {
  return function_data(kAcquireLoad).IsUncompiledDataWithoutPreparseData();
}

void SharedFunctionInfo::ClearPreparseData() {
  DCHECK(HasUncompiledDataWithPreparseData());
  UncompiledDataWithPreparseData data = uncompiled_data_with_preparse_data();

  // Trim off the pre-parsed scope data from the uncompiled data by swapping the
  // map, leaving only an uncompiled data without pre-parsed scope.
  DisallowGarbageCollection no_gc;
  Heap* heap = GetHeapFromWritableObject(data);

  // Swap the map.
  heap->NotifyObjectLayoutChange(data, no_gc);
  STATIC_ASSERT(UncompiledDataWithoutPreparseData::kSize <
                UncompiledDataWithPreparseData::kSize);
  STATIC_ASSERT(UncompiledDataWithoutPreparseData::kSize ==
                UncompiledData::kHeaderSize);
  data.synchronized_set_map(
      GetReadOnlyRoots().uncompiled_data_without_preparse_data_map());

  // Fill the remaining space with filler.
  heap->CreateFillerObjectAt(
      data.address() + UncompiledDataWithoutPreparseData::kSize,
      UncompiledDataWithPreparseData::kSize -
          UncompiledDataWithoutPreparseData::kSize,
      ClearRecordedSlots::kYes);

  // Ensure that the clear was successful.
  DCHECK(HasUncompiledDataWithoutPreparseData());
}

void UncompiledData::InitAfterBytecodeFlush(
    String inferred_name, int start_position, int end_position,
    std::function<void(HeapObject object, ObjectSlot slot, HeapObject target)>
        gc_notify_updated_slot) {
  set_inferred_name(inferred_name);
  gc_notify_updated_slot(*this, RawField(UncompiledData::kInferredNameOffset),
                         inferred_name);
  set_start_position(start_position);
  set_end_position(end_position);
}

HeapObject SharedFunctionInfo::script() const {
  HeapObject maybe_script = script_or_debug_info(kAcquireLoad);
  if (maybe_script.IsDebugInfo()) {
    return DebugInfo::cast(maybe_script).script();
  }
  return maybe_script;
}

void SharedFunctionInfo::set_script(HeapObject script) {
  HeapObject maybe_debug_info = script_or_debug_info(kAcquireLoad);
  if (maybe_debug_info.IsDebugInfo()) {
    DebugInfo::cast(maybe_debug_info).set_script(script);
  } else {
    set_script_or_debug_info(script, kReleaseStore);
  }
}

bool SharedFunctionInfo::is_repl_mode() const {
  return script().IsScript() && Script::cast(script()).is_repl_mode();
}

bool SharedFunctionInfo::HasDebugInfo() const {
  return script_or_debug_info(kAcquireLoad).IsDebugInfo();
}

DebugInfo SharedFunctionInfo::GetDebugInfo() const {
  auto debug_info = script_or_debug_info(kAcquireLoad);
  DCHECK(debug_info.IsDebugInfo());
  return DebugInfo::cast(debug_info);
}

void SharedFunctionInfo::SetDebugInfo(DebugInfo debug_info) {
  DCHECK(!HasDebugInfo());
  DCHECK_EQ(debug_info.script(), script_or_debug_info(kAcquireLoad));
  set_script_or_debug_info(debug_info, kReleaseStore);
}

bool SharedFunctionInfo::HasInferredName() {
  Object scope_info = name_or_scope_info(kAcquireLoad);
  if (scope_info.IsScopeInfo()) {
    return ScopeInfo::cast(scope_info).HasInferredFunctionName();
  }
  return HasUncompiledData();
}

String SharedFunctionInfo::inferred_name() {
  Object maybe_scope_info = name_or_scope_info(kAcquireLoad);
  if (maybe_scope_info.IsScopeInfo()) {
    ScopeInfo scope_info = ScopeInfo::cast(maybe_scope_info);
    if (scope_info.HasInferredFunctionName()) {
      Object name = scope_info.InferredFunctionName();
      if (name.IsString()) return String::cast(name);
    }
  } else if (HasUncompiledData()) {
    return uncompiled_data().inferred_name();
  }
  return GetReadOnlyRoots().empty_string();
}

bool SharedFunctionInfo::IsUserJavaScript() const {
  Object script_obj = script();
  if (script_obj.IsUndefined()) return false;
  Script script = Script::cast(script_obj);
  return script.IsUserJavaScript();
}

bool SharedFunctionInfo::IsSubjectToDebugging() const {
#if V8_ENABLE_WEBASSEMBLY
  if (HasAsmWasmData()) return false;
#endif  // V8_ENABLE_WEBASSEMBLY
  return IsUserJavaScript();
}

bool SharedFunctionInfo::CanDiscardCompiled() const {
#if V8_ENABLE_WEBASSEMBLY
  if (HasAsmWasmData()) return true;
#endif  // V8_ENABLE_WEBASSEMBLY
  return HasBytecodeArray() || HasUncompiledDataWithPreparseData() ||
         HasBaselineData();
}

bool SharedFunctionInfo::is_class_constructor() const {
  return IsClassConstructorBit::decode(flags());
}

bool SharedFunctionInfo::is_oneshot_iife() const {
  bool bit = is_oneshot_iife_or_properties_are_final();
  return bit && !is_class_constructor();
}

void SharedFunctionInfo::set_is_oneshot_iife(bool value) {
  DCHECK(!value || !is_class_constructor());
  if (!is_class_constructor()) {
    set_is_oneshot_iife_or_properties_are_final(value);
  }
}

void SharedFunctionInfo::set_are_properties_final(bool value) {
  if (is_class_constructor()) {
    set_is_oneshot_iife_or_properties_are_final(value);
  }
}

bool SharedFunctionInfo::are_properties_final() const {
  bool bit = is_oneshot_iife_or_properties_are_final();
  return bit && is_class_constructor();
}

}  // namespace internal
}  // namespace v8

#include "src/base/platform/wrappers.h"
#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_SHARED_FUNCTION_INFO_INL_H_
