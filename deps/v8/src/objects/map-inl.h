// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_MAP_INL_H_
#define V8_OBJECTS_MAP_INL_H_

#include "src/objects/map.h"

#include "src/field-type.h"
#include "src/objects-inl.h"
#include "src/objects/descriptor-array.h"
#include "src/objects/shared-function-info.h"
#include "src/property.h"
#include "src/transitions.h"

// For pulling in heap/incremental-marking.h which is needed by
// ACCESSORS_CHECKED.
#include "src/heap/heap-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

CAST_ACCESSOR(Map)

ACCESSORS(Map, instance_descriptors, DescriptorArray, kDescriptorsOffset)
ACCESSORS_CHECKED(Map, layout_descriptor, LayoutDescriptor,
                  kLayoutDescriptorOffset, FLAG_unbox_double_fields)
WEAK_ACCESSORS(Map, raw_transitions, kTransitionsOrPrototypeInfoOffset)

// |bit_field| fields.
BIT_FIELD_ACCESSORS(Map, bit_field, has_non_instance_prototype,
                    Map::HasNonInstancePrototypeBit)
BIT_FIELD_ACCESSORS(Map, bit_field, is_callable, Map::IsCallableBit)
BIT_FIELD_ACCESSORS(Map, bit_field, has_named_interceptor,
                    Map::HasNamedInterceptorBit)
BIT_FIELD_ACCESSORS(Map, bit_field, has_indexed_interceptor,
                    Map::HasIndexedInterceptorBit)
BIT_FIELD_ACCESSORS(Map, bit_field, is_undetectable, Map::IsUndetectableBit)
BIT_FIELD_ACCESSORS(Map, bit_field, is_access_check_needed,
                    Map::IsAccessCheckNeededBit)
BIT_FIELD_ACCESSORS(Map, bit_field, is_constructor, Map::IsConstructorBit)
BIT_FIELD_ACCESSORS(Map, bit_field, has_prototype_slot,
                    Map::HasPrototypeSlotBit)

// |bit_field2| fields.
BIT_FIELD_ACCESSORS(Map, bit_field2, is_extensible, Map::IsExtensibleBit)
BIT_FIELD_ACCESSORS(Map, bit_field2, is_prototype_map, Map::IsPrototypeMapBit)
BIT_FIELD_ACCESSORS(Map, bit_field2, is_in_retained_map_list,
                    Map::IsInRetainedMapListBit)

// |bit_field3| fields.
BIT_FIELD_ACCESSORS(Map, bit_field3, owns_descriptors, Map::OwnsDescriptorsBit)
BIT_FIELD_ACCESSORS(Map, bit_field3, has_hidden_prototype,
                    Map::HasHiddenPrototypeBit)
BIT_FIELD_ACCESSORS(Map, bit_field3, is_deprecated, Map::IsDeprecatedBit)
BIT_FIELD_ACCESSORS(Map, bit_field3, is_migration_target,
                    Map::IsMigrationTargetBit)
BIT_FIELD_ACCESSORS(Map, bit_field3, is_immutable_proto,
                    Map::IsImmutablePrototypeBit)
BIT_FIELD_ACCESSORS(Map, bit_field3, new_target_is_base,
                    Map::NewTargetIsBaseBit)
BIT_FIELD_ACCESSORS(Map, bit_field3, may_have_interesting_symbols,
                    Map::MayHaveInterestingSymbolsBit)
BIT_FIELD_ACCESSORS(Map, bit_field3, construction_counter,
                    Map::ConstructionCounterBits)

TYPE_CHECKER(Map, MAP_TYPE)

InterceptorInfo* Map::GetNamedInterceptor() {
  DCHECK(has_named_interceptor());
  FunctionTemplateInfo* info = GetFunctionTemplateInfo();
  return InterceptorInfo::cast(info->named_property_handler());
}

InterceptorInfo* Map::GetIndexedInterceptor() {
  DCHECK(has_indexed_interceptor());
  FunctionTemplateInfo* info = GetFunctionTemplateInfo();
  return InterceptorInfo::cast(info->indexed_property_handler());
}

