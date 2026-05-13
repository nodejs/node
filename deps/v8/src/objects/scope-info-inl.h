// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_SCOPE_INFO_INL_H_
#define V8_OBJECTS_SCOPE_INFO_INL_H_

#include "src/objects/scope-info.h"
// Include the non-inl header before the rest of the headers.

#include "src/heap/heap-write-barrier-inl.h"
#include "src/objects/dependent-code.h"
#include "src/objects/fixed-array-inl.h"
#include "src/objects/name-inl.h"
#include "src/objects/string.h"
#include "src/roots/roots-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

bool ScopeInfo::IsAsmModule() const { return IsAsmModuleBit::decode(Flags()); }

bool ScopeInfo::HasSimpleParameters() const {
  return HasSimpleParametersBit::decode(Flags());
}

bool ScopeInfo::HasContextCells() const {
  return HasContextCellsBit::decode(Flags());
}

bool ScopeInfo::is_hoisted_in_context() const {
  return IsHoistedInContextBit::decode(Flags());
}

uint32_t ScopeInfo::Flags() const { return flags(kRelaxedLoad); }
int ScopeInfo::ParameterCount() const { return parameter_count(); }
int ScopeInfo::ContextLocalCount() const { return context_local_count(); }

// -----------------------------------------------------------------------
// Named field accessors.

uint32_t ScopeInfo::flags(RelaxedLoadTag) const {
  return flags_.load(std::memory_order_relaxed);
}

void ScopeInfo::set_flags(uint32_t value, RelaxedStoreTag) {
  flags_.store(value, std::memory_order_relaxed);
}

int ScopeInfo::parameter_count() const {
  return parameter_count_.load().value();
}

void ScopeInfo::set_parameter_count(int value) {
  parameter_count_.store(this, Smi::FromInt(value));
}

int ScopeInfo::context_local_count() const {
  return context_local_count_.load().value();
}

void ScopeInfo::set_context_local_count(int value) {
  context_local_count_.store(this, Smi::FromInt(value));
}

int ScopeInfo::position_info_start() const {
  return position_info_start_.load().value();
}

void ScopeInfo::set_position_info_start(int value) {
  position_info_start_.store(this, Smi::FromInt(value));
}

int ScopeInfo::position_info_end() const {
  return position_info_end_.load().value();
}

void ScopeInfo::set_position_info_end(int value) {
  position_info_end_.store(this, Smi::FromInt(value));
}

// -----------------------------------------------------------------------
// Variable-length conditional field offsets. Each offset chains off of
// the preceding one based on flags (scope type, has-* bits) and counts
// (context_local_count, module_variable_count). Each returned offset is
// the byte offset from the start of the object.

int ScopeInfo::ModuleVariableCountOffset() const {
  // ModuleVariableCount is the first slot of the variable tail. It's
  // always at OFFSET_OF_DATA_START(ScopeInfo) regardless of whether the
  // field is actually present (length 0 when not a module scope).
  return OFFSET_OF_DATA_START(ScopeInfo);
}

int ScopeInfo::ContextLocalNamesOffset() const {
  const bool is_module =
      ScopeTypeBits::decode(Flags()) == ScopeType::MODULE_SCOPE;
  return ModuleVariableCountOffset() + (is_module ? kTaggedSize : 0);
}

int ScopeInfo::ContextLocalNamesHashtableOffset() const {
  const int local_count = context_local_count();
  const int inlined_count =
      local_count < kScopeInfoMaxInlinedLocalNamesSize ? local_count : 0;
  return ContextLocalNamesOffset() + inlined_count * kTaggedSize;
}

int ScopeInfo::ContextLocalInfosOffset() const {
  const int local_count = context_local_count();
  const bool has_hashtable = local_count >= kScopeInfoMaxInlinedLocalNamesSize;
  return ContextLocalNamesHashtableOffset() + (has_hashtable ? kTaggedSize : 0);
}

int ScopeInfo::SavedClassVariableInfoOffset() const {
  return ContextLocalInfosOffset() + context_local_count() * kTaggedSize;
}

int ScopeInfo::FunctionVariableInfoOffset() const {
  const bool has_saved_class_variable =
      HasSavedClassVariableBit::decode(Flags());
  return SavedClassVariableInfoOffset() +
         (has_saved_class_variable ? kTaggedSize : 0);
}

