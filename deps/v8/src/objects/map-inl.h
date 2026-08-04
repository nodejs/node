// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_MAP_INL_H_
#define V8_OBJECTS_MAP_INL_H_

#include "src/objects/map.h"
// Include the non-inl header before the rest of the headers.

#include "src/common/globals.h"
#include "src/heap/heap-layout-inl.h"
#include "src/heap/heap-write-barrier-inl.h"
#include "src/objects/api-callbacks-inl.h"
#include "src/objects/cell-inl.h"
#include "src/objects/dependent-code.h"
#include "src/objects/descriptor-array-inl.h"
#include "src/objects/dictionary.h"
#include "src/objects/field-type.h"
#include "src/objects/heap-object.h"
#include "src/objects/hole.h"
#include "src/objects/instance-type-inl.h"
#include "src/objects/js-function-inl.h"
#include "src/objects/literal-objects.h"
#include "src/objects/map-updater.h"
#include "src/objects/object-predicates-inl.h"
#include "src/objects/property.h"
#include "src/objects/prototype-info-inl.h"
#include "src/objects/shared-function-info-inl.h"
#include "src/objects/templates-inl.h"
#include "src/objects/transitions-inl.h"
#include "src/objects/transitions.h"
#include "src/utils/memcopy.h"

#if V8_ENABLE_WEBASSEMBLY
#include "src/wasm/wasm-objects.h"
#endif  // V8_ENABLE_WEBASSEMBLY

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

#include "torque-generated/src/objects/map-tq-inl.inc"

// `instance_descriptors_` is a TaggedMember<UnionOf<DescriptorArray,
// WasmStruct>> on Wasm builds; the `instance_descriptors` and
// `custom_descriptor` accessors are deliberately overlapping narrower views
// of that single slot, dispatched on instance type by the caller.
#if V8_ENABLE_WEBASSEMBLY
Tagged<DescriptorArray> Map::instance_descriptors() const {
  Tagged<DescriptorArray> value =
      Cast<DescriptorArray>(instance_descriptors_.load());
  // Fetching the instance descriptors of a Wasm map is safe as long as
  // that's the empty descriptor array (and not a Custom Descriptor).
  DCHECK(!IsWasmStructMap(this) || HeapLayout::InReadOnlySpace(value));
  return value;
}
void Map::set_instance_descriptors(Tagged<DescriptorArray> value,
                                   WriteBarrierMode mode) {
  instance_descriptors_.store(this, value, mode);
}
Tagged<UnionOf<DescriptorArray, WasmStruct>> Map::custom_descriptor() const {
  DCHECK(IsWasmStructMap(this));
  return instance_descriptors_.load();
}
void Map::set_custom_descriptor(Tagged<WasmStruct> value,
                                WriteBarrierMode mode) {
  DCHECK(IsWasmStructMap(this));
  instance_descriptors_.store(this, value, mode);
}
#else
Tagged<DescriptorArray> Map::instance_descriptors() const {
  return instance_descriptors_.load();
}
void Map::set_instance_descriptors(Tagged<DescriptorArray> value,
                                   WriteBarrierMode mode) {
  instance_descriptors_.store(this, value, mode);
}
#endif  // V8_ENABLE_WEBASSEMBLY

Tagged<DescriptorArray> Map::instance_descriptors(AcquireLoadTag) const {
#if V8_ENABLE_WEBASSEMBLY
  return Cast<DescriptorArray>(instance_descriptors_.Acquire_Load());
#else
  return instance_descriptors_.Acquire_Load();
#endif  // V8_ENABLE_WEBASSEMBLY
}
void Map::set_instance_descriptors(Tagged<DescriptorArray> value,
                                   ReleaseStoreTag, WriteBarrierMode mode) {
  instance_descriptors_.Release_Store(this, value, mode);
}

// `raw_transitions` is a reinterpret-load of the shared
// `transitions_or_prototype_info_` slot. The returned value may be any
// member of the full field union (transitions or prototype info);
// consumers dispatch on the runtime type (see
// TransitionsAccessor::GetEncoding). The `prototype_info` accessors are
// a separate narrower view restricted by is_prototype_map() /
// IsWasmObjectMap() DCHECKs at the call site.
Tagged<Map::RawTransitionsT> Map::raw_transitions() const {
  return transitions_or_prototype_info_.load();
}
void Map::set_raw_transitions(Tagged<Map::RawTransitionsT> value,
                              WriteBarrierMode mode) {
  transitions_or_prototype_info_.store(this, value, mode);
}
Tagged<Map::RawTransitionsT> Map::raw_transitions(AcquireLoadTag) const {
  return transitions_or_prototype_info_.Acquire_Load();
}
void Map::set_raw_transitions(Tagged<Map::RawTransitionsT> value,
                              ReleaseStoreTag, WriteBarrierMode mode) {
  transitions_or_prototype_info_.Release_Store(this, value, mode);
}

// `prototype_` field type is exactly Tagged<JSPrototype> (alias for
// Union<JSReceiver, Null>); no narrowing needed.
Tagged<JSPrototype> Map::prototype() const { return prototype_.load(); }
void Map::set_prototype(Tagged<JSPrototype> value, WriteBarrierMode mode) {
  DCHECK(IsNull(value) || IsJSProxy(value) || IsWasmObject(value) ||
         (IsJSObject(value) && (HeapLayout::InWritableSharedSpace(value) ||
                                value->map()->is_prototype_map())));
  prototype_.store(this, value, mode);
}

Tagged<UnionOf<Smi, PrototypeInfo, PrototypeSharedClosureInfo>>
Map::prototype_info() const {
#if V8_ENABLE_WEBASSEMBLY
  DCHECK(this->is_prototype_map() || IsWasmObjectMap(this));
#else
  DCHECK(this->is_prototype_map());
#endif  // V8_ENABLE_WEBASSEMBLY
  return Cast<UnionOf<Smi, PrototypeInfo, PrototypeSharedClosureInfo>>(
      transitions_or_prototype_info_.load());
}

Tagged<UnionOf<Smi, PrototypeInfo, PrototypeSharedClosureInfo>>
Map::prototype_info(AcquireLoadTag) const {
  return Cast<UnionOf<Smi, PrototypeInfo, PrototypeSharedClosureInfo>>(
      transitions_or_prototype_info_.Acquire_Load());
}
void Map::set_prototype_info(
    Tagged<UnionOf<Smi, PrototypeInfo, PrototypeSharedClosureInfo>> value,
    ReleaseStoreTag, WriteBarrierMode mode) {
  transitions_or_prototype_info_.Release_Store(this, value, mode);
}

void Map::init_prototype_and_constructor_or_back_pointer(ReadOnlyRoots roots) {
  Tagged<HeapObject> null = roots.null_value();
  constructor_or_back_pointer_or_native_context_.store(this, null,
                                                       SKIP_WRITE_BARRIER);
  prototype_.store(this, Cast<JSPrototype>(null), SKIP_WRITE_BARRIER);
}

void Map::init_prototype_and_constructor_or_back_pointer_during_bootstrap(
    ReadOnlyRoots roots) {
  Tagged<HeapObject> null = roots.null_value();
  constructor_or_back_pointer_or_native_context_.store(this, null,
                                                       SKIP_WRITE_BARRIER);
  // UncheckedCast because Cast<JSPrototype>'s IsNull check goes through
  // GetReadOnlyRoots() which is not yet usable during early RO-heap
  // initialization.
  prototype_.store(this, UncheckedCast<JSPrototype>(null), SKIP_WRITE_BARRIER);
}