bool Map::IsInplaceGeneralizableField(PropertyConstness constness,
                                      Representation representation,
                                      FieldType* field_type) {
  if (FLAG_track_constant_fields && FLAG_modify_map_inplace &&
      (constness == kConst)) {
    // kConst -> kMutable field generalization may happen in-place.
    return true;
  }
  if (representation.IsHeapObject() && !field_type->IsAny()) {
    return true;
  }
  return false;
}

bool Map::CanHaveFastTransitionableElementsKind(InstanceType instance_type) {
  return instance_type == JS_ARRAY_TYPE || instance_type == JS_VALUE_TYPE ||
         instance_type == JS_ARGUMENTS_TYPE;
}

bool Map::CanHaveFastTransitionableElementsKind() const {
  return CanHaveFastTransitionableElementsKind(instance_type());
}

// static
void Map::GeneralizeIfCanHaveTransitionableFastElementsKind(
    Isolate* isolate, InstanceType instance_type, PropertyConstness* constness,
    Representation* representation, Handle<FieldType>* field_type) {
  if (CanHaveFastTransitionableElementsKind(instance_type)) {
    // We don't support propagation of field generalization through elements
    // kind transitions because they are inserted into the transition tree
    // before field transitions. In order to avoid complexity of handling
    // such a case we ensure that all maps with transitionable elements kinds
    // do not have fields that can be generalized in-place (without creation
    // of a new map).
    if (FLAG_track_constant_fields && FLAG_modify_map_inplace) {
      // The constness is either already kMutable or should become kMutable if
      // it was kConst.
      *constness = kMutable;
    }
    if (representation->IsHeapObject()) {
      // The field type is either already Any or should become Any if it was
      // something else.
      *field_type = FieldType::Any(isolate);
    }
  }
}

bool Map::IsUnboxedDoubleField(FieldIndex index) const {
  if (!FLAG_unbox_double_fields) return false;
  if (index.is_hidden_field() || !index.is_inobject()) return false;
  return !layout_descriptor()->IsTagged(index.property_index());
}

bool Map::TooManyFastProperties(StoreFromKeyed store_mode) const {
  if (UnusedPropertyFields() != 0) return false;
  if (is_prototype_map()) return false;
  int minimum = store_mode == CERTAINLY_NOT_STORE_FROM_KEYED ? 128 : 12;
  int limit = Max(minimum, GetInObjectProperties());
  int external = NumberOfFields() - GetInObjectProperties();
  return external > limit;
}

PropertyDetails Map::GetLastDescriptorDetails() const {
  return instance_descriptors()->GetDetails(LastAdded());
}

int Map::LastAdded() const {
  int number_of_own_descriptors = NumberOfOwnDescriptors();
  DCHECK_GT(number_of_own_descriptors, 0);
  return number_of_own_descriptors - 1;
}

int Map::NumberOfOwnDescriptors() const {
  return NumberOfOwnDescriptorsBits::decode(bit_field3());
}

void Map::SetNumberOfOwnDescriptors(int number) {
  DCHECK_LE(number, instance_descriptors()->number_of_descriptors());
  CHECK_LE(static_cast<unsigned>(number),
           static_cast<unsigned>(kMaxNumberOfDescriptors));
  set_bit_field3(NumberOfOwnDescriptorsBits::update(bit_field3(), number));
}

int Map::EnumLength() const { return EnumLengthBits::decode(bit_field3()); }

void Map::SetEnumLength(int length) {
  if (length != kInvalidEnumCacheSentinel) {
    DCHECK_LE(length, NumberOfOwnDescriptors());
    CHECK_LE(static_cast<unsigned>(length),
             static_cast<unsigned>(kMaxNumberOfDescriptors));
  }
  set_bit_field3(EnumLengthBits::update(bit_field3(), length));
}

FixedArrayBase* Map::GetInitialElements() const {
  FixedArrayBase* result = nullptr;
  if (has_fast_elements() || has_fast_string_wrapper_elements()) {
    result = GetHeap()->empty_fixed_array();
  } else if (has_fast_sloppy_arguments_elements()) {
    result = GetHeap()->empty_sloppy_arguments_elements();
  } else if (has_fixed_typed_array_elements()) {
    result = GetHeap()->EmptyFixedTypedArrayForMap(this);
  } else if (has_dictionary_elements()) {
    result = GetHeap()->empty_slow_element_dictionary();
  } else {
    UNREACHABLE();
  }
  DCHECK(!GetHeap()->InNewSpace(result));
  return result;
}