int ScopeInfo::InferredFunctionNameOffset() const {
  const bool has_function_variable =
      FunctionVariableBits::decode(Flags()) != VariableAllocationInfo::NONE;
  return FunctionVariableInfoOffset() +
         (has_function_variable
              ? TorqueGeneratedFunctionVariableInfoOffsets::kSize
              : 0);
}

int ScopeInfo::OuterScopeInfoOffset() const {
  const bool has_inferred_name = HasInferredFunctionNameBit::decode(Flags());
  return InferredFunctionNameOffset() + (has_inferred_name ? kTaggedSize : 0);
}

int ScopeInfo::ModuleInfoOffset() const {
  const bool has_outer = HasOuterScopeInfoBit::decode(Flags());
  return OuterScopeInfoOffset() + (has_outer ? kTaggedSize : 0);
}

int ScopeInfo::ModuleVariablesOffset() const {
  const bool is_module =
      ScopeTypeBits::decode(Flags()) == ScopeType::MODULE_SCOPE;
  return ModuleInfoOffset() + (is_module ? kTaggedSize : 0);
}

int ScopeInfo::DependentCodeOffset() const {
  const bool is_module =
      ScopeTypeBits::decode(Flags()) == ScopeType::MODULE_SCOPE;
  const int mod_var_count = is_module ? module_variable_count() : 0;
  return ModuleVariablesOffset() +
         mod_var_count * TorqueGeneratedModuleVariableOffsets::kSize;
}

int ScopeInfo::UnusedParameterBitsOffset() const {
  const bool has_dependent_code = SloppyEvalCanExtendVarsBit::decode(Flags());
  return DependentCodeOffset() + (has_dependent_code ? kTaggedSize : 0);
}

int ScopeInfo::AllocatedSize() const {
  const bool is_function =
      ScopeTypeBits::decode(Flags()) == ScopeType::FUNCTION_SCOPE;
  return UnusedParameterBitsOffset() + (is_function ? kTaggedSize : 0);
}

// -----------------------------------------------------------------------
// Raw slot accessors for variable-length fields. Given a byte offset
// in the variable tail, compute the corresponding index into `data()`.

namespace scope_info_internal {
constexpr int DataSlotIndex(int byte_offset) {
  DCHECK_EQ(0, (byte_offset - OFFSET_OF_DATA_START(ScopeInfo)) % kTaggedSize);
  return (byte_offset - OFFSET_OF_DATA_START(ScopeInfo)) / kTaggedSize;
}
}  // namespace scope_info_internal

int ScopeInfo::module_variable_count() const {
  // Only valid when scope_type == MODULE_SCOPE.
  DCHECK_EQ(ScopeTypeBits::decode(Flags()), ScopeType::MODULE_SCOPE);
  return Cast<Smi>(data()[0].load()).value();
}

void ScopeInfo::set_module_variable_count(int value) {
  DCHECK_EQ(ScopeTypeBits::decode(Flags()), ScopeType::MODULE_SCOPE);
  data()[0].store(this, Smi::FromInt(value));
}

Tagged<String> ScopeInfo::context_local_names(int i) const {
  DCHECK_GE(i, 0);
  DCHECK_LT(i, context_local_count());
  DCHECK_LT(context_local_count(), kScopeInfoMaxInlinedLocalNamesSize);
  const int slot = scope_info_internal::DataSlotIndex(
      ContextLocalNamesOffset() + i * kTaggedSize);
  return Cast<String>(data()[slot].load());
}

void ScopeInfo::set_context_local_names(int i, Tagged<String> value,
                                        WriteBarrierMode mode) {
  DCHECK_GE(i, 0);
  DCHECK_LT(i, context_local_count());
  DCHECK_LT(context_local_count(), kScopeInfoMaxInlinedLocalNamesSize);
  const int slot = scope_info_internal::DataSlotIndex(
      ContextLocalNamesOffset() + i * kTaggedSize);
  data()[slot].store(this, value, mode);
}

Tagged<NameToIndexHashTable> ScopeInfo::context_local_names_hashtable() const {
  DCHECK_GE(context_local_count(), kScopeInfoMaxInlinedLocalNamesSize);
  const int slot =
      scope_info_internal::DataSlotIndex(ContextLocalNamesHashtableOffset());
  return Cast<NameToIndexHashTable>(data()[slot].load());
}

void ScopeInfo::set_context_local_names_hashtable(
    Tagged<NameToIndexHashTable> value, WriteBarrierMode mode) {
  DCHECK_GE(context_local_count(), kScopeInfoMaxInlinedLocalNamesSize);
  const int slot =
      scope_info_internal::DataSlotIndex(ContextLocalNamesHashtableOffset());
  data()[slot].store(this, value, mode);
}