// PtrComprCageBase forwarding overloads. The cage_base parameter is a no-op
// for HeapObjectLayout subclasses since fields are accessed via direct memory
// reads (no decompression hint needed). These exist so that callers continue
// to work without per-call-site changes.
#if V8_ENABLE_WEBASSEMBLY
Tagged<DescriptorArray> Map::instance_descriptors(PtrComprCageBase) const {
  return instance_descriptors();
}
Tagged<UnionOf<DescriptorArray, WasmStruct>> Map::custom_descriptor(
    PtrComprCageBase) const {
  return custom_descriptor();
}
Tagged<Map> Map::immediate_supertype_map(PtrComprCageBase) const {
  return immediate_supertype_map();
}
Tagged<WasmTypeInfo> Map::wasm_type_info(PtrComprCageBase) const {
  return wasm_type_info();
}
#else
Tagged<DescriptorArray> Map::instance_descriptors(PtrComprCageBase) const {
  return instance_descriptors();
}
#endif  // V8_ENABLE_WEBASSEMBLY
Tagged<DescriptorArray> Map::instance_descriptors(PtrComprCageBase,
                                                  AcquireLoadTag tag) const {
  return instance_descriptors(tag);
}
Tagged<Map::RawTransitionsT> Map::raw_transitions(PtrComprCageBase) const {
  return raw_transitions();
}
Tagged<Map::RawTransitionsT> Map::raw_transitions(PtrComprCageBase,
                                                  AcquireLoadTag tag) const {
  return raw_transitions(tag);
}
Tagged<JSPrototype> Map::prototype(PtrComprCageBase) const {
  return prototype();
}
Tagged<UnionOf<Smi, PrototypeInfo, PrototypeSharedClosureInfo>>
Map::prototype_info(PtrComprCageBase) const {
  return prototype_info();
}
Tagged<UnionOf<Smi, PrototypeInfo, PrototypeSharedClosureInfo>>
Map::prototype_info(PtrComprCageBase, AcquireLoadTag tag) const {
  return prototype_info(tag);
}
Tagged<DependentCode> Map::dependent_code(PtrComprCageBase) const {
  return dependent_code();
}
Tagged<UnionOf<Smi, Cell>> Map::prototype_validity_cell(
    PtrComprCageBase, RelaxedLoadTag tag) const {
  return prototype_validity_cell(tag);
}
Tagged<Object> Map::constructor_or_back_pointer(PtrComprCageBase) const {
  return constructor_or_back_pointer();
}
Tagged<Object> Map::constructor_or_back_pointer(PtrComprCageBase,
                                                RelaxedLoadTag tag) const {
  return constructor_or_back_pointer(tag);
}
Tagged<NativeContext> Map::native_context(PtrComprCageBase) const {
  return native_context();
}
Tagged<Object> Map::native_context_or_null(PtrComprCageBase) const {
  return native_context_or_null();
}
Tagged<Object> Map::raw_native_context_or_null(PtrComprCageBase) const {
  return raw_native_context_or_null();
}

// constructor_or_back_pointer_or_native_context / transitions_or_prototype_info
// are direct-field accessors (separate from the type-narrowing
// constructor_or_back_pointer / native_context / etc. above).
Tagged<Object> Map::constructor_or_back_pointer_or_native_context() const {
  return constructor_or_back_pointer_or_native_context_.load();
}
Tagged<Object> Map::constructor_or_back_pointer_or_native_context(
    PtrComprCageBase) const {
  return constructor_or_back_pointer_or_native_context();
}
void Map::set_constructor_or_back_pointer_or_native_context(
    Tagged<Object> value, WriteBarrierMode mode) {
  constructor_or_back_pointer_or_native_context_.store(this, value, mode);
}
Tagged<Object> Map::transitions_or_prototype_info() const {
  return UncheckedCast<Object>(transitions_or_prototype_info_.load());
}
Tagged<Object> Map::transitions_or_prototype_info(PtrComprCageBase) const {
  return transitions_or_prototype_info();
}
void Map::set_transitions_or_prototype_info(Tagged<Object> value,
                                            WriteBarrierMode mode) {
  using FieldT = UnionOf<Smi, MaybeWeak<Map>, TransitionArray, PrototypeInfo,
                         PrototypeSharedClosureInfo>;
  transitions_or_prototype_info_.store(this, UncheckedCast<FieldT>(value),
                                       mode);
}

// |bit_field| fields.
// Concurrent access to |is_extended_map| and |has_non_instance_prototype|
// is explicitly allowlisted here. The former is never modified after the map
// is setup but it's being read by concurrent marker when pointer compression
// is enabled. The latter bit can be modified on a live objects.
BIT_FIELD_ACCESSORS(Map, relaxed_bit_field, has_non_instance_prototype,
                    Map::Bits1::HasNonInstancePrototypeBit)
BIT_FIELD_ACCESSORS(Map, relaxed_bit_field, is_extended_map,
                    Map::Bits1::IsExtendedMapBit)

// These are fine to be written as non-atomic since we don't have data races.
// However, they have to be read atomically from the background since the
// |bit_field| as a whole can mutate when using the above setters.
BIT_FIELD_ACCESSORS2(Map, relaxed_bit_field, bit_field, is_callable,
                     Map::Bits1::IsCallableBit)
BIT_FIELD_ACCESSORS2(Map, relaxed_bit_field, bit_field, has_named_interceptor,
                     Map::Bits1::HasNamedInterceptorBit)
BIT_FIELD_ACCESSORS2(Map, relaxed_bit_field, bit_field, has_indexed_interceptor,
                     Map::Bits1::HasIndexedInterceptorBit)
BIT_FIELD_ACCESSORS2(Map, relaxed_bit_field, bit_field, is_undetectable,
                     Map::Bits1::IsUndetectableBit)
BIT_FIELD_ACCESSORS2(Map, relaxed_bit_field, bit_field, is_access_check_needed,
                     Map::Bits1::IsAccessCheckNeededBit)
BIT_FIELD_ACCESSORS2(Map, relaxed_bit_field, bit_field, is_constructor,
                     Map::Bits1::IsConstructorBit)

// |bit_field2| fields.
BIT_FIELD_ACCESSORS(Map, bit_field2, new_target_is_base,
                    Map::Bits2::NewTargetIsBaseBit)
BIT_FIELD_ACCESSORS(Map, bit_field2, is_immutable_proto,
                    Map::Bits2::IsImmutablePrototypeBit)

// |bit_field3| fields.
BIT_FIELD_ACCESSORS(Map, relaxed_bit_field3, owns_descriptors,
                    Map::Bits3::OwnsDescriptorsBit)
BIT_FIELD_ACCESSORS(Map, release_acquire_bit_field3, is_deprecated,
                    Map::Bits3::IsDeprecatedBit)
BIT_FIELD_ACCESSORS(Map, relaxed_bit_field3, is_in_retained_map_list,
                    Map::Bits3::IsInRetainedMapListBit)
BIT_FIELD_ACCESSORS(Map, release_acquire_bit_field3, is_prototype_map,
                    Map::Bits3::IsPrototypeMapBit)
BIT_FIELD_ACCESSORS(Map, relaxed_bit_field3, is_migration_target,
                    Map::Bits3::IsMigrationTargetBit)
BIT_FIELD_ACCESSORS2(Map, relaxed_bit_field3, bit_field3, is_extensible,
                     Map::Bits3::IsExtensibleBit)
BIT_FIELD_ACCESSORS(Map, bit_field3, may_have_interesting_properties,
                    Map::Bits3::MayHaveInterestingPropertiesBit)
BIT_FIELD_ACCESSORS(Map, relaxed_bit_field3, construction_counter,
                    Map::Bits3::ConstructionCounterBits)

