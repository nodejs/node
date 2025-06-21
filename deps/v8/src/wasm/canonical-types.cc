// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/canonical-types.h"

#include "src/base/hashing.h"
#include "src/execution/isolate.h"
#include "src/handles/handles-inl.h"
#include "src/heap/heap-inl.h"
#include "src/init/v8.h"
#include "src/roots/roots-inl.h"
#include "src/utils/utils.h"
#include "src/wasm/names-provider.h"
#include "src/wasm/std-object-sizes.h"
#include "src/wasm/wasm-engine.h"

namespace v8::internal::wasm {

TypeCanonicalizer* GetTypeCanonicalizer() {
  return GetWasmEngine()->type_canonicalizer();
}

TypeCanonicalizer::TypeCanonicalizer() { AddPredefinedArrayTypes(); }

void TypeCanonicalizer::CheckMaxCanonicalIndex() const {
  if (V8_UNLIKELY(canonical_supertypes_.size() > kMaxCanonicalTypes)) {
    V8::FatalProcessOutOfMemory(nullptr, "too many canonicalized types");
  }
}

void TypeCanonicalizer::AddRecursiveGroup(WasmModule* module, uint32_t size) {
  if (size == 0) return;
  // If the caller knows statically that {size == 1}, it should have called
  // {AddRecursiveSingletonGroup} directly. For cases where this is not
  // statically determined we add this dispatch here.
  if (size == 1) return AddRecursiveSingletonGroup(module);

  uint32_t start_index = static_cast<uint32_t>(module->types.size() - size);

  // Multiple threads could try to register recursive groups concurrently.
  // TODO(manoskouk): Investigate if we can fine-grain the synchronization.
  base::MutexGuard mutex_guard(&mutex_);
  // Compute the first canonical index in the recgroup in the case that it does
  // not already exist.
  CanonicalTypeIndex first_new_canonical_index{
      static_cast<uint32_t>(canonical_supertypes_.size())};

  // Create a snapshot of the zone; this will be restored in case we find a
  // matching recursion group.
  ZoneSnapshot zone_snapshot = zone_.Snapshot();

  DCHECK_GE(module->types.size(), start_index + size);
  CanonicalGroup group{&zone_, size, first_new_canonical_index};
  for (uint32_t i = 0; i < size; i++) {
    group.types[i] = CanonicalizeTypeDef(
        module, ModuleTypeIndex{start_index + i}, ModuleTypeIndex{start_index},
        first_new_canonical_index);
  }
  if (CanonicalTypeIndex canonical_index = FindCanonicalGroup(group);
      canonical_index.valid()) {
    // Delete zone memory from {CanonicalizeTypeDef} and {CanonicalGroup}.
    zone_snapshot.Restore(&zone_);

    // Identical group found. Map new types to the old types's canonical
    // representatives.
    for (uint32_t i = 0; i < size; i++) {
      CanonicalTypeIndex existing_type_index =
          CanonicalTypeIndex{canonical_index.index + i};
      module->isorecursive_canonical_type_ids[start_index + i] =
          existing_type_index;
    }
    return;
  }
  canonical_supertypes_.resize(first_new_canonical_index.index + size);
  CheckMaxCanonicalIndex();
  canonical_types_.reserve(first_new_canonical_index.index + size, &zone_);
  for (uint32_t i = 0; i < size; i++) {
    CanonicalType& canonical_type = group.types[i];
    CanonicalTypeIndex canonical_id{first_new_canonical_index.index + i};
    // {CanonicalGroup} allocates types in the Zone.
    DCHECK(zone_.Contains(&canonical_type));
    canonical_types_.set(canonical_id, &canonical_type);
    canonical_supertypes_[canonical_id.index] = canonical_type.supertype;
    module->isorecursive_canonical_type_ids[start_index + i] = canonical_id;
  }
  // Check that this canonical ID is not used yet.
  DCHECK(std::none_of(
      canonical_singleton_groups_.begin(), canonical_singleton_groups_.end(),
      [=](auto& entry) { return entry.index == first_new_canonical_index; }));
  DCHECK(std::none_of(
      canonical_groups_.begin(), canonical_groups_.end(),
      [=](auto& entry) { return entry.first == first_new_canonical_index; }));
  canonical_groups_.emplace(group);
}

void TypeCanonicalizer::AddRecursiveSingletonGroup(WasmModule* module) {
  DCHECK(!module->types.empty());
  uint32_t type_index = static_cast<uint32_t>(module->types.size() - 1);
  base::MutexGuard guard(&mutex_);
  CanonicalTypeIndex new_canonical_index{
      static_cast<uint32_t>(canonical_supertypes_.size())};
  // Snapshot the zone before allocating the new type; the zone will be reset if
  // we find an identical type.
  ZoneSnapshot zone_snapshot = zone_.Snapshot();
  CanonicalType type =
      CanonicalizeTypeDef(module, ModuleTypeIndex{type_index},
                          ModuleTypeIndex{type_index}, new_canonical_index);
  CanonicalSingletonGroup group{type, new_canonical_index};
  if (CanonicalTypeIndex index = FindCanonicalGroup(group); index.valid()) {
    zone_snapshot.Restore(&zone_);
    module->isorecursive_canonical_type_ids[type_index] = index;
    return;
  }
  // Check that the new canonical ID is not used yet.
  DCHECK(std::none_of(
      canonical_singleton_groups_.begin(), canonical_singleton_groups_.end(),
      [=](auto& entry) { return entry.index == new_canonical_index; }));
  DCHECK(std::none_of(
      canonical_groups_.begin(), canonical_groups_.end(),
      [=](auto& entry) { return entry.first == new_canonical_index; }));
  // {group.type} is stack-allocated, whereas {canonical_singleton_groups_}
  // creates a long-lived zone-allocated copy of it.
  auto stored_group = canonical_singleton_groups_.emplace(group).first;
  canonical_supertypes_.push_back(type.supertype);
  CheckMaxCanonicalIndex();
  canonical_types_.reserve(new_canonical_index.index + 1, &zone_);
  canonical_types_.set(new_canonical_index, &stored_group->type);
  module->isorecursive_canonical_type_ids[type_index] = new_canonical_index;
}

CanonicalTypeIndex TypeCanonicalizer::AddRecursiveGroup(
    const FunctionSig* sig) {
// Types in the signature must be module-independent.
#if DEBUG
  for (ValueType type : sig->all()) DCHECK(!type.has_index());
#endif
  const bool kFinal = true;
  // Because of the checks above, we can treat the type_def as canonical.
  // TODO(366180605): It would be nice to not have to rely on a cast here.
  // Is there a way to avoid it? In the meantime, these asserts provide at
  // least partial assurances that the cast is safe:
  static_assert(sizeof(CanonicalValueType) == sizeof(ValueType));
  static_assert(
      CanonicalValueType::Primitive(NumericKind::kI32).raw_bit_field() ==
      ValueType::Primitive(kI32).raw_bit_field());
  CanonicalType canonical{reinterpret_cast<const CanonicalSig*>(sig),
                          CanonicalTypeIndex{kNoSuperType}, kFinal, kNotShared};
  base::MutexGuard guard(&mutex_);
  // Fast path lookup before canonicalizing (== copying into the
  // TypeCanonicalizer's zone) the function signature.
  CanonicalTypeIndex new_canonical_index{
      static_cast<uint32_t>(canonical_supertypes_.size())};
  CanonicalTypeIndex index = FindCanonicalGroup(
      CanonicalSingletonGroup{canonical, new_canonical_index});
  if (index.valid()) return index;

  // Copy into this class's zone to store this as a new canonical function type.
  CanonicalSig::Builder builder(&zone_, sig->return_count(),
                                sig->parameter_count());
  for (ValueType ret : sig->returns()) {
    builder.AddReturn(CanonicalValueType{ret});
  }
  for (ValueType param : sig->parameters()) {
    builder.AddParam(CanonicalValueType{param});
  }
  canonical.function_sig = builder.Get();

  CanonicalSingletonGroup group{canonical, new_canonical_index};
  // Copying the signature shouldn't make a difference: There is no match.
  DCHECK(!FindCanonicalGroup(group).valid());
  // Check that the new canonical ID is not used yet.
  DCHECK(std::none_of(
      canonical_singleton_groups_.begin(), canonical_singleton_groups_.end(),
      [=](auto& entry) { return entry.index == new_canonical_index; }));
  DCHECK(std::none_of(
      canonical_groups_.begin(), canonical_groups_.end(),
      [=](auto& entry) { return entry.first == new_canonical_index; }));
  // {group.type} is stack-allocated, whereas {canonical_singleton_groups_}
  // creates a long-lived zone-allocated copy of it.
  const CanonicalSingletonGroup& stored_group =
      *canonical_singleton_groups_.emplace(group).first;
  canonical_supertypes_.push_back(CanonicalTypeIndex{kNoSuperType});
  CheckMaxCanonicalIndex();
  canonical_types_.reserve(new_canonical_index.index + 1, &zone_);
  canonical_types_.set(new_canonical_index, &stored_group.type);
  return new_canonical_index;
}

const CanonicalSig* TypeCanonicalizer::LookupFunctionSignature(
    CanonicalTypeIndex index) const {
  const CanonicalType* type = canonical_types_[index];
  SBXCHECK_EQ(type->kind, CanonicalType::kFunction);
  return type->function_sig;
}

const CanonicalStructType* TypeCanonicalizer::LookupStruct(
    CanonicalTypeIndex index) const {
  const CanonicalType* type = canonical_types_[index];
  SBXCHECK_EQ(type->kind, CanonicalType::kStruct);
  return type->struct_type;
}

const CanonicalArrayType* TypeCanonicalizer::LookupArray(
    CanonicalTypeIndex index) const {
  const CanonicalType* type = canonical_types_[index];
  SBXCHECK_EQ(type->kind, CanonicalType::kArray);
  return type->array_type;
}

void TypeCanonicalizer::AddPredefinedArrayTypes() {
  static constexpr std::pair<CanonicalTypeIndex, CanonicalValueType>
      kPredefinedArrayTypes[] = {{kPredefinedArrayI8Index, {kWasmI8}},
                                 {kPredefinedArrayI16Index, {kWasmI16}}};
  canonical_types_.reserve(kNumberOfPredefinedTypes, &zone_);
  for (auto [index, element_type] : kPredefinedArrayTypes) {
    DCHECK_GT(kNumberOfPredefinedTypes, index.index);
    DCHECK_EQ(index.index, canonical_singleton_groups_.size());
    static constexpr bool kMutable = true;
    static constexpr bool kFinal = true;
    static constexpr bool kShared = false;  // TODO(14616): Fix this.
    CanonicalArrayType* type =
        zone_.New<CanonicalArrayType>(element_type, kMutable);
    CanonicalSingletonGroup group{
        .type = CanonicalType(type, CanonicalTypeIndex{kNoSuperType}, kFinal,
                              kShared),
        .index = index};
    const CanonicalSingletonGroup& stored_group =
        *canonical_singleton_groups_.emplace(group).first;
    canonical_types_.set(index, &stored_group.type);
    canonical_supertypes_.emplace_back(CanonicalTypeIndex{kNoSuperType});
    DCHECK_LE(canonical_supertypes_.size(), kMaxCanonicalTypes);
  }
}

bool TypeCanonicalizer::IsCanonicalSubtype(CanonicalTypeIndex sub_index,
                                           CanonicalTypeIndex super_index) {
  // Fast path without synchronization:
  if (sub_index == super_index) return true;

  // Multiple threads could try to register and access recursive groups
  // concurrently.
  // TODO(manoskouk): Investigate if we can improve this synchronization.
  base::MutexGuard mutex_guard(&mutex_);
  return IsCanonicalSubtype_Locked(sub_index, super_index);
}
bool TypeCanonicalizer::IsCanonicalSubtype_Locked(
    CanonicalTypeIndex sub_index, CanonicalTypeIndex super_index) const {
  while (sub_index.valid()) {
    if (sub_index == super_index) return true;
    // TODO(jkummerow): Investigate if replacing this with
    // `sub_index = canonical_types_[sub_index].supertype;`
    // has acceptable performance, which would allow us to save the memory
    // cost of storing {canonical_supertypes_}.
    sub_index = canonical_supertypes_[sub_index.index];
  }
  return false;
}

bool TypeCanonicalizer::IsCanonicalSubtype(ModuleTypeIndex sub_index,
                                           ModuleTypeIndex super_index,
                                           const WasmModule* sub_module,
                                           const WasmModule* super_module) {
  CanonicalTypeIndex canonical_super =
      super_module->canonical_type_id(super_index);
  CanonicalTypeIndex canonical_sub = sub_module->canonical_type_id(sub_index);
  return IsCanonicalSubtype(canonical_sub, canonical_super);
}

bool TypeCanonicalizer::IsHeapSubtype(CanonicalTypeIndex sub,
                                      CanonicalTypeIndex super) const {
  DCHECK_NE(sub, super);
  base::MutexGuard mutex_guard(&mutex_);
  return IsCanonicalSubtype_Locked(sub, super);
}

void TypeCanonicalizer::EmptyStorageForTesting() {
  // Any remaining native modules might reference the types we're about to
  // clear.
  CHECK_EQ(GetWasmEngine()->NativeModuleCount(), 0);

  base::MutexGuard mutex_guard(&mutex_);
  canonical_types_.ClearForTesting();
  canonical_supertypes_.clear();
  canonical_groups_.clear();
  canonical_singleton_groups_.clear();
  zone_.Reset();
  AddPredefinedArrayTypes();
}

TypeCanonicalizer::CanonicalType TypeCanonicalizer::CanonicalizeTypeDef(
    const WasmModule* module, ModuleTypeIndex module_type_idx,
    ModuleTypeIndex recgroup_start,
    CanonicalTypeIndex canonical_recgroup_start) {
  mutex_.AssertHeld();  // The caller must hold the mutex.

  auto CanonicalizeTypeIndex = [=](ModuleTypeIndex type_index) {
    if (!type_index.valid()) return CanonicalTypeIndex::Invalid();
    DCHECK(type_index.valid());
    if (type_index < recgroup_start) {
      // This references a type from an earlier recgroup; use the
      // already-canonicalized type index.
      return module->canonical_type_id(type_index);
    }
    // For types within the same recgroup, generate indexes assuming that this
    // is a new canonical recgroup. To prevent truncation in the
    // CanonicalValueType's bit field, we must check the range here.
    uint32_t new_index = canonical_recgroup_start.index +
                         (type_index.index - recgroup_start.index);
    if (V8_UNLIKELY(new_index >= kMaxCanonicalTypes)) {
      V8::FatalProcessOutOfMemory(nullptr, "too many canonicalized types");
    }
    return CanonicalTypeIndex{new_index};
  };

  auto CanonicalizeValueType = [=](ValueType type) {
    if (!type.has_index()) return CanonicalValueType{type};
    static_assert(kMaxCanonicalTypes <=
                  (1 << CanonicalValueType::kNumIndexBits));
    return type.Canonicalize(CanonicalizeTypeIndex(type.ref_index()));
  };

  TypeDefinition type = module->type(module_type_idx);
  CanonicalTypeIndex supertype = CanonicalizeTypeIndex(type.supertype);
  switch (type.kind) {
    case TypeDefinition::kFunction: {
      const FunctionSig* original_sig = type.function_sig;
      CanonicalSig::Builder builder(&zone_, original_sig->return_count(),
                                    original_sig->parameter_count());
      for (ValueType ret : original_sig->returns()) {
        builder.AddReturn(CanonicalizeValueType(ret));
      }
      for (ValueType param : original_sig->parameters()) {
        builder.AddParam(CanonicalizeValueType(param));
      }
      return CanonicalType(builder.Get(), supertype, type.is_final,
                           type.is_shared);
    }
    case TypeDefinition::kStruct: {
      const StructType* original_type = type.struct_type;
      CanonicalStructType::Builder builder(&zone_, original_type->field_count(),
                                           original_type->is_descriptor());
      for (uint32_t i = 0; i < original_type->field_count(); i++) {
        builder.AddField(CanonicalizeValueType(original_type->field(i)),
                         original_type->mutability(i),
                         original_type->field_offset(i));
      }
      builder.set_total_fields_size(original_type->total_fields_size());
      return CanonicalType(
          builder.Build(CanonicalStructType::Builder::kUseProvidedOffsets),
          supertype, CanonicalizeTypeIndex(type.descriptor),
          CanonicalizeTypeIndex(type.describes), type.is_final, type.is_shared);
    }
    case TypeDefinition::kArray: {
      CanonicalValueType element_type =
          CanonicalizeValueType(type.array_type->element_type());
      CanonicalArrayType* array_type = zone_.New<CanonicalArrayType>(
          element_type, type.array_type->mutability());
      return CanonicalType(array_type, supertype, type.is_final,
                           type.is_shared);
    }
    case TypeDefinition::kCont: {
      CanonicalTypeIndex canonical_index =
          CanonicalizeTypeIndex(type.cont_type->contfun_typeindex());
      CanonicalContType* canonical_cont =
          zone_.New<CanonicalContType>(canonical_index);
      return CanonicalType(canonical_cont, supertype, type.is_final,
                           type.is_shared);
    }
  }
}

// Returns the index of the canonical representative of the first type in this
// group if it exists, and `CanonicalTypeIndex::Invalid()` otherwise.
CanonicalTypeIndex TypeCanonicalizer::FindCanonicalGroup(
    const CanonicalGroup& group) const {
  // Groups of size 0 do not make sense here; groups of size 1 should use
  // {CanonicalSingletonGroup} (see below).
  DCHECK_LT(1, group.types.size());
  auto it = canonical_groups_.find(group);
  return it == canonical_groups_.end() ? CanonicalTypeIndex::Invalid()
                                       : it->first;
}

// Returns the canonical index of the given group if it already exists.
CanonicalTypeIndex TypeCanonicalizer::FindCanonicalGroup(
    const CanonicalSingletonGroup& group) const {
  auto it = canonical_singleton_groups_.find(group);
  static_assert(kMaxCanonicalTypes <= kMaxInt);
  return it == canonical_singleton_groups_.end() ? CanonicalTypeIndex::Invalid()
                                                 : it->index;
}

size_t TypeCanonicalizer::EstimateCurrentMemoryConsumption() const {
  UPDATE_WHEN_CLASS_CHANGES(TypeCanonicalizer, 8048);
  // The storage of the canonical group's types is accounted for via the
  // allocator below (which tracks the zone memory).
  base::MutexGuard mutex_guard(&mutex_);
  size_t result = ContentSize(canonical_supertypes_);
  result += ContentSize(canonical_groups_);
  result += ContentSize(canonical_singleton_groups_);
  // Note: the allocator also tracks zone allocations of `canonical_types_`.
  result += allocator_.GetCurrentMemoryUsage();
  if (v8_flags.trace_wasm_offheap_memory) {
    PrintF("TypeCanonicalizer: %zu\n", result);
  }
  return result;
}

size_t TypeCanonicalizer::GetCurrentNumberOfTypes() const {
  base::MutexGuard mutex_guard(&mutex_);
  return canonical_supertypes_.size();
}

// static
void TypeCanonicalizer::PrepareForCanonicalTypeId(Isolate* isolate,
                                                  CanonicalTypeIndex id) {
  if (!id.valid()) return;
  Heap* heap = isolate->heap();
  // {2 * (id + 1)} needs to fit in an int.
  CHECK_LE(id.index, kMaxInt / 2 - 1);
  // Canonical types and wrappers are zero-indexed.
  const int length = id.index + 1;
  // The fast path is non-handlified.
  Tagged<WeakFixedArray> old_rtts_raw = heap->wasm_canonical_rtts();
  Tagged<WeakFixedArray> old_wrappers_raw = heap->js_to_wasm_wrappers();

  // Fast path: Lengths are sufficient.
  int old_length = old_rtts_raw->length();
  DCHECK_EQ(old_length, old_wrappers_raw->length());
  if (old_length >= length) return;

  // Allocate bigger WeakFixedArrays for rtts and wrappers. Grow them
  // exponentially.
  const int new_length = std::max(old_length * 3 / 2, length);
  CHECK_LT(old_length, new_length);

  // Allocation can invalidate previous unhandled pointers.
  DirectHandle<WeakFixedArray> old_rtts{old_rtts_raw, isolate};
  DirectHandle<WeakFixedArray> old_wrappers{old_wrappers_raw, isolate};
  old_rtts_raw = old_wrappers_raw = {};

  // We allocate the WeakFixedArray filled with undefined values, as we cannot
  // pass the cleared value in a handle (see https://crbug.com/364591622). We
  // overwrite the new entries via {MemsetTagged} afterwards.
  DirectHandle<WeakFixedArray> new_rtts =
      WeakFixedArray::New(isolate, new_length, AllocationType::kOld);
  WeakFixedArray::CopyElements(isolate, *new_rtts, 0, *old_rtts, 0, old_length);
  MemsetTagged(new_rtts->RawFieldOfFirstElement() + old_length,
               ClearedValue(isolate), new_length - old_length);
  DirectHandle<WeakFixedArray> new_wrappers =
      WeakFixedArray::New(isolate, new_length, AllocationType::kOld);
  WeakFixedArray::CopyElements(isolate, *new_wrappers, 0, *old_wrappers, 0,
                               old_length);
  MemsetTagged(new_wrappers->RawFieldOfFirstElement() + old_length,
               ClearedValue(isolate), new_length - old_length);
  heap->SetWasmCanonicalRttsAndJSToWasmWrappers(*new_rtts, *new_wrappers);
}

// static
void TypeCanonicalizer::ClearWasmCanonicalTypesForTesting(Isolate* isolate) {
  ReadOnlyRoots roots(isolate);
  isolate->heap()->SetWasmCanonicalRttsAndJSToWasmWrappers(
      roots.empty_weak_fixed_array(), roots.empty_weak_fixed_array());
}

bool TypeCanonicalizer::IsFunctionSignature(CanonicalTypeIndex index) const {
  return canonical_types_[index]->kind == CanonicalType::kFunction;
}
bool TypeCanonicalizer::IsStruct(CanonicalTypeIndex index) const {
  return canonical_types_[index]->kind == CanonicalType::kStruct;
}
bool TypeCanonicalizer::IsArray(CanonicalTypeIndex index) const {
  return canonical_types_[index]->kind == CanonicalType::kArray;
}
bool TypeCanonicalizer::IsShared(CanonicalTypeIndex index) const {
  return canonical_types_[index]->is_shared;
}

CanonicalTypeIndex TypeCanonicalizer::FindIndex_Slow(
    const CanonicalSig* sig) const {
  // TODO(397489547): Make this faster. The plan is to allocate an extra
  // slot in the Zone immediately preceding each CanonicalSig, so we can
  // get from the sig's address to that slot's address via pointer arithmetic.
  // For now, just search through all known signatures, which is acceptable
  // as long as only the type-reflection proposal needs this.
  // TODO(42210967): Improve this before shipping Type Reflection.
  return canonical_types_.FindIndex_Slow(sig);
}

#ifdef DEBUG
bool TypeCanonicalizer::Contains(const CanonicalSig* sig) const {
  base::MutexGuard mutex_guard(&mutex_);
  return zone_.Contains(sig);
}
#endif

}  // namespace v8::internal::wasm