int ScopeInfo::context_local_infos(int i) const {
  DCHECK_GE(i, 0);
  DCHECK_LT(i, context_local_count());
  const int slot = scope_info_internal::DataSlotIndex(
      ContextLocalInfosOffset() + i * kTaggedSize);
  return Cast<Smi>(data()[slot].load()).value();
}

void ScopeInfo::set_context_local_infos(int i, int value) {
  DCHECK_GE(i, 0);
  DCHECK_LT(i, context_local_count());
  const int slot = scope_info_internal::DataSlotIndex(
      ContextLocalInfosOffset() + i * kTaggedSize);
  data()[slot].store(this, Smi::From31BitPattern(value));
}

Tagged<Union<Name, Smi>> ScopeInfo::saved_class_variable_info() const {
  DCHECK(HasSavedClassVariableBit::decode(Flags()));
  const int slot =
      scope_info_internal::DataSlotIndex(SavedClassVariableInfoOffset());
  return Cast<Union<Name, Smi>>(data()[slot].load());
}

void ScopeInfo::set_saved_class_variable_info(Tagged<Union<Name, Smi>> value,
                                              WriteBarrierMode mode) {
  DCHECK(HasSavedClassVariableBit::decode(Flags()));
  const int slot =
      scope_info_internal::DataSlotIndex(SavedClassVariableInfoOffset());
  data()[slot].store(this, value, mode);
}

Tagged<Union<Smi, String>> ScopeInfo::function_variable_info_name() const {
  DCHECK_NE(FunctionVariableBits::decode(Flags()),
            VariableAllocationInfo::NONE);
  const int slot = scope_info_internal::DataSlotIndex(
      FunctionVariableInfoOffset() +
      TorqueGeneratedFunctionVariableInfoOffsets::kNameOffset);
  return Cast<Union<Smi, String>>(data()[slot].load());
}

void ScopeInfo::set_function_variable_info_name(
    Tagged<Union<Smi, String>> value, WriteBarrierMode mode) {
  DCHECK_NE(FunctionVariableBits::decode(Flags()),
            VariableAllocationInfo::NONE);
  const int slot = scope_info_internal::DataSlotIndex(
      FunctionVariableInfoOffset() +
      TorqueGeneratedFunctionVariableInfoOffsets::kNameOffset);
  data()[slot].store(this, value, mode);
}

int ScopeInfo::function_variable_info_context_or_stack_slot_index() const {
  DCHECK_NE(FunctionVariableBits::decode(Flags()),
            VariableAllocationInfo::NONE);
  const int slot = scope_info_internal::DataSlotIndex(
      FunctionVariableInfoOffset() +
      TorqueGeneratedFunctionVariableInfoOffsets::
          kContextOrStackSlotIndexOffset);
  return Cast<Smi>(data()[slot].load()).value();
}

void ScopeInfo::set_function_variable_info_context_or_stack_slot_index(
    int value) {
  DCHECK_NE(FunctionVariableBits::decode(Flags()),
            VariableAllocationInfo::NONE);
  const int slot = scope_info_internal::DataSlotIndex(
      FunctionVariableInfoOffset() +
      TorqueGeneratedFunctionVariableInfoOffsets::
          kContextOrStackSlotIndexOffset);
  data()[slot].store(this, Smi::FromInt(value));
}

Tagged<Union<String, Undefined>> ScopeInfo::inferred_function_name() const {
  DCHECK(HasInferredFunctionNameBit::decode(Flags()));
  const int slot =
      scope_info_internal::DataSlotIndex(InferredFunctionNameOffset());
  return Cast<Union<String, Undefined>>(data()[slot].load());
}

void ScopeInfo::set_inferred_function_name(
    Tagged<Union<String, Undefined>> value, WriteBarrierMode mode) {
  DCHECK(HasInferredFunctionNameBit::decode(Flags()));
  const int slot =
      scope_info_internal::DataSlotIndex(InferredFunctionNameOffset());
  data()[slot].store(this, value, mode);
}

Tagged<ScopeInfo> ScopeInfo::outer_scope_info() const {
  DCHECK(HasOuterScopeInfoBit::decode(Flags()));
  const int slot = scope_info_internal::DataSlotIndex(OuterScopeInfoOffset());
  return Cast<ScopeInfo>(data()[slot].load());
}