Tagged<InterceptorInfo> Map::GetNamedInterceptor() const {
  DCHECK(has_named_interceptor());
  Tagged<FunctionTemplateInfo> info = GetFunctionTemplateInfo();
  return Cast<InterceptorInfo>(info->GetNamedPropertyHandler());
}

Tagged<InterceptorInfo> Map::GetIndexedInterceptor() const {
  DCHECK(has_indexed_interceptor());
  Tagged<FunctionTemplateInfo> info = GetFunctionTemplateInfo();
  return Cast<InterceptorInfo>(info->GetIndexedPropertyHandler());
}

// static
bool Map::IsMostGeneralFieldType(Representation representation,
                                 Tagged<FieldType> field_type) {
  return !representation.IsHeapObject() || IsAny(field_type);
}

// static
bool Map::CanHaveFastTransitionableElementsKind(InstanceType instance_type) {
  return instance_type == JS_ARRAY_TYPE ||
         instance_type == JS_PRIMITIVE_WRAPPER_TYPE ||
         instance_type == JS_ARGUMENTS_OBJECT_TYPE;
}

bool Map::CanHaveFastTransitionableElementsKind() const {
  return CanHaveFastTransitionableElementsKind(instance_type());
}

bool Map::IsDetached(Isolate* isolate) const {
  if (is_prototype_map()) return true;
  return instance_type() == JS_OBJECT_TYPE && NumberOfOwnDescriptors() > 0 &&
         IsUndefined(GetBackPointer(), isolate);
}

// static
void Map::GeneralizeIfCanHaveTransitionableFastElementsKind(
    Isolate* isolate, InstanceType instance_type,
    Representation* representation, DirectHandle<FieldType>* field_type) {
  if (CanHaveFastTransitionableElementsKind(instance_type)) {
    // We don't support propagation of field generalization through elements
    // kind transitions because they are inserted into the transition tree
    // before field transitions. In order to avoid complexity of handling
    // such a case we ensure that all maps with transitionable elements kinds
    // have the most general field representation and type.
    *field_type = FieldType::Any(isolate);
    *representation = Representation::Tagged();
  }
}

Handle<Map> Map::Normalize(Isolate* isolate, DirectHandle<Map> fast_map,
                           PropertyNormalizationMode mode, const char* reason) {
  const bool kUseCache = true;
  return Normalize(isolate, fast_map, fast_map->elements_kind(), {}, mode,
                   kUseCache, reason);
}

bool Map::EquivalentToForNormalization(const Tagged<Map> other,
                                       PropertyNormalizationMode mode) const {
  return EquivalentToForNormalization(other, elements_kind(), prototype(),
                                      mode);
}

bool Map::TooManyFastProperties(StoreOrigin store_origin) const {
  if (UnusedPropertyFields() != 0) return false;
  if (store_origin != StoreOrigin::kMaybeKeyed) return false;
  if (is_prototype_map()) return false;
  int limit = std::max(
      {v8_flags.fast_properties_soft_limit.value(), GetInObjectProperties()});
  int external =
      NumberOfFields(ConcurrencyMode::kSynchronous) - GetInObjectProperties();
  return external > limit;
}

Tagged<Name> Map::GetLastDescriptorName(Isolate* isolate) const {
  return instance_descriptors(isolate)->GetKey(LastAdded());
}

PropertyDetails Map::GetLastDescriptorDetails(Isolate* isolate) const {
  return instance_descriptors(isolate)->GetDetails(LastAdded());
}

InternalIndex Map::LastAdded() const {
  int number_of_own_descriptors = NumberOfOwnDescriptors();
  DCHECK_GT(number_of_own_descriptors, 0);
  return InternalIndex(number_of_own_descriptors - 1);
}

// TODO(375937549): Convert to uint32_t.
int Map::NumberOfOwnDescriptors() const {
  return Bits3::NumberOfOwnDescriptorsBits::decode(
      release_acquire_bit_field3());
}

void Map::SetNumberOfOwnDescriptors(int number) {
  DCHECK_LE(number, instance_descriptors()->number_of_descriptors());
  CHECK_LE(static_cast<unsigned>(number),
           static_cast<unsigned>(kMaxNumberOfDescriptors));
  set_release_acquire_bit_field3(
      Bits3::NumberOfOwnDescriptorsBits::update(bit_field3(), number));
}

InternalIndex::Range Map::IterateOwnDescriptors() const {
  return InternalIndex::Range(NumberOfOwnDescriptors());
}

int Map::EnumLength() const {
  return Bits3::EnumLengthBits::decode(bit_field3());
}

void Map::SetEnumLength(int length) {
  if (length != kInvalidEnumCacheSentinel) {
    DCHECK_LE(length, NumberOfOwnDescriptors());
    CHECK_LE(static_cast<unsigned>(length),
             static_cast<unsigned>(kMaxNumberOfDescriptors));
  }
  set_relaxed_bit_field3(Bits3::EnumLengthBits::update(bit_field3(), length));
}

Tagged<FixedArrayBase> Map::GetInitialElements() const {
  Tagged<FixedArrayBase> result;
  if (has_fast_elements() || has_fast_string_wrapper_elements() ||
      has_any_nonextensible_elements()) {
    result = GetReadOnlyRoots().empty_fixed_array();
  } else if (has_typed_array_or_rab_gsab_typed_array_elements()) {
    result = GetReadOnlyRoots().empty_byte_array();
  } else if (has_dictionary_elements()) {
    result = GetReadOnlyRoots().empty_slow_element_dictionary();
  } else {
    UNREACHABLE();
  }
  DCHECK(!HeapLayout::InYoungGeneration(result));
  return result;
}

VisitorId Map::visitor_id() const {
  return static_cast<VisitorId>(visitor_id_.load(std::memory_order_relaxed));
}

void Map::set_visitor_id(VisitorId id) {
  CHECK_LT(static_cast<unsigned>(id), 256);
  visitor_id_.store(static_cast<uint8_t>(id), std::memory_order_relaxed);
}

int Map::instance_size_in_words() const {
  return instance_size_in_words_.load(std::memory_order_relaxed);
}

void Map::set_instance_size_in_words(int value) {
  instance_size_in_words_.store(static_cast<uint8_t>(value),
                                std::memory_order_relaxed);
}

int Map::instance_size() const {
  return instance_size_in_words() << kTaggedSizeLog2;
}

void Map::set_instance_size(int size_in_bytes) {
  CHECK(IsAligned(size_in_bytes, kTaggedSize));
  DCHECK_LE(static_cast<unsigned>(size_in_bytes), JSObject::kMaxInstanceSize);
  int size_in_words = size_in_bytes >>= kTaggedSizeLog2;
  CHECK_LE(static_cast<unsigned>(size_in_words), kMaxUInt8);
  set_instance_size_in_words(size_in_words);
}

uint8_t Map::inobject_properties_start_or_constructor_function_index() const {
  // TODO(solanes, v8:7790, v8:11353): Make this and the setter non-atomic
  // when TSAN sees the map's store synchronization.
  return inobject_properties_start_or_constructor_function_index_.load(
      std::memory_order_relaxed);
}

void Map::set_inobject_properties_start_or_constructor_function_index(
    uint8_t value) {
  inobject_properties_start_or_constructor_function_index_.store(
      value, std::memory_order_relaxed);
}

uint8_t Map::GetInObjectPropertiesStartInWords() const {
  DCHECK(IsJSObjectMap(this));
  return inobject_properties_start_or_constructor_function_index();
}

void Map::SetInObjectPropertiesStartInWords(uint8_t value) {
  CHECK(IsJSObjectMap(this));
  set_inobject_properties_start_or_constructor_function_index(value);
}