VisitorId Map::visitor_id() const {
  return static_cast<VisitorId>(
      RELAXED_READ_BYTE_FIELD(this, kVisitorIdOffset));
}

void Map::set_visitor_id(VisitorId id) {
  CHECK_LT(static_cast<unsigned>(id), 256);
  RELAXED_WRITE_BYTE_FIELD(this, kVisitorIdOffset, static_cast<byte>(id));
}

int Map::instance_size_in_words() const {
  return RELAXED_READ_BYTE_FIELD(this, kInstanceSizeInWordsOffset);
}

void Map::set_instance_size_in_words(int value) {
  RELAXED_WRITE_BYTE_FIELD(this, kInstanceSizeInWordsOffset,
                           static_cast<byte>(value));
}

int Map::instance_size() const {
  return instance_size_in_words() << kPointerSizeLog2;
}

void Map::set_instance_size(int value) {
  CHECK_EQ(0, value & (kPointerSize - 1));
  value >>= kPointerSizeLog2;
  CHECK_LT(static_cast<unsigned>(value), 256);
  set_instance_size_in_words(value);
}

int Map::inobject_properties_start_or_constructor_function_index() const {
  return RELAXED_READ_BYTE_FIELD(
      this, kInObjectPropertiesStartOrConstructorFunctionIndexOffset);
}

void Map::set_inobject_properties_start_or_constructor_function_index(
    int value) {
  CHECK_LT(static_cast<unsigned>(value), 256);
  RELAXED_WRITE_BYTE_FIELD(
      this, kInObjectPropertiesStartOrConstructorFunctionIndexOffset,
      static_cast<byte>(value));
}

int Map::GetInObjectPropertiesStartInWords() const {
  DCHECK(IsJSObjectMap());
  return inobject_properties_start_or_constructor_function_index();
}

void Map::SetInObjectPropertiesStartInWords(int value) {
  CHECK(IsJSObjectMap());
  set_inobject_properties_start_or_constructor_function_index(value);
}

int Map::GetInObjectProperties() const {
  DCHECK(IsJSObjectMap());
  return instance_size_in_words() - GetInObjectPropertiesStartInWords();
}

int Map::GetConstructorFunctionIndex() const {
  DCHECK(IsPrimitiveMap());
  return inobject_properties_start_or_constructor_function_index();
}

void Map::SetConstructorFunctionIndex(int value) {
  CHECK(IsPrimitiveMap());
  set_inobject_properties_start_or_constructor_function_index(value);
}

int Map::GetInObjectPropertyOffset(int index) const {
  return (GetInObjectPropertiesStartInWords() + index) * kPointerSize;
}

Handle<Map> Map::AddMissingTransitionsForTesting(
    Handle<Map> split_map, Handle<DescriptorArray> descriptors,
    Handle<LayoutDescriptor> full_layout_descriptor) {
  return AddMissingTransitions(split_map, descriptors, full_layout_descriptor);
}

InstanceType Map::instance_type() const {
  return static_cast<InstanceType>(
      READ_UINT16_FIELD(this, kInstanceTypeOffset));
}

void Map::set_instance_type(InstanceType value) {
  WRITE_UINT16_FIELD(this, kInstanceTypeOffset, value);
}