void ScopeInfo::set_outer_scope_info(Tagged<ScopeInfo> value,
                                     WriteBarrierMode mode) {
  DCHECK(HasOuterScopeInfoBit::decode(Flags()));
  const int slot = scope_info_internal::DataSlotIndex(OuterScopeInfoOffset());
  data()[slot].store(this, value, mode);
}

Tagged<FixedArray> ScopeInfo::module_info() const {
  DCHECK_EQ(ScopeTypeBits::decode(Flags()), ScopeType::MODULE_SCOPE);
  const int slot = scope_info_internal::DataSlotIndex(ModuleInfoOffset());
  return Cast<FixedArray>(data()[slot].load());
}

void ScopeInfo::set_module_info(Tagged<FixedArray> value,
                                WriteBarrierMode mode) {
  DCHECK_EQ(ScopeTypeBits::decode(Flags()), ScopeType::MODULE_SCOPE);
  const int slot = scope_info_internal::DataSlotIndex(ModuleInfoOffset());
  data()[slot].store(this, value, mode);
}

Tagged<String> ScopeInfo::module_variables_name(int i) const {
  DCHECK_GE(i, 0);
  DCHECK_LT(i, module_variable_count());
  const int slot = scope_info_internal::DataSlotIndex(
      ModuleVariablesOffset() +
      i * TorqueGeneratedModuleVariableOffsets::kSize +
      TorqueGeneratedModuleVariableOffsets::kNameOffset);
  return Cast<String>(data()[slot].load());
}

void ScopeInfo::set_module_variables_name(int i, Tagged<String> value,
                                          WriteBarrierMode mode) {
  DCHECK_GE(i, 0);
  DCHECK_LT(i, module_variable_count());
  const int slot = scope_info_internal::DataSlotIndex(
      ModuleVariablesOffset() +
      i * TorqueGeneratedModuleVariableOffsets::kSize +
      TorqueGeneratedModuleVariableOffsets::kNameOffset);
  data()[slot].store(this, value, mode);
}

int ScopeInfo::module_variables_index(int i) const {
  DCHECK_GE(i, 0);
  DCHECK_LT(i, module_variable_count());
  const int slot = scope_info_internal::DataSlotIndex(
      ModuleVariablesOffset() +
      i * TorqueGeneratedModuleVariableOffsets::kSize +
      TorqueGeneratedModuleVariableOffsets::kIndexOffset);
  return Cast<Smi>(data()[slot].load()).value();
}

void ScopeInfo::set_module_variables_index(int i, int value) {
  DCHECK_GE(i, 0);
  DCHECK_LT(i, module_variable_count());
  const int slot = scope_info_internal::DataSlotIndex(
      ModuleVariablesOffset() +
      i * TorqueGeneratedModuleVariableOffsets::kSize +
      TorqueGeneratedModuleVariableOffsets::kIndexOffset);
  data()[slot].store(this, Smi::FromInt(value));
}

int ScopeInfo::module_variables_properties(int i) const {
  DCHECK_GE(i, 0);
  DCHECK_LT(i, module_variable_count());
  const int slot = scope_info_internal::DataSlotIndex(
      ModuleVariablesOffset() +
      i * TorqueGeneratedModuleVariableOffsets::kSize +
      TorqueGeneratedModuleVariableOffsets::kPropertiesOffset);
  return Cast<Smi>(data()[slot].load()).value();
}

void ScopeInfo::set_module_variables_properties(int i, int value) {
  DCHECK_GE(i, 0);
  DCHECK_LT(i, module_variable_count());
  const int slot = scope_info_internal::DataSlotIndex(
      ModuleVariablesOffset() +
      i * TorqueGeneratedModuleVariableOffsets::kSize +
      TorqueGeneratedModuleVariableOffsets::kPropertiesOffset);
  data()[slot].store(this, Smi::From31BitPattern(value));
}

Tagged<DependentCode> ScopeInfo::dependent_code() const {
  DCHECK(SloppyEvalCanExtendVarsBit::decode(Flags()));
  const int slot = scope_info_internal::DataSlotIndex(DependentCodeOffset());
  return Cast<DependentCode>(data()[slot].load());
}

void ScopeInfo::set_dependent_code(Tagged<DependentCode> value,
                                   WriteBarrierMode mode) {
  DCHECK(SloppyEvalCanExtendVarsBit::decode(Flags()));
  const int slot = scope_info_internal::DataSlotIndex(DependentCodeOffset());
  data()[slot].store(this, value, mode);
}