void Map::SetInObjectPropertiesStartInWords(int value) {
  SetInObjectPropertiesStartInWords(base::checked_cast<uint8_t>(value));
}

bool Map::HasOutOfObjectProperties() const {
  bool ret = used_or_unused_instance_size_in_words() < JSObject::kFieldsAdded;
  DCHECK_EQ(ret, GetInObjectProperties() <
                     NumberOfFields(ConcurrencyMode::kSynchronous));
  return ret;
}

int Map::GetInObjectProperties() const {
  DCHECK(IsJSObjectMap(this));
  return instance_size_in_words() - GetInObjectPropertiesStartInWords();
}

bool Map::IsFieldInObject(int field_index) const {
  return field_index < GetInObjectProperties();
}

int Map::GetConstructorFunctionIndex() const {
#if V8_ENABLE_WEBASSEMBLY
  // We allow WasmNull here so builtins can produce error messages when
  // called from Wasm, without having to special-case WasmNull at every
  // caller of such a builtin.
  DCHECK(IsPrimitiveMap(this) || instance_type() == WASM_NULL_TYPE);
#else
  DCHECK(IsPrimitiveMap(this));
#endif
  return inobject_properties_start_or_constructor_function_index();
}

void Map::SetConstructorFunctionIndex(int value) {
  CHECK(IsPrimitiveMap(this));
  set_inobject_properties_start_or_constructor_function_index(value);
}

int Map::GetInObjectPropertyOffset(int index) const {
  return (GetInObjectPropertiesStartInWords() + index) * kTaggedSize;
}

DirectHandle<Map> Map::AddMissingTransitionsForTesting(
    Isolate* isolate, DirectHandle<Map> split_map,
    DirectHandle<DescriptorArray> descriptors) {
  return AddMissingTransitions(isolate, split_map, descriptors);
}

void Map::set_instance_type(InstanceType value) {
  instance_type_.store(value, std::memory_order_relaxed);
}

int Map::AllocatedSize() const {
  if (is_extended_map()) [[unlikely]] {
    // This is an extended map, figure out its size from the extended map kind.
    Tagged<ExtendedMap> self = UncheckedCast<ExtendedMap>(this);
    return self->map_size();
  }
  // This is either a meta map or a regular map. Currently they have the same
  // size.
  return Map::kSize;
}

int Map::UnusedPropertyFields() const {
#if V8_ENABLE_WEBASSEMBLY
  DCHECK(!IsWasmObjectMap(this));
#endif  // V8_ENABLE_WEBASSEMBLY
  int value = used_or_unused_instance_size_in_words();
  DCHECK_IMPLIES(!IsJSObjectMap(this), value == 0);
  int unused;
  if (value >= JSObject::kFieldsAdded) {
    unused = instance_size_in_words() - value;
  } else {
    // For out of object properties "used_or_unused_instance_size_in_words"
    // byte encodes the slack in the property array.
    unused = value;
  }
  return unused;
}

int Map::UnusedInObjectProperties() const {
  // Like Map::UnusedPropertyFields(), but returns 0 for out of object
  // properties.
#if V8_ENABLE_WEBASSEMBLY
  DCHECK(!IsWasmObjectMap(this));
#endif  // V8_ENABLE_WEBASSEMBLY
  int value = used_or_unused_instance_size_in_words();
  DCHECK_IMPLIES(!IsJSObjectMap(this), value == 0);
  if (value >= JSObject::kFieldsAdded) {
    return instance_size_in_words() - value;
  }
  return 0;
}

int Map::used_or_unused_instance_size_in_words() const {
  return used_or_unused_instance_size_in_words_.load(std::memory_order_relaxed);
}

void Map::set_used_or_unused_instance_size_in_words(int value) {
  CHECK_LE(static_cast<unsigned>(value), 255);
  used_or_unused_instance_size_in_words_.store(static_cast<uint8_t>(value),
                                               std::memory_order_relaxed);
}

int Map::UsedInstanceSize() const {
#if V8_ENABLE_WEBASSEMBLY
  DCHECK(!IsWasmObjectMap(this));
#endif  // V8_ENABLE_WEBASSEMBLY
  int words = used_or_unused_instance_size_in_words();
  if (words < JSObject::kFieldsAdded) {
    // All in-object properties are used and the words is tracking the slack
    // in the property array.
    return instance_size();
  }
  return words * kTaggedSize;
}

void Map::SetInObjectUnusedPropertyFields(int value) {
  static_assert(JSObject::kFieldsAdded == JSObject::kHeaderSize / kTaggedSize);
  if (!IsJSObjectMap(this)) {
    CHECK_EQ(0, value);
    set_used_or_unused_instance_size_in_words(0);
    return;
  }
  CHECK_LE(0, value);
  DCHECK_LE(value, GetInObjectProperties());
  int used_inobject_properties = GetInObjectProperties() - value;
  set_used_or_unused_instance_size_in_words(
      GetInObjectPropertyOffset(used_inobject_properties) / kTaggedSize);
  DCHECK_EQ(value, UnusedPropertyFields());
}

void Map::SetOutOfObjectUnusedPropertyFields(int value) {
  static_assert(JSObject::kFieldsAdded == JSObject::kHeaderSize / kTaggedSize);
  CHECK_LT(static_cast<unsigned>(value), JSObject::kFieldsAdded);
  // For out of object properties "used_instance_size_in_words" byte encodes
  // the slack in the property array.
  set_used_or_unused_instance_size_in_words(value);
  DCHECK_EQ(value, UnusedPropertyFields());
}

void Map::CopyUnusedPropertyFields(Tagged<Map> map) {
  set_used_or_unused_instance_size_in_words(
      map->used_or_unused_instance_size_in_words());
  DCHECK_EQ(UnusedPropertyFields(), map->UnusedPropertyFields());
}

void Map::CopyUnusedPropertyFieldsAdjustedForInstanceSize(Tagged<Map> map) {
  int value = map->used_or_unused_instance_size_in_words();
  if (value >= JSObject::kFieldsAdded) {
    // Unused in-object fields. Adjust the offset from the object’s start
    // so it matches the distance to the object’s end.
    value += instance_size_in_words() - map->instance_size_in_words();
  }
  set_used_or_unused_instance_size_in_words(value);
  DCHECK_EQ(UnusedPropertyFields(), map->UnusedPropertyFields());
}

void Map::AccountAddedPropertyField() {
  // Update used instance size and unused property fields number.
  static_assert(JSObject::kFieldsAdded == JSObject::kHeaderSize / kTaggedSize);
#ifdef DEBUG
  int new_unused = UnusedPropertyFields() - 1;
  if (new_unused < 0) new_unused += JSObject::kFieldsAdded;
#endif
  int value = used_or_unused_instance_size_in_words();
  if (value >= JSObject::kFieldsAdded) {
    if (value == instance_size_in_words()) {
      AccountAddedOutOfObjectPropertyField(0);
    } else {
      // The property is added in-object, so simply increment the counter.
      set_used_or_unused_instance_size_in_words(value + 1);
    }
  } else {
    AccountAddedOutOfObjectPropertyField(value);
  }
  DCHECK_EQ(new_unused, UnusedPropertyFields());
}

void Map::AccountAddedOutOfObjectPropertyField(int unused_in_property_array) {
  unused_in_property_array--;
  if (unused_in_property_array < 0) {
    unused_in_property_array += JSObject::kFieldsAdded;
  }
  CHECK_LT(static_cast<unsigned>(unused_in_property_array),
           JSObject::kFieldsAdded);
  set_used_or_unused_instance_size_in_words(unused_in_property_array);
  DCHECK_EQ(unused_in_property_array, UnusedPropertyFields());
}