int Map::UnusedPropertyFields() const {
  int value = used_or_unused_instance_size_in_words();
  DCHECK_IMPLIES(!IsJSObjectMap(), value == 0);
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

int Map::used_or_unused_instance_size_in_words() const {
  return RELAXED_READ_BYTE_FIELD(this, kUsedOrUnusedInstanceSizeInWordsOffset);
}

void Map::set_used_or_unused_instance_size_in_words(int value) {
  CHECK_LE(static_cast<unsigned>(value), 255);
  RELAXED_WRITE_BYTE_FIELD(this, kUsedOrUnusedInstanceSizeInWordsOffset,
                           static_cast<byte>(value));
}

int Map::UsedInstanceSize() const {
  int words = used_or_unused_instance_size_in_words();
  if (words < JSObject::kFieldsAdded) {
    // All in-object properties are used and the words is tracking the slack
    // in the property array.
    return instance_size();
  }
  return words * kPointerSize;
}

void Map::SetInObjectUnusedPropertyFields(int value) {
  STATIC_ASSERT(JSObject::kFieldsAdded == JSObject::kHeaderSize / kPointerSize);
  if (!IsJSObjectMap()) {
    CHECK_EQ(0, value);
    set_used_or_unused_instance_size_in_words(0);
    DCHECK_EQ(0, UnusedPropertyFields());
    return;
  }
  CHECK_LE(0, value);
  DCHECK_LE(value, GetInObjectProperties());
  int used_inobject_properties = GetInObjectProperties() - value;
  set_used_or_unused_instance_size_in_words(
      GetInObjectPropertyOffset(used_inobject_properties) / kPointerSize);
  DCHECK_EQ(value, UnusedPropertyFields());
}

void Map::SetOutOfObjectUnusedPropertyFields(int value) {
  STATIC_ASSERT(JSObject::kFieldsAdded == JSObject::kHeaderSize / kPointerSize);
  CHECK_LT(static_cast<unsigned>(value), JSObject::kFieldsAdded);
  // For out of object properties "used_instance_size_in_words" byte encodes
  // the slack in the property array.
  set_used_or_unused_instance_size_in_words(value);
  DCHECK_EQ(value, UnusedPropertyFields());
}

void Map::CopyUnusedPropertyFields(Map* map) {
  set_used_or_unused_instance_size_in_words(
      map->used_or_unused_instance_size_in_words());
  DCHECK_EQ(UnusedPropertyFields(), map->UnusedPropertyFields());
}

void Map::AccountAddedPropertyField() {
  // Update used instance size and unused property fields number.
  STATIC_ASSERT(JSObject::kFieldsAdded == JSObject::kHeaderSize / kPointerSize);
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

byte Map::bit_field() const { return READ_BYTE_FIELD(this, kBitFieldOffset); }

void Map::set_bit_field(byte value) {
  WRITE_BYTE_FIELD(this, kBitFieldOffset, value);
}

byte Map::bit_field2() const { return READ_BYTE_FIELD(this, kBitField2Offset); }

void Map::set_bit_field2(byte value) {
  WRITE_BYTE_FIELD(this, kBitField2Offset, value);
}

bool Map::is_abandoned_prototype_map() const {
  return is_prototype_map() && !owns_descriptors();
}

bool Map::should_be_fast_prototype_map() const {
  if (!prototype_info()->IsPrototypeInfo()) return false;
  return PrototypeInfo::cast(prototype_info())->should_be_fast_map();
}

void Map::set_elements_kind(ElementsKind elements_kind) {
  CHECK_LT(static_cast<int>(elements_kind), kElementsKindCount);
  set_bit_field2(Map::ElementsKindBits::update(bit_field2(), elements_kind));
}

ElementsKind Map::elements_kind() const {
  return Map::ElementsKindBits::decode(bit_field2());
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

bool Map::has_sloppy_arguments_elements() const {
  return IsSloppyArgumentsElementsKind(elements_kind());
}

bool Map::has_fast_sloppy_arguments_elements() const {
  return elements_kind() == FAST_SLOPPY_ARGUMENTS_ELEMENTS;
}

bool Map::has_fast_string_wrapper_elements() const {
  return elements_kind() == FAST_STRING_WRAPPER_ELEMENTS;
}

bool Map::has_fixed_typed_array_elements() const {
  return IsFixedTypedArrayElementsKind(elements_kind());
}

bool Map::has_dictionary_elements() const {
  return IsDictionaryElementsKind(elements_kind());
}

void Map::set_is_dictionary_map(bool value) {
  uint32_t new_bit_field3 = IsDictionaryMapBit::update(bit_field3(), value);
  new_bit_field3 = IsUnstableBit::update(new_bit_field3, value);
  set_bit_field3(new_bit_field3);
}

bool Map::is_dictionary_map() const {
  return IsDictionaryMapBit::decode(bit_field3());
}

void Map::mark_unstable() {
  set_bit_field3(IsUnstableBit::update(bit_field3(), true));
}

bool Map::is_stable() const { return !IsUnstableBit::decode(bit_field3()); }

bool Map::CanBeDeprecated() const {
  int descriptor = LastAdded();
  for (int i = 0; i <= descriptor; i++) {
    PropertyDetails details = instance_descriptors()->GetDetails(i);
    if (details.representation().IsNone()) return true;
    if (details.representation().IsSmi()) return true;
    if (details.representation().IsDouble()) return true;
    if (details.representation().IsHeapObject()) return true;
    if (details.kind() == kData && details.location() == kDescriptor) {
      return true;
    }
  }
  return false;
}

void Map::NotifyLeafMapLayoutChange() {
  if (is_stable()) {
    mark_unstable();
    dependent_code()->DeoptimizeDependentCodeGroup(
        GetIsolate(), DependentCode::kPrototypeCheckGroup);
  }
}

bool Map::CanTransition() const {
  // Only JSObject and subtypes have map transitions and back pointers.
  STATIC_ASSERT(LAST_TYPE == LAST_JS_OBJECT_TYPE);
  return instance_type() >= FIRST_JS_OBJECT_TYPE;
}

bool Map::IsBooleanMap() const { return this == GetHeap()->boolean_map(); }
bool Map::IsPrimitiveMap() const {
  STATIC_ASSERT(FIRST_PRIMITIVE_TYPE == FIRST_TYPE);
  return instance_type() <= LAST_PRIMITIVE_TYPE;
}
bool Map::IsJSReceiverMap() const {
  STATIC_ASSERT(LAST_JS_RECEIVER_TYPE == LAST_TYPE);
  return instance_type() >= FIRST_JS_RECEIVER_TYPE;
}
bool Map::IsJSObjectMap() const {
  STATIC_ASSERT(LAST_JS_OBJECT_TYPE == LAST_TYPE);
  return instance_type() >= FIRST_JS_OBJECT_TYPE;
}
bool Map::IsJSPromiseMap() const { return instance_type() == JS_PROMISE_TYPE; }
bool Map::IsJSArrayMap() const { return instance_type() == JS_ARRAY_TYPE; }
bool Map::IsJSFunctionMap() const {
  return instance_type() == JS_FUNCTION_TYPE;
}
bool Map::IsStringMap() const { return instance_type() < FIRST_NONSTRING_TYPE; }
bool Map::IsJSProxyMap() const { return instance_type() == JS_PROXY_TYPE; }
bool Map::IsJSGlobalProxyMap() const {
  return instance_type() == JS_GLOBAL_PROXY_TYPE;
}
bool Map::IsJSGlobalObjectMap() const {
  return instance_type() == JS_GLOBAL_OBJECT_TYPE;
}
bool Map::IsJSTypedArrayMap() const {
  return instance_type() == JS_TYPED_ARRAY_TYPE;
}
bool Map::IsJSDataViewMap() const {
  return instance_type() == JS_DATA_VIEW_TYPE;
}

Object* Map::prototype() const { return READ_FIELD(this, kPrototypeOffset); }

void Map::set_prototype(Object* value, WriteBarrierMode mode) {
  DCHECK(value->IsNull(GetIsolate()) || value->IsJSReceiver());
  WRITE_FIELD(this, kPrototypeOffset, value);
  CONDITIONAL_WRITE_BARRIER(GetHeap(), this, kPrototypeOffset, value, mode);
}

LayoutDescriptor* Map::layout_descriptor_gc_safe() const {
  DCHECK(FLAG_unbox_double_fields);
  Object* layout_desc = RELAXED_READ_FIELD(this, kLayoutDescriptorOffset);
  return LayoutDescriptor::cast_gc_safe(layout_desc);
}

bool Map::HasFastPointerLayout() const {
  DCHECK(FLAG_unbox_double_fields);
  Object* layout_desc = RELAXED_READ_FIELD(this, kLayoutDescriptorOffset);
  return LayoutDescriptor::IsFastPointerLayout(layout_desc);
}

void Map::UpdateDescriptors(DescriptorArray* descriptors,
                            LayoutDescriptor* layout_desc) {
  set_instance_descriptors(descriptors);
  if (FLAG_unbox_double_fields) {
    if (layout_descriptor()->IsSlowLayout()) {
      set_layout_descriptor(layout_desc);
    }
#ifdef VERIFY_HEAP
    // TODO(ishell): remove these checks from VERIFY_HEAP mode.
    if (FLAG_verify_heap) {
      CHECK(layout_descriptor()->IsConsistentWithMap(this));
      CHECK_EQ(Map::GetVisitorId(this), visitor_id());
    }
#else
    SLOW_DCHECK(layout_descriptor()->IsConsistentWithMap(this));
    DCHECK(visitor_id() == Map::GetVisitorId(this));
#endif
  }
}

void Map::InitializeDescriptors(DescriptorArray* descriptors,
                                LayoutDescriptor* layout_desc) {
  int len = descriptors->number_of_descriptors();
  set_instance_descriptors(descriptors);
  SetNumberOfOwnDescriptors(len);

  if (FLAG_unbox_double_fields) {
    set_layout_descriptor(layout_desc);
#ifdef VERIFY_HEAP
    // TODO(ishell): remove these checks from VERIFY_HEAP mode.
    if (FLAG_verify_heap) {
      CHECK(layout_descriptor()->IsConsistentWithMap(this));
    }
#else
    SLOW_DCHECK(layout_descriptor()->IsConsistentWithMap(this));
#endif
    set_visitor_id(Map::GetVisitorId(this));
  }
}

void Map::set_bit_field3(uint32_t bits) {
  if (kInt32Size != kPointerSize) {
    WRITE_UINT32_FIELD(this, kBitField3Offset + kInt32Size, 0);
  }
  WRITE_UINT32_FIELD(this, kBitField3Offset, bits);
}

uint32_t Map::bit_field3() const {
  return READ_UINT32_FIELD(this, kBitField3Offset);
}

LayoutDescriptor* Map::GetLayoutDescriptor() const {
  return FLAG_unbox_double_fields ? layout_descriptor()
                                  : LayoutDescriptor::FastPointerLayout();
}

void Map::AppendDescriptor(Descriptor* desc) {
  DescriptorArray* descriptors = instance_descriptors();
  int number_of_own_descriptors = NumberOfOwnDescriptors();
  DCHECK(descriptors->number_of_descriptors() == number_of_own_descriptors);
  descriptors->Append(desc);
  SetNumberOfOwnDescriptors(number_of_own_descriptors + 1);

  // Properly mark the map if the {desc} is an "interesting symbol".
  if (desc->GetKey()->IsInterestingSymbol()) {
    set_may_have_interesting_symbols(true);
  }
  PropertyDetails details = desc->GetDetails();
  if (details.location() == kField) {
    DCHECK_GT(UnusedPropertyFields(), 0);
    AccountAddedPropertyField();
  }

// This function does not support appending double field descriptors and
// it should never try to (otherwise, layout descriptor must be updated too).
#ifdef DEBUG
  DCHECK(details.location() != kField || !details.representation().IsDouble());
#endif
}

Object* Map::GetBackPointer() const {
  Object* object = constructor_or_backpointer();
  if (object->IsMap()) {
    return object;
  }
  return GetIsolate()->heap()->undefined_value();
}

Map* Map::ElementsTransitionMap() {
  DisallowHeapAllocation no_gc;
  return TransitionsAccessor(this, &no_gc)
      .SearchSpecial(GetHeap()->elements_transition_symbol());
}

Object* Map::prototype_info() const {
  DCHECK(is_prototype_map());
  return READ_FIELD(this, Map::kTransitionsOrPrototypeInfoOffset);
}

void Map::set_prototype_info(Object* value, WriteBarrierMode mode) {
  CHECK(is_prototype_map());
  WRITE_FIELD(this, Map::kTransitionsOrPrototypeInfoOffset, value);
  CONDITIONAL_WRITE_BARRIER(
      GetHeap(), this, Map::kTransitionsOrPrototypeInfoOffset, value, mode);
}

void Map::SetBackPointer(Object* value, WriteBarrierMode mode) {
  CHECK_GE(instance_type(), FIRST_JS_RECEIVER_TYPE);
  CHECK(value->IsMap());
  CHECK(GetBackPointer()->IsUndefined(GetIsolate()));
  CHECK_IMPLIES(value->IsMap(), Map::cast(value)->GetConstructor() ==
                                    constructor_or_backpointer());
  set_constructor_or_backpointer(value, mode);
}

ACCESSORS(Map, dependent_code, DependentCode, kDependentCodeOffset)
ACCESSORS(Map, weak_cell_cache, Object, kWeakCellCacheOffset)
ACCESSORS(Map, prototype_validity_cell, Object, kPrototypeValidityCellOffset)
ACCESSORS(Map, constructor_or_backpointer, Object,
          kConstructorOrBackPointerOffset)

bool Map::IsPrototypeValidityCellValid() const {
  Object* validity_cell = prototype_validity_cell();
  Object* value = validity_cell->IsSmi() ? Smi::cast(validity_cell)
                                         : Cell::cast(validity_cell)->value();
  return value == Smi::FromInt(Map::kPrototypeChainValid);
}

Object* Map::GetConstructor() const {
  Object* maybe_constructor = constructor_or_backpointer();
  // Follow any back pointers.
  while (maybe_constructor->IsMap()) {
    maybe_constructor =
        Map::cast(maybe_constructor)->constructor_or_backpointer();
  }
  return maybe_constructor;
}

FunctionTemplateInfo* Map::GetFunctionTemplateInfo() const {
  Object* constructor = GetConstructor();
  if (constructor->IsJSFunction()) {
    DCHECK(JSFunction::cast(constructor)->shared()->IsApiFunction());
    return JSFunction::cast(constructor)->shared()->get_api_func_data();
  }
  DCHECK(constructor->IsFunctionTemplateInfo());
  return FunctionTemplateInfo::cast(constructor);
}

void Map::SetConstructor(Object* constructor, WriteBarrierMode mode) {
  // Never overwrite a back pointer with a constructor.
  CHECK(!constructor_or_backpointer()->IsMap());
  set_constructor_or_backpointer(constructor, mode);
}

Handle<Map> Map::CopyInitialMap(Handle<Map> map) {
  return CopyInitialMap(map, map->instance_size(), map->GetInObjectProperties(),
                        map->UnusedPropertyFields());
}

bool Map::IsInobjectSlackTrackingInProgress() const {
  return construction_counter() != Map::kNoSlackTracking;
}

void Map::InobjectSlackTrackingStep() {
  // Slack tracking should only be performed on an initial map.
  DCHECK(GetBackPointer()->IsUndefined(GetIsolate()));
  if (!IsInobjectSlackTrackingInProgress()) return;
  int counter = construction_counter();
  set_construction_counter(counter - 1);
  if (counter == kSlackTrackingCounterEnd) {
    CompleteInobjectSlackTracking();
  }
}

int Map::SlackForArraySize(int old_size, int size_limit) {
  const int max_slack = size_limit - old_size;
  CHECK_LE(0, max_slack);
  if (old_size < 4) {
    DCHECK_LE(1, max_slack);
    return 1;
  }
  return Min(max_slack, old_size / 4);
}

int NormalizedMapCache::GetIndex(Handle<Map> map) {
  return map->Hash() % NormalizedMapCache::kEntries;
}

bool NormalizedMapCache::IsNormalizedMapCache(const HeapObject* obj) {
  if (!obj->IsFixedArray()) return false;
  if (FixedArray::cast(obj)->length() != NormalizedMapCache::kEntries) {
    return false;
  }
#ifdef VERIFY_HEAP
  if (FLAG_verify_heap) {
    reinterpret_cast<NormalizedMapCache*>(const_cast<HeapObject*>(obj))
        ->NormalizedMapCacheVerify();
  }
#endif
  return true;
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_MAP_INL_H_