int ScopeInfo::unused_parameter_bits() const {
  DCHECK_EQ(ScopeTypeBits::decode(Flags()), ScopeType::FUNCTION_SCOPE);
  const int slot =
      scope_info_internal::DataSlotIndex(UnusedParameterBitsOffset());
  return Cast<Smi>(data()[slot].load()).value();
}

void ScopeInfo::set_unused_parameter_bits(int value) {
  DCHECK_EQ(ScopeTypeBits::decode(Flags()), ScopeType::FUNCTION_SCOPE);
  const int slot =
      scope_info_internal::DataSlotIndex(UnusedParameterBitsOffset());
  data()[slot].store(this, Smi::From31BitPattern(value));
}

void ScopeInfo::InitializeTaggedMembers(Tagged<Object> value, int tail_length) {
  // Covers the fixed TaggedMember<Smi> header slots
  // (parameter_count_..position_info_end_) plus the |tail_length| tail
  // slots. Matches the GC-visible range of ScopeInfo::BodyDescriptor.
  MemsetTagged(ObjectSlot(reinterpret_cast<Address>(this) +
                          offsetof(ScopeInfo, parameter_count_)),
               value, kFixedTaggedHeaderSlotCount + tail_length);
}

bool ScopeInfo::HasInlinedLocalNames() const {
  return ContextLocalCount() < kScopeInfoMaxInlinedLocalNamesSize;
}

template <typename ScopeInfoPtr>
class ScopeInfo::LocalNamesRange {
 public:
  class Iterator {
   public:
    Iterator(const LocalNamesRange* range, InternalIndex index)
        : range_(range), index_(index) {
      DCHECK_NOT_NULL(range);
      if (!range_->inlined()) advance_hashtable_index();
    }

    Iterator& operator++() {
      DCHECK_LT(index_, range_->max_index());
      ++index_;
      if (range_->inlined()) return *this;
      advance_hashtable_index();
      return *this;
    }

    friend bool operator==(const Iterator& a, const Iterator& b) {
      return a.range_ == b.range_ && a.index_ == b.index_;
    }

    friend bool operator!=(const Iterator& a, const Iterator& b) {
      return !(a == b);
    }

    Tagged<String> name() const {
      DCHECK_LT(index_, range_->max_index());
      if (range_->inlined()) {
        return scope_info()->ContextInlinedLocalName(index_.as_int());
      }
      return Cast<String>(table()->KeyAt(index_));
    }

    const Iterator* operator*() const { return this; }

    int index() const {
      if (range_->inlined()) return index_.as_int();
      return table()->IndexAt(index_);
    }

   private:
    const LocalNamesRange* range_;
    InternalIndex index_;

    ScopeInfoPtr scope_info() const { return range_->scope_info_; }

    Tagged<NameToIndexHashTable> table() const {
      return scope_info()->context_local_names_hashtable();
    }

    void advance_hashtable_index() {
      DisallowGarbageCollection no_gc;
      ReadOnlyRoots roots = GetReadOnlyRoots();
      InternalIndex max = range_->max_index();
      // Increment until iterator points to a valid key or max.
      while (index_ < max) {
        Tagged<Object> key = table()->KeyAt(index_);
        if (table()->IsKey(roots, key)) break;
        ++index_;
      }
    }

    friend class LocalNamesRange;
  };

  bool inlined() const { return scope_info_->HasInlinedLocalNames(); }

  InternalIndex max_index() const {
    int max = inlined()
                  ? scope_info_->ContextLocalCount()
                  : scope_info_->context_local_names_hashtable()->Capacity();
    return InternalIndex(max);
  }

  explicit LocalNamesRange(ScopeInfoPtr scope_info) : scope_info_(scope_info) {}

  inline Iterator begin() const { return Iterator(this, InternalIndex(0)); }

  inline Iterator end() const { return Iterator(this, max_index()); }

 private:
  ScopeInfoPtr scope_info_;
};

// static
ScopeInfo::LocalNamesRange<DirectHandle<ScopeInfo>>
ScopeInfo::IterateLocalNames(DirectHandle<ScopeInfo> scope_info) {
  return LocalNamesRange<DirectHandle<ScopeInfo>>(scope_info);
}

// static
ScopeInfo::LocalNamesRange<Tagged<ScopeInfo>> ScopeInfo::IterateLocalNames(
    Tagged<ScopeInfo> scope_info, const DisallowGarbageCollection& no_gc) {
  USE(no_gc);
  return LocalNamesRange<Tagged<ScopeInfo>>(scope_info);
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_SCOPE_INFO_INL_H_