#if V8_ENABLE_WEBASSEMBLY
uint8_t Map::WasmByte1() const {
  DCHECK(IsWasmObjectMap(this));
  return inobject_properties_start_or_constructor_function_index();
}

uint8_t Map::WasmByte2() const {
  DCHECK(IsWasmObjectMap(this));
  return used_or_unused_instance_size_in_words();
}

void Map::SetWasmByte1(uint8_t value) {
  CHECK(IsWasmObjectMap(this));
  set_inobject_properties_start_or_constructor_function_index(value);
}

void Map::SetWasmByte2(uint8_t value) {
  CHECK(IsWasmObjectMap(this));
  set_used_or_unused_instance_size_in_words(value);
}
#endif  // V8_ENABLE_WEBASSEMBLY

uint8_t Map::bit_field() const {
  // TODO(solanes, v8:7790, v8:11353): Make this non-atomic when TSAN sees the
  // map's store synchronization.
  return relaxed_bit_field();
}

void Map::set_bit_field(uint8_t value) {
  // TODO(solanes, v8:7790, v8:11353): Make this non-atomic when TSAN sees the
  // map's store synchronization.
  set_relaxed_bit_field(value);
}

uint8_t Map::relaxed_bit_field() const {
  return bit_field_.load(std::memory_order_relaxed);
}

void Map::set_relaxed_bit_field(uint8_t value) {
  bit_field_.store(value, std::memory_order_relaxed);
}

uint8_t Map::bit_field2() const { return bit_field2_; }

void Map::set_bit_field2(uint8_t value) { bit_field2_ = value; }

uint32_t Map::bit_field3() const {
  // TODO(solanes, v8:7790, v8:11353): Make this and the setter non-atomic
  // when TSAN sees the map's store synchronization.
  return relaxed_bit_field3();
}

void Map::set_bit_field3(uint32_t value) {
  // TODO(solanes, v8:7790, v8:11353): Make this non-atomic when TSAN sees the
  // map's store synchronization.
  set_relaxed_bit_field3(value);
}

uint32_t Map::relaxed_bit_field3() const {
  return bit_field3_.load(std::memory_order_relaxed);
}

void Map::set_relaxed_bit_field3(uint32_t value) {
  bit_field3_.store(value, std::memory_order_relaxed);
}

uint32_t Map::release_acquire_bit_field3() const {
  return bit_field3_.load(std::memory_order_acquire);
}

void Map::set_release_acquire_bit_field3(uint32_t value) {
  bit_field3_.store(value, std::memory_order_release);
}

bool Map::is_abandoned_prototype_map() const {
  return is_prototype_map() && !owns_descriptors();
}

bool Map::should_be_fast_prototype_map() const {
  DCHECK(is_prototype_map());
  if (!has_prototype_info()) return false;
  return Cast<PrototypeInfo>(prototype_info())->should_be_fast_map();
}

bool Map::has_prototype_info() const {
  DCHECK(is_prototype_map());
  return IsPrototypeInfo(prototype_info());
}

bool Map::TryGetPrototypeInfo(Tagged<PrototypeInfo>* result) const {
#if V8_ENABLE_WEBASSEMBLY
  DCHECK(is_prototype_map() || IsWasmObjectMap(this));
#else
  DCHECK(is_prototype_map());
#endif  // V8_ENABLE_WEBASSEMBLY
  Tagged<Object> maybe_proto_info = prototype_info();
  if (!IsPrototypeInfo(maybe_proto_info)) return false;
  *result = Cast<PrototypeInfo>(maybe_proto_info);
  return true;
}

bool Map::TryGetPrototypeSharedClosureInfo(
    Tagged<PrototypeSharedClosureInfo>* result) const {
  if (!is_prototype_map()) return false;

  if (Tagged<PrototypeInfo> proto_info; TryGetPrototypeInfo(&proto_info)) {
    if (Tagged<Object> maybe_proto_shared_closure_info =
            proto_info->prototype_shared_closure_info();
        TryCast(maybe_proto_shared_closure_info, result)) {
      return true;
    }
  } else if (Tagged<Object> maybe_proto_shared_closure_info = prototype_info();
             TryCast(maybe_proto_shared_closure_info, result)) {
    return true;
  }

  return false;
}

void Map::SetPrototypeSharedClosureInfo(
    Tagged<PrototypeSharedClosureInfo> closure_infos) {
  DCHECK(is_prototype_map());
  if (Tagged<PrototypeInfo> proto_info; TryGetPrototypeInfo(&proto_info)) {
    proto_info->set_prototype_shared_closure_info(closure_infos);
  } else {
    this->set_prototype_info(closure_infos, kReleaseStore);
  }
}

// static
bool Map::TryGetValidityCellHolderMap(
    Tagged<Map> map, Isolate* isolate,
    Tagged<Map>* out_validity_cell_holder_map) {
  if (map->is_prototype_map()) {
    // For prototype maps we can use their validity cell for guarding changes.
    *out_validity_cell_holder_map = map;
    return true;
  }
  // For non-prototype maps we use prototype's map's validity cell.
  Tagged<Object> maybe_prototype =
      map->GetPrototypeChainRootMap(isolate)->prototype();

  if (!IsAnyObjectThatCanBeTrackedAsPrototype(maybe_prototype)) {
    return false;
  }
  *out_validity_cell_holder_map = Cast<JSReceiver>(maybe_prototype)->map();
  return true;
}

void Map::set_elements_kind(ElementsKind elements_kind) {
  CHECK_LT(static_cast<int>(elements_kind), kElementsKindCount);
  set_bit_field2(
      Map::Bits2::ElementsKindBits::update(bit_field2(), elements_kind));
}

ElementsKind Map::elements_kind() const {
  return Map::Bits2::ElementsKindBits::decode(bit_field2());
}

bool Map::has_fast_smi_elements() const {
  return IsSmiElementsKind(elements_kind());
}

bool Map::has_fast_object_elements() const {
  return IsObjectElementsKind(elements_kind());
}

bool Map::has_fast_smi_or_object_elements() const {
  return IsSmiOrObjectElementsKind(elements_kind());
}

bool Map::has_fast_double_elements() const {
  return IsDoubleElementsKind(elements_kind());
}

bool Map::has_fast_elements() const {
  return IsFastElementsKind(elements_kind());
}

bool Map::has_fast_packed_elements() const {
  return IsFastPackedElementsKind(elements_kind());
}

bool Map::has_sloppy_arguments_elements() const {
  return IsSloppyArgumentsElementsKind(elements_kind());
}

bool Map::has_fast_sloppy_arguments_elements() const {
  return elements_kind() == FAST_SLOPPY_ARGUMENTS_ELEMENTS;
}

bool Map::has_fast_string_wrapper_elements() const {
  return elements_kind() == FAST_STRING_WRAPPER_ELEMENTS;
}

bool Map::has_typed_array_or_rab_gsab_typed_array_elements() const {
  return IsTypedArrayOrRabGsabTypedArrayElementsKind(elements_kind());
}

bool Map::has_any_typed_array_or_wasm_array_elements() const {
  ElementsKind kind = elements_kind();
  return IsTypedArrayOrRabGsabTypedArrayElementsKind(kind) ||
#if V8_ENABLE_WEBASSEMBLY
         IsWasmArrayElementsKind(kind) ||
#endif  // V8_ENABLE_WEBASSEMBLY
         false;
}

bool Map::has_dictionary_elements() const {
  return IsDictionaryElementsKind(elements_kind());
}

bool Map::has_any_nonextensible_elements() const {
  return IsAnyNonextensibleElementsKind(elements_kind());
}

bool Map::has_nonextensible_elements() const {
  return IsNonextensibleElementsKind(elements_kind());
}

