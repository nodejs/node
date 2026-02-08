// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_SHARED_FUNCTION_INFO_INL_H_
#define V8_OBJECTS_SHARED_FUNCTION_INFO_INL_H_

#include "src/objects/shared-function-info.h"
// Include the non-inl header before the rest of the headers.

#include <optional>

#include "src/base/macros.h"
#include "src/base/platform/mutex.h"
#include "src/builtins/builtins.h"
#include "src/codegen/optimized-compilation-info.h"
#include "src/common/globals.h"
#include "src/handles/handles-inl.h"
#include "src/heap/heap-write-barrier-inl.h"
#include "src/objects/abstract-code-inl.h"
#include "src/objects/debug-objects-inl.h"
#include "src/objects/feedback-vector-inl.h"
#include "src/objects/heap-object-inl.h"
#include "src/objects/instance-type-inl.h"
#include "src/objects/objects-inl.h"
#include "src/objects/scope-info-inl.h"
#include "src/objects/script-inl.h"
#include "src/objects/string.h"
#include "src/objects/templates-inl.h"

#if V8_ENABLE_WEBASSEMBLY
#include "src/wasm/wasm-objects.h"
#endif  // V8_ENABLE_WEBASSEMBLY

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8::internal {

#include "torque-generated/src/objects/shared-function-info-tq-inl.inc"

// static
int PreparseData::SizeFor(int data_length, int children_length) {
  return OFFSET_OF_DATA_START(PreparseData) +
         ChildrenOffsetInData(data_length) +
         children_length * sizeof(TaggedMember<PreparseData>);
}

int PreparseData::children_start_offset() const {
  return OFFSET_OF_DATA_START(PreparseData) +
         ChildrenOffsetInData(data_length());
}

void PreparseData::clear_padding() {
  int data_end_offset = data_length() * sizeof(uint8_t);
  int padding_size = ChildrenOffsetInData(data_length()) - data_end_offset;
  DCHECK_LE(0, padding_size);
  if (padding_size == 0) return;
  memset(&data_and_children()[data_end_offset], 0, padding_size);
}

uint8_t PreparseData::get(int index) const {
  DCHECK_LE(0, index);
  DCHECK_LT(index, data_length());
  return data()[index];
}

void PreparseData::set(int index, uint8_t value) {
  DCHECK_LE(0, index);
  DCHECK_LT(index, data_length());
  data()[index] = value;
}

void PreparseData::copy_in(int index, const uint8_t* buffer, int length) {
  DCHECK(index >= 0 && length >= 0 && length <= kMaxInt - index &&
         index + length <= this->data_length());
  memcpy(&data()[index], buffer, length);
}

Tagged<PreparseData> PreparseData::get_child(int index) const {
  DCHECK_LE(0, index);
  DCHECK_LT(index, children_length());
  return children()[index].Relaxed_Load();
}

void PreparseData::set_child(int index, Tagged<PreparseData> value,
                             WriteBarrierMode mode) {
  DCHECK_LE(0, index);
  DCHECK_LT(index, children_length());
  children()[index].Relaxed_Store(this, value, mode);
}

Tagged<String> UncompiledData::inferred_name() const {
  return inferred_name_.load();
}
void UncompiledData::set_inferred_name(Tagged<String> value,
                                       WriteBarrierMode mode) {
  inferred_name_.store(this, value, mode);
}

Tagged<PreparseData> UncompiledDataWithPreparseData::preparse_data() const {
  return preparse_data_.load();
}
void UncompiledDataWithPreparseData::set_preparse_data(
    Tagged<PreparseData> value, WriteBarrierMode mode) {
  preparse_data_.store(this, value, mode);
}

Tagged<BytecodeArray> InterpreterData::bytecode_array() const {
  DCHECK(has_bytecode_array());
  return bytecode_array_.load();
}
void InterpreterData::set_bytecode_array(Tagged<BytecodeArray> value,
                                         WriteBarrierMode mode) {
  DCHECK(TrustedHeapLayout::IsOwnedByAnyHeap(this));
  bytecode_array_.store(this, value, mode);
}
bool InterpreterData::has_bytecode_array() const {
  return !bytecode_array_.load().is_null();
}
void InterpreterData::clear_bytecode_array() {
  bytecode_array_.store(this, {}, SKIP_WRITE_BARRIER);
}

Tagged<Code> InterpreterData::interpreter_trampoline() const {
  DCHECK(has_interpreter_trampoline());
  return interpreter_trampoline_.load();
}
void InterpreterData::set_interpreter_trampoline(Tagged<Code> value,
                                                 WriteBarrierMode mode) {
  DCHECK(TrustedHeapLayout::IsOwnedByAnyHeap(this));
  interpreter_trampoline_.store(this, value, mode);
}
bool InterpreterData::has_interpreter_trampoline() const {
  return !interpreter_trampoline_.load().is_null();
}
void InterpreterData::clear_interpreter_trampoline() {
  interpreter_trampoline_.store(this, {}, SKIP_WRITE_BARRIER);
}

TQ_OBJECT_CONSTRUCTORS_IMPL(SharedFunctionInfo)

RELEASE_ACQUIRE_ACCESSORS(SharedFunctionInfo, name_or_scope_info,
                          Tagged<NameOrScopeInfoT>, kNameOrScopeInfoOffset)
RELEASE_ACQUIRE_ACCESSORS(SharedFunctionInfo, script, Tagged<HeapObject>,
                          kScriptOffset)
RELEASE_ACQUIRE_ACCESSORS(SharedFunctionInfo, raw_script, Tagged<Object>,
                          kScriptOffset)

void SharedFunctionInfo::SetTrustedData(Tagged<ExposedTrustedObject> value,
                                        WriteBarrierMode mode) {
  WriteTrustedPointerField<kUnknownIndirectPointerTag>(
      kTrustedFunctionDataOffset, value);

  // Only one of trusted_function_data and untrusted_function_data can be in
  // use, so clear the untrusted data field. Using -1 here as cleared data value
  // allows HasBuiltinId to become quite simple, as it can just check if the
  // untrusted data is a Smi containing a valid builtin ID.
  constexpr int kClearedUntrustedFunctionDataValue = -1;
  static_assert(!Builtins::IsBuiltinId(kClearedUntrustedFunctionDataValue));
  TaggedField<Object, kUntrustedFunctionDataOffset>::Release_Store(
      *this, Smi::FromInt(kClearedUntrustedFunctionDataValue));

  CONDITIONAL_TRUSTED_POINTER_WRITE_BARRIER(*this, kTrustedFunctionDataOffset,
                                            kUnknownIndirectPointerTag, value,
                                            mode);
}

void SharedFunctionInfo::SetUntrustedData(Tagged<Object> value,
                                          WriteBarrierMode mode) {
  TaggedField<Object, kUntrustedFunctionDataOffset>::Release_Store(*this,
                                                                   value);

  // Only one of trusted_function_data and untrusted_function_data can be in
  // use, so clear the trusted data field.
  ClearTrustedPointerField(kTrustedFunctionDataOffset, kReleaseStore);

  CONDITIONAL_WRITE_BARRIER(*this, kUntrustedFunctionDataOffset, value, mode);
}

bool SharedFunctionInfo::HasTrustedData() const {
  return !IsTrustedPointerFieldEmpty(kTrustedFunctionDataOffset);
}

bool SharedFunctionInfo::HasUnpublishedTrustedData(
    IsolateForSandbox isolate) const {
  return IsTrustedPointerFieldUnpublished(kTrustedFunctionDataOffset,
                                          kUnknownIndirectPointerTag, isolate);
}

bool SharedFunctionInfo::HasUntrustedData() const { return !HasTrustedData(); }

Tagged<Object> SharedFunctionInfo::GetTrustedData(
    IsolateForSandbox isolate) const {
  return ReadMaybeEmptyTrustedPointerField<kUnknownIndirectPointerTag>(
      kTrustedFunctionDataOffset, isolate, kAcquireLoad);
}

template <typename T, IndirectPointerTag tag>
Tagged<T> SharedFunctionInfo::GetTrustedData(IsolateForSandbox isolate) const {
  static_assert(tag != kUnknownIndirectPointerTag);
  return HeapObject::ReadTrustedPointerField<tag>(kTrustedFunctionDataOffset,
                                                  isolate, kAcquireLoad);
}

Tagged<Object> SharedFunctionInfo::GetUntrustedData() const {
  return TaggedField<Object, kUntrustedFunctionDataOffset>::Acquire_Load(*this);
}

DEF_GETTER(SharedFunctionInfo, script, Tagged<HeapObject>) {
  return script(cage_base, kAcquireLoad);
}
bool SharedFunctionInfo::has_script(AcquireLoadTag tag) const {
  return IsScript(script(tag));
}

RENAME_TORQUE_ACCESSORS(SharedFunctionInfo,
                        raw_outer_scope_info_or_feedback_metadata,
                        outer_scope_info_or_feedback_metadata,
                        Tagged<UnionOf<ScopeInfo, FeedbackMetadata, TheHole>>)
DEF_ACQUIRE_GETTER(SharedFunctionInfo,
                   raw_outer_scope_info_or_feedback_metadata,
                   Tagged<UnionOf<ScopeInfo, FeedbackMetadata, TheHole>>) {
  return TaggedField<
      UnionOf<ScopeInfo, FeedbackMetadata, TheHole>,
      kOuterScopeInfoOrFeedbackMetadataOffset>::Acquire_Load(cage_base, *this);
}

uint16_t SharedFunctionInfo::internal_formal_parameter_count_with_receiver()
    const {
  const uint16_t param_count = TorqueGeneratedClass::formal_parameter_count();
  return param_count;
}

bool SharedFunctionInfo::IsSloppyNormalJSFunction() const {
  // TODO(dcarney): Fix the empty scope and push this down into
  //                ScopeInfo::IsSloppyNormalJSFunction.
  return kind() == FunctionKind::kNormalFunction && is_sloppy(language_mode());
}

uint32_t SharedFunctionInfo::unused_parameter_bits() const {
  DCHECK_EQ(scope_info(kAcquireLoad)->scope_type(), ScopeType::FUNCTION_SCOPE);
  return scope_info(kAcquireLoad)->unused_parameter_bits();
}

bool SharedFunctionInfo::CanOnlyAccessFixedFormalParameters() const {
  return scope_info(kAcquireLoad)->CanOnlyAccessFixedFormalParameters();
}

uint16_t SharedFunctionInfo::internal_formal_parameter_count_without_receiver()
    const {
  const uint16_t param_count = TorqueGeneratedClass::formal_parameter_count();
  if (param_count == kDontAdaptArgumentsSentinel) return param_count;
  return param_count - kJSArgcReceiverSlots;
}

void SharedFunctionInfo::set_internal_formal_parameter_count(int value) {
  DCHECK_EQ(value, static_cast<uint16_t>(value));
  DCHECK_GE(value, kJSArgcReceiverSlots);
  TorqueGeneratedClass::set_formal_parameter_count(value);
}

RENAME_PRIMITIVE_TORQUE_ACCESSORS(SharedFunctionInfo, raw_function_token_offset,
                                  function_token_offset, uint16_t)

RELAXED_INT32_ACCESSORS(SharedFunctionInfo, flags, kFlagsOffset)
RELAXED_INT32_ACCESSORS(SharedFunctionInfo, function_literal_id,
                        kFunctionLiteralIdOffset)
int32_t SharedFunctionInfo::relaxed_flags() const {
  return flags(kRelaxedLoad);
}
void SharedFunctionInfo::set_relaxed_flags(int32_t flags) {
  return set_flags(flags, kRelaxedStore);
}

UINT8_ACCESSORS(SharedFunctionInfo, flags2, kFlags2Offset)

bool SharedFunctionInfo::HasSharedName() const {
  Tagged<Object> value = name_or_scope_info(kAcquireLoad);
  if (IsScopeInfo(value)) {
    return Cast<ScopeInfo>(value)->HasSharedFunctionName();
  }
  return value != kNoSharedNameSentinel;
}

Tagged<String> SharedFunctionInfo::Name() const {
  if (!HasSharedName()) return GetReadOnlyRoots().empty_string();
  Tagged<Object> value = name_or_scope_info(kAcquireLoad);
  if (IsScopeInfo(value)) {
    if (Cast<ScopeInfo>(value)->HasFunctionName()) {
      return Cast<String>(Cast<ScopeInfo>(value)->FunctionName());
    }
    return GetReadOnlyRoots().empty_string();
  }
  return Cast<String>(value);
}

void SharedFunctionInfo::SetName(Tagged<String> name) {
  Tagged<Object> maybe_scope_info = name_or_scope_info(kAcquireLoad);
  if (IsScopeInfo(maybe_scope_info)) {
    Cast<ScopeInfo>(maybe_scope_info)->SetFunctionName(name);
  } else {
    DCHECK(IsString(maybe_scope_info) ||
           maybe_scope_info == kNoSharedNameSentinel);
    set_name_or_scope_info(name, kReleaseStore);
  }
  UpdateFunctionMapIndex();
}

bool SharedFunctionInfo::is_script() const {
  return scope_info(kAcquireLoad)->is_script_scope() &&
         Cast<Script>(script())->compilation_type() ==
             Script::CompilationType::kHost;
}

bool SharedFunctionInfo::needs_script_context() const {
  return is_script() && scope_info(kAcquireLoad)->ContextLocalCount() > 0;
}

Tagged<AbstractCode> SharedFunctionInfo::abstract_code(Isolate* isolate) {
  // TODO(v8:11429): Decide if this return bytecode or baseline code, when the
  // latter is present.
  if (HasBytecodeArray(isolate)) {
    return Cast<AbstractCode>(GetBytecodeArray(isolate));
  } else {
    return Cast<AbstractCode>(GetCode(isolate));
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

template <typename IsolateT>
bool SharedFunctionInfo::AreSourcePositionsAvailable(IsolateT* isolate) const {
  if (v8_flags.enable_lazy_source_positions) {
    return !HasBytecodeArray() ||
           GetBytecodeArray(isolate)->HasSourcePositionTable();
  }
  return true;
}

template <typename IsolateT>
SharedFunctionInfo::Inlineability SharedFunctionInfo::GetInlineability(
    CodeKind code_kind, IsolateT* isolate) const {
  if (!IsScript(script())) return kHasNoScript;

  if (isolate->is_precise_binary_code_coverage() &&
      !has_reported_binary_coverage()) {
    // We may miss invocations if this function is inlined.
    return kNeedsBinaryCoverage;
  }

  // Built-in functions are handled by the JSCallReducer.
  if (HasBuiltinId()) return kIsBuiltin;

  if (!IsUserJavaScript()) return kIsNotUserCode;

  // If there is no bytecode array, it is either not compiled or it is compiled
  // with WebAssembly for the asm.js pipeline. In either case we don't want to
  // inline.
  if (!HasBytecodeArray()) return kHasNoBytecode;

  if (GetBytecodeArray(isolate)->length() >
      v8_flags.max_inlined_bytecode_size) {
    return kExceedsBytecodeLimit;
  }

  {
    MutexGuardIfOffThread<IsolateT> mutex_guard(
        isolate->shared_function_info_access(), isolate);
    if (HasBreakInfo(isolate->GetMainThreadIsolateUnsafe())) {
      return kMayContainBreakPoints;
    }
  }

  if (optimization_disabled(code_kind)) return kHasOptimizationDisabled;

  return kIsInlineable;
}

BIT_FIELD_ACCESSORS(SharedFunctionInfo, flags2, class_scope_has_private_brand,
                    SharedFunctionInfo::ClassScopeHasPrivateBrandBit)

BIT_FIELD_ACCESSORS(SharedFunctionInfo, flags2,
                    has_static_private_methods_or_accessors,
                    SharedFunctionInfo::HasStaticPrivateMethodsOrAccessorsBit)

BIT_FIELD_ACCESSORS(SharedFunctionInfo, flags2, is_sparkplug_compiling,
                    SharedFunctionInfo::IsSparkplugCompilingBit)

BIT_FIELD_ACCESSORS(SharedFunctionInfo, flags2, maglev_compilation_failed,
                    SharedFunctionInfo::MaglevCompilationFailedBit)

BIT_FIELD_ACCESSORS(SharedFunctionInfo, flags2,
                    function_context_independent_compiled,
                    SharedFunctionInfo::FunctionContextIndependentCompiledBit)

BIT_FIELD_ACCESSORS(SharedFunctionInfo, relaxed_flags, syntax_kind,
                    SharedFunctionInfo::FunctionSyntaxKindBits)

BIT_FIELD_ACCESSORS(SharedFunctionInfo, relaxed_flags, allows_lazy_compilation,
                    SharedFunctionInfo::AllowLazyCompilationBit)
BIT_FIELD_ACCESSORS(SharedFunctionInfo, relaxed_flags, has_duplicate_parameters,
                    SharedFunctionInfo::HasDuplicateParametersBit)

BIT_FIELD_ACCESSORS(SharedFunctionInfo, relaxed_flags, native,
                    SharedFunctionInfo::IsNativeBit)
#if V8_ENABLE_WEBASSEMBLY
BIT_FIELD_ACCESSORS(SharedFunctionInfo, relaxed_flags, is_asm_wasm_broken,
                    SharedFunctionInfo::IsAsmWasmBrokenBit)
#endif  // V8_ENABLE_WEBASSEMBLY
BIT_FIELD_ACCESSORS(SharedFunctionInfo, relaxed_flags,
                    requires_instance_members_initializer,
                    SharedFunctionInfo::RequiresInstanceMembersInitializerBit)

BIT_FIELD_ACCESSORS(SharedFunctionInfo, relaxed_flags,
                    name_should_print_as_anonymous,
                    SharedFunctionInfo::NameShouldPrintAsAnonymousBit)
BIT_FIELD_ACCESSORS(SharedFunctionInfo, relaxed_flags,
                    has_reported_binary_coverage,
                    SharedFunctionInfo::HasReportedBinaryCoverageBit)

BIT_FIELD_ACCESSORS(SharedFunctionInfo, relaxed_flags, is_toplevel,
                    SharedFunctionInfo::IsTopLevelBit)
BIT_FIELD_ACCESSORS(SharedFunctionInfo, relaxed_flags, properties_are_final,
                    SharedFunctionInfo::PropertiesAreFinalBit)
BIT_FIELD_ACCESSORS(SharedFunctionInfo, relaxed_flags,
                    private_name_lookup_skips_outer_class,
                    SharedFunctionInfo::PrivateNameLookupSkipsOuterClassBit)
BIT_FIELD_ACCESSORS(SharedFunctionInfo, relaxed_flags, live_edited,
                    SharedFunctionInfo::LiveEditedBit)

bool SharedFunctionInfo::optimization_disabled(CodeKind kind) const {
  switch (kind) {
    case CodeKind::MAGLEV:
      return IsTerminalBailoutReasonForMaglev(disabled_optimization_reason());
    case CodeKind::TURBOFAN_JS:
      return IsTerminalBailoutReasonForTurbofan(disabled_optimization_reason());
    default:
      UNREACHABLE();
  }
}

bool SharedFunctionInfo::all_optimization_disabled() const {
  return IsTerminalBailoutReason(disabled_optimization_reason());
}

BailoutReason SharedFunctionInfo::disabled_optimization_reason() const {
  return DisabledOptimizationReasonBits::decode(flags(kRelaxedLoad));
}

LanguageMode SharedFunctionInfo::language_mode() const {
  static_assert(LanguageModeSize == 2);
  return construct_language_mode(IsStrictBit::decode(flags(kRelaxedLoad)));
}

void SharedFunctionInfo::set_language_mode(LanguageMode language_mode) {
  static_assert(LanguageModeSize == 2);
  // We only allow language mode transitions that set the same language mode
  // again or go up in the chain:
  DCHECK(is_sloppy(this->language_mode()) || is_strict(language_mode));
  int hints = flags(kRelaxedLoad);
  hints = IsStrictBit::update(hints, is_strict(language_mode));
  set_flags(hints, kRelaxedStore);
  UpdateFunctionMapIndex();
}

FunctionKind SharedFunctionInfo::kind() const {
  static_assert(FunctionKindBits::kSize == kFunctionKindBitSize);
  return FunctionKindBits::decode(flags(kRelaxedLoad));
}

void SharedFunctionInfo::set_kind(FunctionKind kind) {
  int hints = flags(kRelaxedLoad);
  hints = FunctionKindBits::update(hints, kind);
  hints = IsClassConstructorBit::update(hints, IsClassConstructor(kind));
  set_flags(hints, kRelaxedStore);
  UpdateFunctionMapIndex();
}

bool SharedFunctionInfo::is_wrapped() const {
  return syntax_kind() == FunctionSyntaxKind::kWrapped;
}

bool SharedFunctionInfo::construct_as_builtin() const {
  return ConstructAsBuiltinBit::decode(flags(kRelaxedLoad));
}

void SharedFunctionInfo::CalculateConstructAsBuiltin() {
  bool uses_builtins_construct_stub = false;
  if (HasBuiltinId()) {
    Builtin id = builtin_id();
    if (id != Builtin::kCompileLazy && id != Builtin::kEmptyFunction) {
      uses_builtins_construct_stub = true;
    }
  } else if (IsApiFunction()) {
    uses_builtins_construct_stub = true;
  }

  int f = flags(kRelaxedLoad);
  f = ConstructAsBuiltinBit::update(f, uses_builtins_construct_stub);
  set_flags(f, kRelaxedStore);
}

uint16_t SharedFunctionInfo::age() const {
  return RELAXED_READ_UINT16_FIELD(*this, kAgeOffset);
}

void SharedFunctionInfo::set_age(uint16_t value) {
  RELAXED_WRITE_UINT16_FIELD(*this, kAgeOffset, value);
}

uint16_t SharedFunctionInfo::CompareExchangeAge(uint16_t expected_age,
                                                uint16_t new_age) {
  Address age_addr = address() + kAgeOffset;
  return base::AsAtomic16::Relaxed_CompareAndSwap(
      reinterpret_cast<base::Atomic16*>(age_addr), expected_age, new_age);
}

int SharedFunctionInfo::function_map_index() const {
  // Note: Must be kept in sync with the FastNewClosure builtin.
  int index = Context::FIRST_FUNCTION_MAP_INDEX +
              FunctionMapIndexBits::decode(flags(kRelaxedLoad));
  DCHECK_LE(index, Context::LAST_FUNCTION_MAP_INDEX);
  return index;
}

void SharedFunctionInfo::set_function_map_index(int index) {
  static_assert(Context::LAST_FUNCTION_MAP_INDEX <=
                Context::FIRST_FUNCTION_MAP_INDEX + FunctionMapIndexBits::kMax);
  DCHECK_LE(Context::FIRST_FUNCTION_MAP_INDEX, index);
  DCHECK_LE(index, Context::LAST_FUNCTION_MAP_INDEX);
  index -= Context::FIRST_FUNCTION_MAP_INDEX;
  set_flags(FunctionMapIndexBits::update(flags(kRelaxedLoad), index),
            kRelaxedStore);
}

void SharedFunctionInfo::UpdateFunctionMapIndex() {
  int map_index =
      Context::FunctionMapIndex(language_mode(), kind(), HasSharedName());
  set_function_map_index(map_index);
}

void SharedFunctionInfo::DontAdaptArguments() {
#if V8_ENABLE_WEBASSEMBLY
  // TODO(leszeks): Revise this DCHECK now that the code field is gone.
  DCHECK(!HasWasmExportedFunctionData(GetCurrentIsolateForSandbox()));
#endif  // V8_ENABLE_WEBASSEMBLY
  if (HasBuiltinId()) {
    Builtin builtin = builtin_id();
    if (Builtins::KindOf(builtin) == Builtins::TFJ) {
      const int formal_parameter_count =
          Builtins::GetStackParameterCount(builtin);
      // If we have `kDontAdaptArgumentsSentinel` or no arguments, then we are
      // good. Otherwise this is a mismatch.
      if (formal_parameter_count != kDontAdaptArgumentsSentinel &&
          formal_parameter_count != JSParameterCount(0)) {
        FATAL(
            "Conflicting argument adaptation configuration (SFI vs call "
            "descriptor) for builtin: %s (%d)",
            Builtins::name(builtin), static_cast<int>(builtin));
      }
    }
  }
  TorqueGeneratedClass::set_formal_parameter_count(kDontAdaptArgumentsSentinel);
}

DEF_ACQUIRE_GETTER(SharedFunctionInfo, scope_info, Tagged<ScopeInfo>) {
  Tagged<Object> maybe_scope_info = name_or_scope_info(cage_base, kAcquireLoad);
  if (IsScopeInfo(maybe_scope_info, cage_base)) {
    return Cast<ScopeInfo>(maybe_scope_info);
  }
  return GetReadOnlyRoots().empty_scope_info();
}

DEF_GETTER(SharedFunctionInfo, scope_info, Tagged<ScopeInfo>) {
  return scope_info(cage_base, kAcquireLoad);
}

Tagged<ScopeInfo> SharedFunctionInfo::EarlyScopeInfo(AcquireLoadTag tag) {
  // Keep in sync with the scope_info getter above.
  PtrComprCageBase cage_base = GetPtrComprCageBase(*this);
  Tagged<Object> maybe_scope_info = name_or_scope_info(cage_base, tag);
  if (IsScopeInfo(maybe_scope_info, cage_base)) {
    return Cast<ScopeInfo>(maybe_scope_info);
  }
  return EarlyGetReadOnlyRoots().empty_scope_info();
}

void SharedFunctionInfo::SetScopeInfo(Tagged<ScopeInfo> scope_info,
                                      WriteBarrierMode mode) {
  // Move the existing name onto the ScopeInfo.
  Tagged<NameOrScopeInfoT> name_or_scope_info =
      this->name_or_scope_info(kAcquireLoad);
  Tagged<UnionOf<Smi, String>> name;
  if (IsScopeInfo(name_or_scope_info)) {
    name = Cast<ScopeInfo>(name_or_scope_info)->FunctionName();
  } else {
    name = Cast<UnionOf<Smi, String>>(name_or_scope_info);
  }
  DCHECK(IsString(name) || name == kNoSharedNameSentinel);
  // ScopeInfo can get promoted to read-only space. Now that we reuse them after
  // flushing bytecode, we'll actually reinstall read-only scopeinfos on
  // SharedFunctionInfos if they required a context. The read-only scopeinfos
  // should already be fully initialized though, and hence will already have the
  // right FunctionName (and InferredName if relevant).
  if (scope_info->FunctionName() != name) {
    scope_info->SetFunctionName(name);
  }
  if (HasInferredName() && inferred_name()->length() != 0 &&
      scope_info->InferredFunctionName() != inferred_name()) {
    scope_info->SetInferredFunctionName(inferred_name());
  }
  set_name_or_scope_info(scope_info, kReleaseStore, mode);
}

void SharedFunctionInfo::set_raw_scope_info(Tagged<ScopeInfo> scope_info,
                                            WriteBarrierMode mode) {
  WRITE_FIELD(*this, kNameOrScopeInfoOffset, scope_info);
  CONDITIONAL_WRITE_BARRIER(*this, kNameOrScopeInfoOffset, scope_info, mode);
}

DEF_GETTER(SharedFunctionInfo, outer_scope_info,
           Tagged<UnionOf<ScopeInfo, TheHole>>) {
  DCHECK(!is_compiled());
  DCHECK(!HasFeedbackMetadata());
  return Cast<UnionOf<ScopeInfo, TheHole>>(
      raw_outer_scope_info_or_feedback_metadata(cage_base));
}

bool SharedFunctionInfo::HasOuterScopeInfo() const {
  Tagged<ScopeInfo> outer_info;
  Tagged<ScopeInfo> info = scope_info(kAcquireLoad);
  if (info->IsEmpty()) {
    if (is_compiled()) return false;
    Tagged<UnionOf<ScopeInfo, TheHole>> maybe_outer_info = outer_scope_info();
    if (IsTheHole(maybe_outer_info)) return false;
    outer_info = Cast<ScopeInfo>(maybe_outer_info);
  } else {
    if (!info->HasOuterScopeInfo()) return false;
    outer_info = info->OuterScopeInfo();
  }
  return !outer_info->IsEmpty();
}

Tagged<ScopeInfo> SharedFunctionInfo::GetOuterScopeInfo() const {
  DCHECK(HasOuterScopeInfo());
  Tagged<ScopeInfo> info = scope_info(kAcquireLoad);
  if (info->IsEmpty()) return Cast<ScopeInfo>(outer_scope_info());
  return info->OuterScopeInfo();
}

void SharedFunctionInfo::set_outer_scope_info(
    Tagged<UnionOf<ScopeInfo, TheHole>> value, WriteBarrierMode mode) {
  DCHECK(!is_compiled());
  DCHECK(IsTheHole(raw_outer_scope_info_or_feedback_metadata()));
  DCHECK(IsTheHole(value) || IsScopeInfo(value));
  DCHECK(scope_info()->IsEmpty());
  set_raw_outer_scope_info_or_feedback_metadata(value, mode);
}

bool SharedFunctionInfo::HasFeedbackMetadata() const {
  Tagged<UnionOf<ScopeInfo, FeedbackMetadata, TheHole>> raw =
      raw_outer_scope_info_or_feedback_metadata();
  return IsFeedbackMetadata(raw);
}

bool SharedFunctionInfo::HasFeedbackMetadata(AcquireLoadTag tag) const {
  Tagged<UnionOf<ScopeInfo, FeedbackMetadata, TheHole>> raw =
      raw_outer_scope_info_or_feedback_metadata(tag);
  return IsFeedbackMetadata(raw);
}

DEF_GETTER(SharedFunctionInfo, feedback_metadata, Tagged<FeedbackMetadata>) {
  DCHECK(HasFeedbackMetadata());
  return Cast<FeedbackMetadata>(
      raw_outer_scope_info_or_feedback_metadata(cage_base));
}

RELEASE_ACQUIRE_ACCESSORS_CHECKED2(SharedFunctionInfo, feedback_metadata,
                                   Tagged<FeedbackMetadata>,
                                   kOuterScopeInfoOrFeedbackMetadataOffset,
                                   HasFeedbackMetadata(kAcquireLoad),
                                   !HasFeedbackMetadata(kAcquireLoad) &&
                                       IsFeedbackMetadata(value))

bool SharedFunctionInfo::is_compiled() const {
  return GetUntrustedData() != Smi::FromEnum(Builtin::kCompileLazy) &&
         !HasUncompiledData(GetCurrentIsolateForSandbox());
}

template <typename IsolateT>
IsCompiledScope SharedFunctionInfo::is_compiled_scope(IsolateT* isolate) const {
  return IsCompiledScope(*this, isolate);
}

IsCompiledScope::IsCompiledScope(const Tagged<SharedFunctionInfo> shared,
                                 Isolate* isolate) {
  Tagged<Object> data_obj = shared->GetTrustedData(isolate);
  if (Tagged<Code> code; TryCast(data_obj, &code)) {
    DCHECK_EQ(code->kind(), CodeKind::BASELINE);
    data_obj = code->bytecode_or_interpreter_data();
  }
  // Unlike GetBytecodeArray, we don't bother checking for DebugInfo here. If
  // there is DebugInfo, then it will hold both the debug and original
  // BytecodeArray strongly, so it doesn't matter which of those we hold.
  if (Tagged<BytecodeArray> bytecode; TryCast(data_obj, &bytecode)) {
    retain_code_ = handle(bytecode, isolate);
    is_compiled_ = true;
  } else if (Tagged<InterpreterData> interpreter_data;
             TryCast(data_obj, &interpreter_data)) {
    retain_code_ = handle(interpreter_data->bytecode_array(), isolate);
    is_compiled_ = true;
  } else if (IsUncompiledData(data_obj)) {
    retain_code_ = {};
    is_compiled_ = false;
  } else {
    retain_code_ = {};
    is_compiled_ = shared->is_compiled();
  }

  DCHECK_IMPLIES(!retain_code_.is_null(), is_compiled());
  DCHECK_EQ(shared->is_compiled(), is_compiled());
}

IsCompiledScope::IsCompiledScope(const Tagged<SharedFunctionInfo> shared,
                                 LocalIsolate* isolate) {
  Tagged<Object> data_obj = shared->GetTrustedData(isolate);
  auto Default = [&]() {
    retain_code_ = {};
    is_compiled_ = shared->is_compiled();
  };

  if (Tagged<HeapObject> data; TryCast<HeapObject>(data_obj, &data)) {
    if (Tagged<Code> code; TryCast(data, &code)) {
      DCHECK(code->kind() == CodeKind::BASELINE);
      data_obj = code->bytecode_or_interpreter_data();
    }
    // Unlike GetBytecodeArray, we don't bother checking for DebugInfo here. If
    // there is DebugInfo, then it will hold both the debug and original
    // BytecodeArray strongly, so it doesn't matter which of those we hold.
    if (Tagged<BytecodeArray> bytecode; TryCast(data, &bytecode)) {
      retain_code_ = isolate->heap()->NewPersistentHandle(bytecode);
      is_compiled_ = true;
    } else if (Tagged<InterpreterData> interpreter_data;
               TryCast(data, &interpreter_data)) {
      retain_code_ = isolate->heap()->NewPersistentHandle(
          interpreter_data->bytecode_array());
      is_compiled_ = true;
    } else if (Is<UncompiledData>(data)) {
      retain_code_ = {};
      is_compiled_ = false;
    } else {
      Default();
    }
  } else {
    Default();
  }

  DCHECK_IMPLIES(!retain_code_.is_null(), is_compiled());
  DCHECK_EQ(shared->is_compiled(), is_compiled());
}

IsBaselineCompiledScope::IsBaselineCompiledScope(
    const Tagged<SharedFunctionInfo> shared, Isolate* isolate) {
  Tagged<Object> data_obj = shared->GetTrustedData(isolate);
  if (Tagged<Code> code; TryCast(data_obj, &code)) {
    DCHECK_EQ(code->kind(), CodeKind::BASELINE);
    retain_code_ = handle(code, isolate);
    is_compiled_ = true;
  }
}

bool SharedFunctionInfo::has_simple_parameters() const {
  return scope_info(kAcquireLoad)->HasSimpleParameters();
}

bool SharedFunctionInfo::CanCollectSourcePosition(Isolate* isolate) {
  // This function is called during heap iteration and so might see
  // dead-but-inconsistent SFIs, e.g. those referencing an unpublished trusted
  // object, so we need to check for that here.
  return v8_flags.enable_lazy_source_positions &&
         !HasUnpublishedTrustedData(isolate) && HasBytecodeArray() &&
         !GetBytecodeArray(isolate)->HasSourcePositionTable();
}

bool SharedFunctionInfo::IsApiFunction() const {
  return IsFunctionTemplateInfo(GetUntrustedData());
}

DEF_GETTER(SharedFunctionInfo, api_func_data, Tagged<FunctionTemplateInfo>) {
  DCHECK(IsApiFunction());
  return Cast<FunctionTemplateInfo>(GetUntrustedData());
}

DEF_GETTER(SharedFunctionInfo, HasBytecodeArray, bool) {
  Tagged<Object> data = GetTrustedData(GetCurrentIsolateForSandbox());
  // If the SFI has no trusted data, GetTrustedData() will return Smi::zero().
  if (IsSmi(data)) return false;
  InstanceType instance_type =
      Cast<HeapObject>(data)->map(cage_base)->instance_type();
  return InstanceTypeChecker::IsBytecodeArray(instance_type) ||
         InstanceTypeChecker::IsInterpreterData(instance_type) ||
         InstanceTypeChecker::IsCode(instance_type);
}

template <typename IsolateT>
Tagged<BytecodeArray> SharedFunctionInfo::GetBytecodeArray(
    IsolateT* isolate) const {
  MutexGuardIfOffThread<IsolateT> mutex_guard(
      isolate->shared_function_info_access(), isolate);
  Isolate* main_isolate = isolate->GetMainThreadIsolateUnsafe();
  return GetBytecodeArrayInternal(main_isolate);
}

Tagged<BytecodeArray> SharedFunctionInfo::GetBytecodeArrayForGC(
    Isolate* isolate) const {
  // Can only be used during GC when all threads are halted.
  DCHECK_EQ(Isolate::Current()->heap()->gc_state(), Heap::MARK_COMPACT);
  return GetBytecodeArrayInternal(isolate);
}

Tagged<BytecodeArray> SharedFunctionInfo::GetBytecodeArrayInternal(
    Isolate* isolate) const {
  DCHECK(HasBytecodeArray());

  std::optional<Tagged<DebugInfo>> debug_info = TryGetDebugInfo(isolate);
  if (debug_info.has_value() &&
      debug_info.value()->HasInstrumentedBytecodeArray()) {
    return debug_info.value()->OriginalBytecodeArray(isolate);
  }

  return GetActiveBytecodeArray(isolate);
}

Tagged<BytecodeArray> SharedFunctionInfo::GetActiveBytecodeArray(
    Isolate* isolate) const {
  Tagged<Object> data = GetTrustedData(isolate);
  if (Tagged<Code> baseline_code; TryCast(data, &baseline_code)) {
    data = baseline_code->bytecode_or_interpreter_data();
  }
  if (Tagged<BytecodeArray> bytecode_array; TryCast(data, &bytecode_array)) {
    return bytecode_array;
  }
  if (!Is<InterpreterData>(data)) {
    // See https://crbug.com/442277757
    InstanceType type = static_cast<InstanceType>(-1);
    if (Is<HeapObject>(data)) {
      type = Cast<HeapObject>(data)->map()->instance_type();
    }
    isolate->PushStackTraceAndDie(
        reinterpret_cast<void*>(data.ptr()),
        reinterpret_cast<void*>(GetTrustedData(isolate).ptr()),
        reinterpret_cast<void*>(type));
  }
  return SbxCast<InterpreterData>(data)->bytecode_array();
}

void SharedFunctionInfo::SetActiveBytecodeArray(Tagged<BytecodeArray> bytecode,
                                                IsolateForSandbox isolate) {
  // We don't allow setting the active bytecode array on baseline-optimized
  // functions. They should have been flushed earlier.
  DCHECK(!HasBaselineCode());

  if (HasInterpreterData(isolate)) {
    interpreter_data(isolate)->set_bytecode_array(bytecode);
  } else {
    DCHECK(HasBytecodeArray());
    overwrite_bytecode_array(bytecode);
  }
}

void SharedFunctionInfo::set_bytecode_array(Tagged<BytecodeArray> bytecode) {
  DCHECK(GetUntrustedData() == Smi::FromEnum(Builtin::kCompileLazy) ||
         HasUncompiledData(GetCurrentIsolateForSandbox()));
  SetTrustedData(bytecode);
}

void SharedFunctionInfo::overwrite_bytecode_array(
    Tagged<BytecodeArray> bytecode) {
  DCHECK(HasBytecodeArray());
  SetTrustedData(bytecode);
}

Tagged<Code> SharedFunctionInfo::InterpreterTrampoline(
    IsolateForSandbox isolate) const {
  DCHECK(HasInterpreterData(isolate));
  return interpreter_data(isolate)->interpreter_trampoline();
}

bool SharedFunctionInfo::HasInterpreterData(IsolateForSandbox isolate) const {
  Tagged<Object> data = GetTrustedData(isolate);
  if (Tagged<Code> baseline_code; TryCast(data, &baseline_code)) {
    DCHECK_EQ(baseline_code->kind(), CodeKind::BASELINE);
    data = baseline_code->bytecode_or_interpreter_data();
  }
  return IsInterpreterData(data);
}

Tagged<InterpreterData> SharedFunctionInfo::interpreter_data(
    IsolateForSandbox isolate) const {
  DCHECK(HasInterpreterData(isolate));
  Tagged<Object> data = GetTrustedData(isolate);
  if (Tagged<Code> baseline_code; TryCast(data, &baseline_code)) {
    DCHECK_EQ(baseline_code->kind(), CodeKind::BASELINE);
    data = baseline_code->bytecode_or_interpreter_data();
  }
  return SbxCast<InterpreterData>(data);
}

void SharedFunctionInfo::set_interpreter_data(
    Isolate* isolate, Tagged<InterpreterData> interpreter_data,
    WriteBarrierMode mode) {
  DCHECK(isolate->interpreted_frames_native_stack());
  DCHECK(!HasBaselineCode());
  SetTrustedData(interpreter_data, mode);
}

DEF_GETTER(SharedFunctionInfo, HasBaselineCode, bool) {
  Tagged<Object> data = GetTrustedData(GetCurrentIsolateForSandbox());
  if (Tagged<Code> code; TryCast(data, &code)) {
    DCHECK_EQ(code->kind(), CodeKind::BASELINE);
    return true;
  }
  return false;
}

DEF_ACQUIRE_GETTER(SharedFunctionInfo, baseline_code, Tagged<Code>) {
  DCHECK(HasBaselineCode(cage_base));
  IsolateForSandbox isolate = GetCurrentIsolateForSandbox();
  return GetTrustedData<Code, kCodeIndirectPointerTag>(isolate);
}

void SharedFunctionInfo::set_baseline_code(Tagged<Code> baseline_code,
                                           ReleaseStoreTag tag,
                                           WriteBarrierMode mode) {
  DCHECK_EQ(baseline_code->kind(), CodeKind::BASELINE);
  SetTrustedData(baseline_code, mode);
}

void SharedFunctionInfo::FlushBaselineCode() {
  DCHECK(HasBaselineCode());
  Tagged<TrustedObject> new_data =
      baseline_code(kAcquireLoad)->bytecode_or_interpreter_data();
  DCHECK(IsBytecodeArray(new_data) || IsInterpreterData(new_data));
  SetTrustedData(TrustedCast<ExposedTrustedObject>(new_data));
}

#if V8_ENABLE_WEBASSEMBLY
bool SharedFunctionInfo::HasAsmWasmData() const {
  return IsAsmWasmData(GetUntrustedData());
}

bool SharedFunctionInfo::HasWasmFunctionData(IsolateForSandbox isolate) const {
  return IsWasmFunctionData(GetTrustedData(isolate));
}

bool SharedFunctionInfo::HasWasmExportedFunctionData(
    IsolateForSandbox isolate) const {
  return IsWasmExportedFunctionData(GetTrustedData(isolate));
}

bool SharedFunctionInfo::HasWasmJSFunctionData(
    IsolateForSandbox isolate) const {
  return IsWasmJSFunctionData(GetTrustedData(isolate));
}

bool SharedFunctionInfo::HasWasmCapiFunctionData(
    IsolateForSandbox isolate) const {
  return IsWasmCapiFunctionData(GetTrustedData(isolate));
}

bool SharedFunctionInfo::HasWasmResumeData() const {
  return IsWasmResumeData(GetUntrustedData());
}

DEF_GETTER(SharedFunctionInfo, asm_wasm_data, Tagged<AsmWasmData>) {
  DCHECK(HasAsmWasmData());
  return Cast<AsmWasmData>(GetUntrustedData());
}

void SharedFunctionInfo::set_asm_wasm_data(Tagged<AsmWasmData> data,
                                           WriteBarrierMode mode) {
  DCHECK(GetUntrustedData() == Smi::FromEnum(Builtin::kCompileLazy) ||
         HasUncompiledData(GetCurrentIsolateForSandbox()) || HasAsmWasmData());
  SetUntrustedData(data, mode);
}

DEF_GETTER(SharedFunctionInfo, wasm_function_data, Tagged<WasmFunctionData>) {
  // TODO(saelo): It would be nicer if the caller provided an
  // IsolateForSandbox.
  IsolateForSandbox isolate = GetCurrentIsolateForSandbox();
  DCHECK(HasWasmFunctionData(isolate));
  return GetTrustedData<WasmFunctionData, kWasmFunctionDataIndirectPointerTag>(
      isolate);
}

DEF_GETTER(SharedFunctionInfo, wasm_exported_function_data,
           Tagged<WasmExportedFunctionData>) {
  DCHECK(HasWasmExportedFunctionData(GetCurrentIsolateForSandbox()));
  Tagged<WasmFunctionData> data = wasm_function_data();
  // TODO(saelo): the SBXCHECKs here and below are only needed because our type
  // tags don't currently support type hierarchies.
  return SbxCast<WasmExportedFunctionData>(data);
}

DEF_GETTER(SharedFunctionInfo, wasm_js_function_data,
           Tagged<WasmJSFunctionData>) {
  DCHECK(HasWasmJSFunctionData(GetCurrentIsolateForSandbox()));
  Tagged<WasmFunctionData> data = wasm_function_data();
  return SbxCast<WasmJSFunctionData>(data);
}

DEF_GETTER(SharedFunctionInfo, wasm_capi_function_data,
           Tagged<WasmCapiFunctionData>) {
  DCHECK(HasWasmCapiFunctionData(GetCurrentIsolateForSandbox()));
  Tagged<WasmFunctionData> data = wasm_function_data();
  return SbxCast<WasmCapiFunctionData>(data);
}

DEF_GETTER(SharedFunctionInfo, wasm_resume_data, Tagged<WasmResumeData>) {
  DCHECK(HasWasmResumeData());
  return Cast<WasmResumeData>(GetUntrustedData());
}

#endif  // V8_ENABLE_WEBASSEMBLY

bool SharedFunctionInfo::HasBuiltinId() const {
  Tagged<Object> data = GetUntrustedData();
  return IsSmi(data) && Builtins::IsBuiltinId(Smi::ToInt(data));
}

Builtin SharedFunctionInfo::builtin_id() const {
  DCHECK(HasBuiltinId());
  int id = Smi::ToInt(GetUntrustedData());
  // The builtin id is read from the heap and so must be assumed to be
  // untrusted in the sandbox attacker model. As it is considered trusted by
  // e.g. `GetCode` (when fetching the code for this SFI), we validate it here.
  SBXCHECK(Builtins::IsBuiltinId(id));
  return Builtins::FromInt(id);
}

void SharedFunctionInfo::set_builtin_id(Builtin builtin) {
  DCHECK(Builtins::IsBuiltinId(builtin));
  SetUntrustedData(Smi::FromInt(static_cast<int>(builtin)), SKIP_WRITE_BARRIER);
}

bool SharedFunctionInfo::HasUncompiledData(IsolateForSandbox isolate) const {
  return !HasUnpublishedTrustedData(isolate) &&
         IsUncompiledData(GetTrustedData(isolate));
}

Tagged<UncompiledData> SharedFunctionInfo::uncompiled_data(
    IsolateForSandbox isolate) const {
  DCHECK(HasUncompiledData(isolate));
  return GetTrustedData<UncompiledData, kUncompiledDataIndirectPointerTag>(
      isolate);
}

void SharedFunctionInfo::set_uncompiled_data(
    Tagged<UncompiledData> uncompiled_data, WriteBarrierMode mode) {
  DCHECK(IsUncompiledData(uncompiled_data));
  SetTrustedData(uncompiled_data, mode);
}

bool SharedFunctionInfo::HasUncompiledDataWithPreparseData(
    IsolateForSandbox isolate) const {
  return IsUncompiledDataWithPreparseData(GetTrustedData(isolate));
}

Tagged<UncompiledDataWithPreparseData>
SharedFunctionInfo::uncompiled_data_with_preparse_data(
    IsolateForSandbox isolate) const {
  DCHECK(HasUncompiledDataWithPreparseData(isolate));
  Tagged<UncompiledData> data = uncompiled_data(isolate);
  // TODO(saelo): this SBXCHECK is needed because our type tags don't currently
  // support type hierarchies.
  return SbxCast<UncompiledDataWithPreparseData>(data);
}

void SharedFunctionInfo::set_uncompiled_data_with_preparse_data(
    Tagged<UncompiledDataWithPreparseData> uncompiled_data_with_preparse_data,
    WriteBarrierMode mode) {
  DCHECK_EQ(GetUntrustedData(), Smi::FromEnum(Builtin::kCompileLazy));
  DCHECK(IsUncompiledDataWithPreparseData(uncompiled_data_with_preparse_data));
  SetTrustedData(uncompiled_data_with_preparse_data, mode);
}

bool SharedFunctionInfo::HasUncompiledDataWithoutPreparseData(
    IsolateForSandbox isolate) const {
  return IsUncompiledDataWithoutPreparseData(GetTrustedData(isolate));
}

void SharedFunctionInfo::ClearUncompiledDataJobPointer(
    IsolateForSandbox isolate) {
  Tagged<UncompiledData> uncompiled_data = this->uncompiled_data(isolate);
  if (Tagged<UncompiledDataWithPreparseDataAndJob> data;
      TryCast(uncompiled_data, &data)) {
    data->set_job(kNullAddress);
  } else if (Tagged<UncompiledDataWithoutPreparseDataWithJob> data_with_job;
             TryCast(uncompiled_data, &data_with_job)) {
    data_with_job->set_job(kNullAddress);
  }
}

void SharedFunctionInfo::ClearPreparseData(IsolateForSandbox isolate) {
  DCHECK(HasUncompiledDataWithPreparseData(isolate));
  Tagged<UncompiledDataWithPreparseData> data =
      uncompiled_data_with_preparse_data(isolate);

  // Trim off the pre-parsed scope data from the uncompiled data by swapping the
  // map, leaving only an uncompiled data without pre-parsed scope.
  DisallowGarbageCollection no_gc;
  Heap* heap = Isolate::Current()->heap();

  // We are basically trimming that object to its supertype, so recorded slots
  // within the object don't need to be invalidated.
  heap->NotifyObjectLayoutChange(data, no_gc, InvalidateRecordedSlots::kNo,
                                 InvalidateExternalPointerSlots::kNo);
  static_assert(sizeof(UncompiledDataWithoutPreparseData) <
                sizeof(UncompiledDataWithPreparseData));
  static_assert(sizeof(UncompiledDataWithoutPreparseData) ==
                sizeof(UncompiledData));

  // Fill the remaining space with filler and clear slots in the trimmed area.
  int old_size = data->Size();
  DCHECK_LE(sizeof(UncompiledDataWithPreparseData), old_size);
  heap->NotifyObjectSizeChange(data, old_size,
                               sizeof(UncompiledDataWithoutPreparseData),
                               ClearRecordedSlots::kYes);

  // Swap the map.
  data->set_map(heap->isolate(),
                GetReadOnlyRoots().uncompiled_data_without_preparse_data_map(),
                kReleaseStore);

  // Ensure that the clear was successful.
  DCHECK(HasUncompiledDataWithoutPreparseData(isolate));
}

void UncompiledData::InitAfterBytecodeFlush(
    Isolate* isolate, Tagged<String> inferred_name, int start_position,
    int end_position,
    std::function<void(Tagged<HeapObject> object, ObjectSlot slot,
                       Tagged<HeapObject> target)>
        gc_notify_updated_slot) {
#ifdef V8_ENABLE_SANDBOX
  InitAndPublish(isolate);
#endif
  set_inferred_name(inferred_name);
  gc_notify_updated_slot(this, ObjectSlot(&inferred_name_), inferred_name);
  set_start_position(start_position);
  set_end_position(end_position);
}

bool SharedFunctionInfo::is_repl_mode() const {
  return IsScript(script()) && Cast<Script>(script())->is_repl_mode();
}

bool SharedFunctionInfo::HasInferredName() {
  Tagged<Object> scope_info = name_or_scope_info(kAcquireLoad);
  if (IsScopeInfo(scope_info)) {
    return Cast<ScopeInfo>(scope_info)->HasInferredFunctionName();
  }
  return HasUncompiledData(GetCurrentIsolateForSandbox());
}

DEF_GETTER(SharedFunctionInfo, inferred_name, Tagged<String>) {
  Tagged<Object> maybe_scope_info = name_or_scope_info(kAcquireLoad);
  if (IsScopeInfo(maybe_scope_info)) {
    Tagged<ScopeInfo> scope_info = Cast<ScopeInfo>(maybe_scope_info);
    if (scope_info->HasInferredFunctionName()) {
      Tagged<Object> name = scope_info->InferredFunctionName();
      if (IsString(name)) return Cast<String>(name);
    }
  } else {
    IsolateForSandbox isolate = GetCurrentIsolateForSandbox();
    if (HasUncompiledData(isolate)) {
      return uncompiled_data(isolate)->inferred_name();
    }
  }
  return GetReadOnlyRoots().empty_string();
}

bool SharedFunctionInfo::IsUserJavaScript() const {
  Tagged<Object> script_obj = script();
  if (IsUndefined(script_obj)) return false;
  Tagged<Script> script = Cast<Script>(script_obj);
  return script->IsUserJavaScript();
}

bool SharedFunctionInfo::IsSubjectToDebugging() const {
#if V8_ENABLE_WEBASSEMBLY
  if (HasAsmWasmData()) return false;
  if (HasWasmExportedFunctionData(GetCurrentIsolateForSandbox())) return false;
#endif  // V8_ENABLE_WEBASSEMBLY
  return IsUserJavaScript();
}

bool SharedFunctionInfo::CanDiscardCompiled() const {
#if V8_ENABLE_WEBASSEMBLY
  if (HasAsmWasmData()) return true;
#endif  // V8_ENABLE_WEBASSEMBLY
  return HasBytecodeArray() ||
         HasUncompiledDataWithPreparseData(GetCurrentIsolateForSandbox()) ||
         HasBaselineCode();
}

bool SharedFunctionInfo::is_class_constructor() const {
  return IsClassConstructorBit::decode(flags(kRelaxedLoad));
}

void SharedFunctionInfo::set_are_properties_final(bool value) {
  if (is_class_constructor()) {
    set_properties_are_final(value);
  }
}

bool SharedFunctionInfo::are_properties_final() const {
  bool bit = properties_are_final();
  return bit && is_class_constructor();
}

OBJECT_CONSTRUCTORS_IMPL(SharedFunctionInfoWrapper, TrustedObject)

ACCESSORS(SharedFunctionInfoWrapper, shared_info, Tagged<SharedFunctionInfo>,
          kSharedInfoOffset)

}  // namespace v8::internal

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_SHARED_FUNCTION_INFO_INL_H_