bool Map::has_sealed_elements() const {
  return IsSealedElementsKind(elements_kind());
}

bool Map::has_frozen_elements() const {
  return IsFrozenElementsKind(elements_kind());
}

bool Map::has_shared_array_elements() const {
  return IsSharedArrayElementsKind(elements_kind());
}

void Map::set_is_dictionary_map(bool value) {
  uint32_t new_bit_field3 =
      Bits3::IsDictionaryMapBit::update(bit_field3(), value);
  new_bit_field3 = Bits3::IsUnstableBit::update(new_bit_field3, value);
  set_bit_field3(new_bit_field3);
}

bool Map::is_dictionary_map() const {
  return Bits3::IsDictionaryMapBit::decode(relaxed_bit_field3());
}

void Map::mark_unstable() {
  set_release_acquire_bit_field3(
      Bits3::IsUnstableBit::update(bit_field3(), true));
}

bool Map::is_stable() const {
  return !Bits3::IsUnstableBit::decode(release_acquire_bit_field3());
}

bool Map::CanBeDeprecated() const {
  for (InternalIndex i : IterateOwnDescriptors()) {
    PropertyDetails details = instance_descriptors(kAcquireLoad)->GetDetails(i);
    if (details.representation().MightCauseMapDeprecation()) return true;
    if (details.kind() == PropertyKind::kData &&
        details.location() == PropertyLocation::kDescriptor) {
      return true;
    }
  }
  return false;
}

void Map::NotifyLeafMapLayoutChange(Isolate* isolate) {
  if (is_stable()) {
    mark_unstable();
    DependentCode::DeoptimizeDependencyGroups<Map>(
        isolate, this, DependentCode::kPrototypeCheckGroup);
  }
}

bool Map::CanTransition() const {
  // Only JSObject and subtypes have map transitions and back pointers.
  const InstanceType type = instance_type();
  // JSExternalObjects are non-extensible and thus the map is allocated in
  // read only sapce.
  DCHECK_IMPLIES(InstanceTypeChecker::IsMaybeReadOnlyJSObject(type),
                 HeapLayout::InReadOnlySpace(this));
  // Shared JS objects have fixed shapes and do not transition. Their maps are
  // either in shared space or RO space.
  DCHECK_IMPLIES(InstanceTypeChecker::IsAlwaysSharedSpaceJSObject(type),
                 HeapLayout::InAnySharedSpace(this));
  return InstanceTypeChecker::IsJSObject(type) &&
         !InstanceTypeChecker::IsMaybeReadOnlyJSObject(type) &&
         !InstanceTypeChecker::IsAlwaysSharedSpaceJSObject(type);
}

bool IsBooleanMap(Tagged<Map> map) {
  return map == GetReadOnlyRoots().boolean_map();
}

bool IsNullMap(Tagged<Map> map) { return map == GetReadOnlyRoots().null_map(); }

bool IsUndefinedMap(Tagged<Map> map) {
  return map == GetReadOnlyRoots().undefined_map();
}

bool IsNullOrUndefinedMap(Tagged<Map> map) {
  auto roots = GetReadOnlyRoots();
  return map == roots.null_map() || map == roots.undefined_map();
}

bool IsPrimitiveMap(Tagged<Map> map) {
  return map->instance_type() <= LAST_PRIMITIVE_HEAP_OBJECT_TYPE;
}

void Map::UpdateDescriptors(Isolate* isolate,
                            Tagged<DescriptorArray> descriptors,
                            int number_of_own_descriptors) {
  SetInstanceDescriptors(isolate, descriptors, number_of_own_descriptors);
}

void Map::InitializeDescriptors(Isolate* isolate,
                                Tagged<DescriptorArray> descriptors) {
  SetInstanceDescriptors(isolate, descriptors,
                         descriptors->number_of_descriptors());
}

void Map::clear_padding() {
#if TAGGED_SIZE_8_BYTES
  optional_padding_ = 0;
#endif
}

void Map::AppendDescriptor(Isolate* isolate, Descriptor* desc) {
  Tagged<DescriptorArray> descriptors = instance_descriptors(isolate);
  int number_of_own_descriptors = NumberOfOwnDescriptors();
  DCHECK(descriptors->number_of_descriptors() == number_of_own_descriptors);
  {
    // The following two operations need to happen before the marking write
    // barrier.
    descriptors->Append(desc);
    SetNumberOfOwnDescriptors(number_of_own_descriptors + 1);
#ifndef V8_DISABLE_WRITE_BARRIERS
    WriteBarrier::ForDescriptorArray(descriptors,
                                     number_of_own_descriptors + 1);
#endif
  }
  // Properly mark the map if the {desc} is an "interesting symbol".
  if (desc->GetKey()->IsInteresting(isolate)) {
    set_may_have_interesting_properties(true);
  }
  PropertyDetails details = desc->GetDetails();
  if (details.location() == PropertyLocation::kField) {
    DCHECK_GT(UnusedPropertyFields(), 0);
    AccountAddedPropertyField();
#ifdef DEBUG
    // Verify after accounting the added field, to make sure we have the
    // expected UsedInstanceSize.
    VerifyPropertyDetailsInObjectBits(details);
#endif
  }

// This function does not support appending double field descriptors and
// it should never try to (otherwise, layout descriptor must be updated too).
#ifdef DEBUG
  DCHECK(details.location() != PropertyLocation::kField ||
         !details.representation().IsDouble());
#endif
}

// static
bool Map::ConcurrentIsHeapObjectWithMap(PtrComprCageBase cage_base,
                                        Tagged<Object> object,
                                        Tagged<Map> meta_map) {
  if (!IsHeapObject(object)) return false;
  Tagged<HeapObject> heap_object = Cast<HeapObject>(object);
  return heap_object->map(cage_base) == meta_map;
}

Tagged<HeapObject> Map::GetBackPointer() const {
  Tagged<Map> back_pointer;
  if (TryGetBackPointer(GetPtrComprCageBase(this), &back_pointer)) {
    return back_pointer;
  }
  return GetReadOnlyRoots().undefined_value();
}

bool Map::TryGetBackPointer(PtrComprCageBase cage_base,
                            Tagged<Map>* back_pointer) const {
  Tagged<Object> object = constructor_or_back_pointer(cage_base, kRelaxedLoad);
  // We don't expect maps from another native context in the transition tree,
  // so just compare object's map against current map's meta map.
  Tagged<Map> meta_map = map();
  if (ConcurrentIsHeapObjectWithMap(cage_base, object, meta_map)) {
    DCHECK(IsMap(object));
    // Sanity check - only contextful maps can transition.
    DCHECK(IsNativeContext(meta_map->native_context_or_null()));
    *back_pointer = Cast<Map>(object);
    return true;
  }
  // If it was a map that'd mean that there are maps from different native
  // contexts in the transition tree.
  DCHECK(!IsMap(object));
  return false;
}

void Map::SetBackPointer(Tagged<HeapObject> value, WriteBarrierMode mode) {
  CHECK_GE(instance_type(), FIRST_JS_RECEIVER_TYPE);
  CHECK(IsMap(value));
  CHECK(IsUndefined(GetBackPointer()));
  CHECK_EQ(Cast<Map>(value)->GetConstructorRaw(),
           constructor_or_back_pointer());
  set_constructor_or_back_pointer(value, mode);
}

// static
Tagged<Map> Map::GetMapFor(ReadOnlyRoots roots, InstanceType type) {
  RootIndex map_idx = TryGetMapRootIdxFor(type).value();
  return UncheckedCast<Map>(roots.object_at(map_idx));
}

// static
Tagged<Map> Map::ElementsTransitionMap(Isolate* isolate,
                                       ConcurrencyMode cmode) {
  return TransitionsAccessor(isolate, this, IsConcurrent(cmode))
      .SearchSpecial(ReadOnlyRoots(isolate).elements_transition_symbol());
}

#if V8_ENABLE_WEBASSEMBLY
Tagged<DependentCode> Map::dependent_code() const {
  Tagged<Object> value = dependent_code_.load();
  if (!IsDependentCode(value)) {
    DCHECK(IsWasmStructMap(this));
    return DependentCode::empty_dependent_code(GetReadOnlyRoots());
  }
  return Cast<DependentCode>(value);
}
void Map::set_dependent_code(Tagged<DependentCode> value,
                             WriteBarrierMode mode) {
  // Only the Factory may call this for Wasm object maps, when default-
  // initializing them. Use the WB mode as a sentinel for that situation.
  DCHECK(mode == SKIP_WRITE_BARRIER || !IsWasmObjectMap(this));
  dependent_code_.store(this, value, mode);
}
Tagged<Map> Map::immediate_supertype_map() const {
  DCHECK(IsWasmObjectMap(this));
  // dependent_code_ slot is reused for the supertype map on Wasm maps.
  return Cast<Map>(dependent_code_.load());
}
void Map::set_immediate_supertype_map(Tagged<Map> value,
                                      WriteBarrierMode mode) {
  DCHECK(IsWasmObjectMap(this));
  dependent_code_.store(this, value, mode);
}
#else   // V8_ENABLE_WEBASSEMBLY
Tagged<DependentCode> Map::dependent_code() const {
  return dependent_code_.load();
}
void Map::set_dependent_code(Tagged<DependentCode> value,
                             WriteBarrierMode mode) {
  dependent_code_.store(this, value, mode);
}
#endif  // V8_ENABLE_WEBASSEMBLY

Tagged<UnionOf<Smi, Cell>> Map::prototype_validity_cell(RelaxedLoadTag) const {
  return prototype_validity_cell_.Relaxed_Load();
}
void Map::set_prototype_validity_cell(Tagged<UnionOf<Smi, Cell>> value,
                                      RelaxedStoreTag, WriteBarrierMode mode) {
  prototype_validity_cell_.Relaxed_Store(this, value, mode);
}

Tagged<Object> Map::constructor_or_back_pointer() const {
  Tagged<Object> value = constructor_or_back_pointer_or_native_context_.load();
  DCHECK(!IsContextMap(this));
  return value;
}
void Map::set_constructor_or_back_pointer(Tagged<Object> value,
                                          WriteBarrierMode mode) {
  DCHECK(IsNull(value) || !IsContextMap(this));
  constructor_or_back_pointer_or_native_context_.store(this, value, mode);
}
Tagged<Object> Map::constructor_or_back_pointer(RelaxedLoadTag) const {
  Tagged<Object> value =
      constructor_or_back_pointer_or_native_context_.Relaxed_Load();
  DCHECK(!IsContextMap(this));
  return value;
}
void Map::set_constructor_or_back_pointer(Tagged<Object> value, RelaxedStoreTag,
                                          WriteBarrierMode mode) {
  DCHECK(IsNull(value) || !IsContextMap(this));
  constructor_or_back_pointer_or_native_context_.Relaxed_Store(this, value,
                                                               mode);
}

Tagged<NativeContext> Map::native_context() const {
  DCHECK(IsContextMap(this) || IsMapMap(this));
  return Cast<NativeContext>(
      constructor_or_back_pointer_or_native_context_.load());
}
void Map::set_native_context(Tagged<NativeContext> value,
                             WriteBarrierMode mode) {
  DCHECK(IsContextMap(this) || IsMapMap(this));
  constructor_or_back_pointer_or_native_context_.store(this, value, mode);
}

Tagged<Object> Map::native_context_or_null() const {
  Tagged<Object> value = constructor_or_back_pointer_or_native_context_.load();
  DCHECK((IsNull(value) || IsNativeContext(value)) &&
         (IsContextMap(this) || IsMapMap(this)));
  return value;
}
void Map::set_native_context_or_null(Tagged<Object> value,
                                     WriteBarrierMode mode) {
  DCHECK((IsNull(value) || IsNativeContext(value)) &&
         (IsContextMap(this) || IsMapMap(this)));
  constructor_or_back_pointer_or_native_context_.store(this, value, mode);
}

// Unlike native_context_or_null() this getter allows the value to be
// equal to Smi::uninitialized_deserialization_value().
Tagged<Object> Map::raw_native_context_or_null() const {
  Tagged<Object> value = constructor_or_back_pointer_or_native_context_.load();
  DCHECK(IsNull(value) || IsNativeContext(value) ||
         value == Smi::uninitialized_deserialization_value());
  DCHECK(IsContextMap(this) || IsMapMap(this));
  return value;
}

#if V8_ENABLE_WEBASSEMBLY
Tagged<WasmTypeInfo> Map::wasm_type_info() const {
  DCHECK(IsWasmStructMap(this) || IsWasmArrayMap(this) ||
         IsWasmFuncRefMap(this) || IsWasmContinuationObjectMap(this));
  return Cast<WasmTypeInfo>(
      constructor_or_back_pointer_or_native_context_.load());
}
void Map::set_wasm_type_info(Tagged<WasmTypeInfo> value,
                             WriteBarrierMode mode) {
  DCHECK(IsWasmStructMap(this) || IsWasmArrayMap(this) ||
         IsWasmFuncRefMap(this) || IsWasmContinuationObjectMap(this));
  constructor_or_back_pointer_or_native_context_.store(this, value, mode);
}
#endif  // V8_ENABLE_WEBASSEMBLY

bool Map::IsPrototypeValidityCellValid() const {
  Tagged<Object> validity_cell = prototype_validity_cell(kRelaxedLoad);
  if (validity_cell == Map::kNoValidityCellSentinel) {
    // Smi validity cells should always be considered valid.
    return true;
  }
  return Cast<Cell>(validity_cell)->maybe_value() !=
         Map::kPrototypeChainInvalid;
}

bool Map::BelongsToSameNativeContextAs(Tagged<Map> other_map) const {
  Tagged<Map> this_meta_map = map();
  // If the meta map is contextless (as in case of remote object's meta map)
  // we can't be sure the maps belong to the same context.
  if (this_meta_map == GetReadOnlyRoots().meta_map()) return false;
  DCHECK(IsNativeContext(this_meta_map->native_context_or_null()));
  return this_meta_map == other_map->map();
}

bool Map::BelongsToSameNativeContextAs(Tagged<Context> context) const {
  Tagged<Map> context_meta_map = context->map()->map();
  Tagged<Map> this_meta_map = map();
  DCHECK_NE(context_meta_map, GetReadOnlyRoots().meta_map());
  return this_meta_map == context_meta_map;
}

Tagged<Object> Map::GetConstructorRaw() const {
  PtrComprCageBase cage_base = GetPtrComprCageBase(this);
  Tagged<Object> maybe_constructor = constructor_or_back_pointer();
  // Follow any back pointers.
  // We don't expect maps from another native context in the transition tree,
  // so just compare object's map against current map's meta map.
  Tagged<Map> meta_map = map();
  while (
      ConcurrentIsHeapObjectWithMap(cage_base, maybe_constructor, meta_map)) {
    DCHECK(IsMap(maybe_constructor));
    // Sanity check - only contextful maps can transition.
    DCHECK(IsNativeContext(meta_map->native_context_or_null()));
    maybe_constructor =
        Cast<Map>(maybe_constructor)->constructor_or_back_pointer();
  }
  // If it was a map that'd mean that there are maps from different native
  // contexts in the transition tree.
  DCHECK(!IsMap(maybe_constructor));
  return maybe_constructor;
}

Tagged<Object> Map::GetNonInstancePrototype() const {
  DCHECK(has_non_instance_prototype());
  Tagged<Object> raw_constructor = GetConstructorRaw();
  CHECK(IsTuple2(raw_constructor));
  // Get prototype from the {constructor, non-instance_prototype} tuple.
  Tagged<Tuple2> non_instance_prototype_constructor_tuple =
      Cast<Tuple2>(raw_constructor);
  Tagged<Object> result = non_instance_prototype_constructor_tuple->value2();
  DCHECK(!IsJSReceiver(result));
  DCHECK(!IsFunctionTemplateInfo(result));
  return result;
}

Tagged<Object> Map::GetConstructor() const {
  Tagged<Object> maybe_constructor = GetConstructorRaw();
  if (IsTuple2(maybe_constructor)) {
    // Get constructor from the {constructor, non-instance_prototype} tuple.
    maybe_constructor = Cast<Tuple2>(maybe_constructor)->value1();
  }
  return maybe_constructor;
}

Tagged<Object> Map::TryGetConstructor(PtrComprCageBase cage_base,
                                      int max_steps) {
  Tagged<Object> maybe_constructor = constructor_or_back_pointer(cage_base);
  // Follow any back pointers.
  while (IsMap(maybe_constructor, cage_base)) {
    if (max_steps-- == 0) return Smi::FromInt(0);
    maybe_constructor =
        Cast<Map>(maybe_constructor)->constructor_or_back_pointer(cage_base);
  }
  if (IsTuple2(maybe_constructor)) {
    // Get constructor from the {constructor, non-instance_prototype} tuple.
    maybe_constructor = Cast<Tuple2>(maybe_constructor)->value1();
  }
  return maybe_constructor;
}

Tagged<FunctionTemplateInfo> Map::GetFunctionTemplateInfo() const {
  Tagged<Object> constructor = GetConstructor();
  if (IsJSFunction(constructor)) {
    Tagged<SharedFunctionInfo> sfi = Cast<JSFunction>(constructor)->shared();
    DCHECK(sfi->IsApiFunction());
    return sfi->api_func_data();
  }
  DCHECK(IsFunctionTemplateInfo(constructor));
  return Cast<FunctionTemplateInfo>(constructor);
}

void Map::SetConstructor(Tagged<Object> constructor, WriteBarrierMode mode) {
  // Never overwrite a back pointer with a constructor.
  CHECK(!IsMap(constructor_or_back_pointer()));
  // Constructor field must contain {constructor, non-instance_prototype} tuple
  // for maps with non-instance prototype.
  DCHECK_EQ(has_non_instance_prototype(), IsTuple2(constructor));
  set_constructor_or_back_pointer(constructor, mode);
}

Handle<Map> Map::CopyInitialMap(Isolate* isolate, DirectHandle<Map> map) {
  return CopyInitialMap(isolate, map, map->instance_size(),
                        map->GetInObjectProperties(),
                        map->UnusedPropertyFields());
}

bool Map::IsInobjectSlackTrackingInProgress() const {
  return construction_counter() != Map::kNoSlackTracking;
}

void Map::InobjectSlackTrackingStep(Isolate* isolate) {
  DisallowGarbageCollection no_gc;
  // Slack tracking should only be performed on an initial map.
  DCHECK(IsUndefined(GetBackPointer()));
  if (!this->IsInobjectSlackTrackingInProgress()) return;
  int counter = construction_counter();
  set_construction_counter(counter - 1);
  if (counter == kSlackTrackingCounterEnd) {
    MapUpdater::CompleteInobjectSlackTracking(isolate, this);
  }
}

int Map::SlackForArraySize(int old_size, int size_limit) {
  const int max_slack = size_limit - old_size;
  CHECK_LE(0, max_slack);
  if (old_size < 4) {
    DCHECK_LE(1, max_slack);
    return 1;
  }
  return std::min(max_slack, old_size / 4);
}

int Map::InstanceSizeFromSlack(int slack) const {
  return instance_size() - slack * kTaggedSize;
}

constexpr int ExtendedMapSizeForKind(ExtendedMapKind kind) {
  switch (kind) {
    case ExtendedMapKind::kJSInterceptorMap:
      return sizeof(JSInterceptorMap);
  }
  UNREACHABLE();
}

uint8_t ExtendedMap::relaxed_bit_field_ex() const {
  return bit_field_ex_.load(std::memory_order_relaxed);
}

void ExtendedMap::set_relaxed_bit_field_ex(uint8_t value) {
  bit_field_ex_.store(value, std::memory_order_relaxed);
}

uint8_t ExtendedMap::bit_field_ex() const {
  // TODO(solanes, v8:7790, v8:11353): Make this non-atomic when TSAN sees the
  // map's store synchronization.
  return relaxed_bit_field_ex();
}

void ExtendedMap::set_bit_field_ex(uint8_t value) {
  // TODO(solanes, v8:7790, v8:11353): Make this non-atomic when TSAN sees the
  // map's store synchronization.
  set_relaxed_bit_field_ex(value);
}

ExtendedMapKind ExtendedMap::map_kind() const {
  return BitsEx::MapKindBits::decode(relaxed_bit_field_ex());
}

uint8_t ExtendedMap::map_size_in_words() const {
  return BitsEx::MapSizeInWordsBits::decode(relaxed_bit_field_ex());
}

int ExtendedMap::map_size() const {
  return map_size_in_words() << kTaggedSizeLog2;
}

void ExtendedMap::set_map_kind_and_size(ExtendedMapKind kind,
                                        int size_in_bytes) {
  DCHECK(IsAligned(size_in_bytes, kTaggedSize));
  int size_in_words = size_in_bytes >> kTaggedSizeLog2;
  CHECK_LE(static_cast<unsigned>(size_in_words), kMaxUInt8);

  uint8_t field =
      BitsEx::MapKindBits::encode(kind) |
      BitsEx::MapSizeInWordsBits::encode(static_cast<uint8_t>(size_in_words));
  set_relaxed_bit_field_ex(field);
}

Tagged<InterceptorInfo> JSInterceptorMap::named_interceptor() const {
  return named_interceptor_.load();
}
void JSInterceptorMap::set_named_interceptor(
    Tagged<InterceptorInfo> interceptor_info, WriteBarrierMode mode) {
  DCHECK(interceptor_info->is_named());
  named_interceptor_.store(this, interceptor_info, mode);
}

Tagged<InterceptorInfo> JSInterceptorMap::indexed_interceptor() const {
  return indexed_interceptor_.load();
}
void JSInterceptorMap::set_indexed_interceptor(
    Tagged<InterceptorInfo> interceptor_info, WriteBarrierMode mode) {
  DCHECK(!interceptor_info->is_named());
  indexed_interceptor_.store(this, interceptor_info, mode);
}

void JSInterceptorMap::clear_extended_padding() {
  memset(extended_padding_, 0, sizeof(extended_padding_));
}

int NormalizedMapCache::GetIndex(Isolate* isolate, Tagged<Map> map,
                                 Tagged<HeapObject> prototype) {
  DisallowGarbageCollection no_gc;
  return map->Hash(isolate, prototype) % NormalizedMapCache::kEntries;
}

DEF_HEAP_OBJECT_PREDICATE(HeapObject, IsNormalizedMapCache) {
  if (!IsWeakFixedArray(obj, cage_base)) return false;
  if (Cast<WeakFixedArray>(obj)->ulength().value() !=
      NormalizedMapCache::kEntries) {
    return false;
  }
  return true;
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_MAP_INL_H_
