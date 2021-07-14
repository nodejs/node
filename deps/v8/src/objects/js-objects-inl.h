// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_JS_OBJECTS_INL_H_
#define V8_OBJECTS_JS_OBJECTS_INL_H_

#include "src/common/globals.h"
#include "src/heap/heap-write-barrier.h"
#include "src/objects/elements.h"
#include "src/objects/embedder-data-slot-inl.h"
#include "src/objects/feedback-vector.h"
#include "src/objects/field-index-inl.h"
#include "src/objects/hash-table-inl.h"
#include "src/objects/heap-number-inl.h"
#include "src/objects/js-objects.h"
#include "src/objects/keys.h"
#include "src/objects/lookup-inl.h"
#include "src/objects/property-array-inl.h"
#include "src/objects/prototype-inl.h"
#include "src/objects/shared-function-info.h"
#include "src/objects/slots.h"
#include "src/objects/smi-inl.h"
#include "src/objects/swiss-name-dictionary-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

#include "torque-generated/src/objects/js-objects-tq-inl.inc"

OBJECT_CONSTRUCTORS_IMPL(JSReceiver, HeapObject)
TQ_OBJECT_CONSTRUCTORS_IMPL(JSObject)
TQ_OBJECT_CONSTRUCTORS_IMPL(JSCustomElementsObject)
TQ_OBJECT_CONSTRUCTORS_IMPL(JSSpecialObject)
TQ_OBJECT_CONSTRUCTORS_IMPL(JSAsyncFromSyncIterator)
TQ_OBJECT_CONSTRUCTORS_IMPL(JSDate)
OBJECT_CONSTRUCTORS_IMPL(JSGlobalObject, JSSpecialObject)
TQ_OBJECT_CONSTRUCTORS_IMPL(JSGlobalProxy)
JSIteratorResult::JSIteratorResult(Address ptr) : JSObject(ptr) {}
OBJECT_CONSTRUCTORS_IMPL(JSMessageObject, JSObject)
TQ_OBJECT_CONSTRUCTORS_IMPL(JSPrimitiveWrapper)
TQ_OBJECT_CONSTRUCTORS_IMPL(JSStringIterator)

NEVER_READ_ONLY_SPACE_IMPL(JSReceiver)

CAST_ACCESSOR(JSGlobalObject)
CAST_ACCESSOR(JSIteratorResult)
CAST_ACCESSOR(JSMessageObject)
CAST_ACCESSOR(JSReceiver)

DEF_GETTER(JSObject, elements, FixedArrayBase) {
  return TaggedField<FixedArrayBase, kElementsOffset>::load(cage_base, *this);
}

FixedArrayBase JSObject::elements(PtrComprCageBase cage_base,
                                  RelaxedLoadTag) const {
  return TaggedField<FixedArrayBase, kElementsOffset>::Relaxed_Load(cage_base,
                                                                    *this);
}

void JSObject::set_elements(FixedArrayBase value, WriteBarrierMode mode) {
  // Note the relaxed atomic store.
  TaggedField<FixedArrayBase, kElementsOffset>::Relaxed_Store(*this, value);
  CONDITIONAL_WRITE_BARRIER(*this, kElementsOffset, value, mode);
}

MaybeHandle<Object> JSReceiver::GetProperty(Isolate* isolate,
                                            Handle<JSReceiver> receiver,
                                            Handle<Name> name) {
  LookupIterator it(isolate, receiver, name, receiver);
  if (!it.IsFound()) return it.factory()->undefined_value();
  return Object::GetProperty(&it);
}

MaybeHandle<Object> JSReceiver::GetElement(Isolate* isolate,
                                           Handle<JSReceiver> receiver,
                                           uint32_t index) {
  LookupIterator it(isolate, receiver, index, receiver);
  if (!it.IsFound()) return it.factory()->undefined_value();
  return Object::GetProperty(&it);
}

Handle<Object> JSReceiver::GetDataProperty(Handle<JSReceiver> object,
                                           Handle<Name> name) {
  LookupIterator it(object->GetIsolate(), object, name, object,
                    LookupIterator::PROTOTYPE_CHAIN_SKIP_INTERCEPTOR);
  if (!it.IsFound()) return it.factory()->undefined_value();
  return GetDataProperty(&it);
}

MaybeHandle<HeapObject> JSReceiver::GetPrototype(Isolate* isolate,
                                                 Handle<JSReceiver> receiver) {
  // We don't expect access checks to be needed on JSProxy objects.
  DCHECK(!receiver->IsAccessCheckNeeded() || receiver->IsJSObject());
  PrototypeIterator iter(isolate, receiver, kStartAtReceiver,
                         PrototypeIterator::END_AT_NON_HIDDEN);
  do {
    if (!iter.AdvanceFollowingProxies()) return MaybeHandle<HeapObject>();
  } while (!iter.IsAtEnd());
  return PrototypeIterator::GetCurrent(iter);
}

MaybeHandle<Object> JSReceiver::GetProperty(Isolate* isolate,
                                            Handle<JSReceiver> receiver,
                                            const char* name) {
  Handle<String> str = isolate->factory()->InternalizeUtf8String(name);
  return GetProperty(isolate, receiver, str);
}

// static
V8_WARN_UNUSED_RESULT MaybeHandle<FixedArray> JSReceiver::OwnPropertyKeys(
    Handle<JSReceiver> object) {
  return KeyAccumulator::GetKeys(object, KeyCollectionMode::kOwnOnly,
                                 ALL_PROPERTIES,
                                 GetKeysConversion::kConvertToString);
}

bool JSObject::PrototypeHasNoElements(Isolate* isolate, JSObject object) {
  DisallowGarbageCollection no_gc;
  HeapObject prototype = HeapObject::cast(object.map().prototype());
  ReadOnlyRoots roots(isolate);
  HeapObject null = roots.null_value();
  FixedArrayBase empty_fixed_array = roots.empty_fixed_array();
  FixedArrayBase empty_slow_element_dictionary =
      roots.empty_slow_element_dictionary();
  while (prototype != null) {
    Map map = prototype.map();
    if (map.IsCustomElementsReceiverMap()) return false;
    FixedArrayBase elements = JSObject::cast(prototype).elements();
    if (elements != empty_fixed_array &&
        elements != empty_slow_element_dictionary) {
      return false;
    }
    prototype = HeapObject::cast(map.prototype());
  }
  return true;
}

ACCESSORS(JSReceiver, raw_properties_or_hash, Object, kPropertiesOrHashOffset)

void JSObject::EnsureCanContainHeapObjectElements(Handle<JSObject> object) {
  JSObject::ValidateElements(*object);
  ElementsKind elements_kind = object->map().elements_kind();
  if (!IsObjectElementsKind(elements_kind)) {
    if (IsHoleyElementsKind(elements_kind)) {
      TransitionElementsKind(object, HOLEY_ELEMENTS);
    } else {
      TransitionElementsKind(object, PACKED_ELEMENTS);
    }
  }
}

template <typename TSlot>
void JSObject::EnsureCanContainElements(Handle<JSObject> object, TSlot objects,
                                        uint32_t count,
                                        EnsureElementsMode mode) {
  static_assert(std::is_same<TSlot, FullObjectSlot>::value ||
                    std::is_same<TSlot, ObjectSlot>::value,
                "Only ObjectSlot and FullObjectSlot are expected here");
  ElementsKind current_kind = object->GetElementsKind();
  ElementsKind target_kind = current_kind;
  {
    DisallowGarbageCollection no_gc;
    DCHECK(mode != ALLOW_COPIED_DOUBLE_ELEMENTS);
    bool is_holey = IsHoleyElementsKind(current_kind);
    if (current_kind == HOLEY_ELEMENTS) return;
    Object the_hole = object->GetReadOnlyRoots().the_hole_value();
    for (uint32_t i = 0; i < count; ++i, ++objects) {
      Object current = *objects;
      if (current == the_hole) {
        is_holey = true;
        target_kind = GetHoleyElementsKind(target_kind);
      } else if (!current.IsSmi()) {
        if (mode == ALLOW_CONVERTED_DOUBLE_ELEMENTS && current.IsNumber()) {
          if (IsSmiElementsKind(target_kind)) {
            if (is_holey) {
              target_kind = HOLEY_DOUBLE_ELEMENTS;
            } else {
              target_kind = PACKED_DOUBLE_ELEMENTS;
            }
          }
        } else if (is_holey) {
          target_kind = HOLEY_ELEMENTS;
          break;
        } else {
          target_kind = PACKED_ELEMENTS;
        }
      }
    }
  }
  if (target_kind != current_kind) {
    TransitionElementsKind(object, target_kind);
  }
}

void JSObject::EnsureCanContainElements(Handle<JSObject> object,
                                        Handle<FixedArrayBase> elements,
                                        uint32_t length,
                                        EnsureElementsMode mode) {
  ReadOnlyRoots roots = object->GetReadOnlyRoots();
  if (elements->map() != roots.fixed_double_array_map()) {
    DCHECK(elements->map() == roots.fixed_array_map() ||
           elements->map() == roots.fixed_cow_array_map());
    if (mode == ALLOW_COPIED_DOUBLE_ELEMENTS) {
      mode = DONT_ALLOW_DOUBLE_ELEMENTS;
    }
    ObjectSlot objects =
        Handle<FixedArray>::cast(elements)->GetFirstElementAddress();
    EnsureCanContainElements(object, objects, length, mode);
    return;
  }

  DCHECK(mode == ALLOW_COPIED_DOUBLE_ELEMENTS);
  if (object->GetElementsKind() == HOLEY_SMI_ELEMENTS) {
    TransitionElementsKind(object, HOLEY_DOUBLE_ELEMENTS);
  } else if (object->GetElementsKind() == PACKED_SMI_ELEMENTS) {
    Handle<FixedDoubleArray> double_array =
        Handle<FixedDoubleArray>::cast(elements);
    for (uint32_t i = 0; i < length; ++i) {
      if (double_array->is_the_hole(i)) {
        TransitionElementsKind(object, HOLEY_DOUBLE_ELEMENTS);
        return;
      }
    }
    TransitionElementsKind(object, PACKED_DOUBLE_ELEMENTS);
  }
}

void JSObject::SetMapAndElements(Handle<JSObject> object, Handle<Map> new_map,
                                 Handle<FixedArrayBase> value) {
  Isolate* isolate = object->GetIsolate();
  JSObject::MigrateToMap(isolate, object, new_map);
  DCHECK((object->map().has_fast_smi_or_object_elements() ||
          (*value == ReadOnlyRoots(isolate).empty_fixed_array()) ||
          object->map().has_fast_string_wrapper_elements()) ==
         (value->map() == ReadOnlyRoots(isolate).fixed_array_map() ||
          value->map() == ReadOnlyRoots(isolate).fixed_cow_array_map()));
  DCHECK((*value == ReadOnlyRoots(isolate).empty_fixed_array()) ||
         (object->map().has_fast_double_elements() ==
          value->IsFixedDoubleArray()));
  object->set_elements(*value);
}

void JSObject::initialize_elements() {
  FixedArrayBase elements = map().GetInitialElements();
  set_elements(elements, SKIP_WRITE_BARRIER);
}

DEF_GETTER(JSObject, GetIndexedInterceptor, InterceptorInfo) {
  return map(cage_base).GetIndexedInterceptor(cage_base);
}

DEF_GETTER(JSObject, GetNamedInterceptor, InterceptorInfo) {
  return map(cage_base).GetNamedInterceptor(cage_base);
}

// static
int JSObject::GetHeaderSize(Map map) {
  // Check for the most common kind of JavaScript object before
  // falling into the generic switch. This speeds up the internal
  // field operations considerably on average.
  InstanceType instance_type = map.instance_type();
  return instance_type == JS_OBJECT_TYPE
             ? JSObject::kHeaderSize
             : GetHeaderSize(instance_type, map.has_prototype_slot());
}

// static
int JSObject::GetEmbedderFieldsStartOffset(Map map) {
  // Embedder fields are located after the object header.
  return GetHeaderSize(map);
}

int JSObject::GetEmbedderFieldsStartOffset() {
  return GetEmbedderFieldsStartOffset(map());
}

// static
int JSObject::GetEmbedderFieldCount(Map map) {
  int instance_size = map.instance_size();
  if (instance_size == kVariableSizeSentinel) return 0;
  // Embedder fields are located after the object header, whereas in-object
  // properties are located at the end of the object. We don't have to round up
  // the header size here because division by kEmbedderDataSlotSizeInTaggedSlots
  // will swallow potential padding in case of (kTaggedSize !=
  // kSystemPointerSize) anyway.
  return (((instance_size - GetEmbedderFieldsStartOffset(map)) >>
           kTaggedSizeLog2) -
          map.GetInObjectProperties()) /
         kEmbedderDataSlotSizeInTaggedSlots;
}

int JSObject::GetEmbedderFieldCount() const {
  return GetEmbedderFieldCount(map());
}

int JSObject::GetEmbedderFieldOffset(int index) {
  DCHECK_LT(static_cast<unsigned>(index),
            static_cast<unsigned>(GetEmbedderFieldCount()));
  return GetEmbedderFieldsStartOffset() + (kEmbedderDataSlotSize * index);
}

void JSObject::InitializeEmbedderField(Isolate* isolate, int index) {
  EmbedderDataSlot(*this, index).AllocateExternalPointerEntry(isolate);
}

Object JSObject::GetEmbedderField(int index) {
  return EmbedderDataSlot(*this, index).load_tagged();
}

void JSObject::SetEmbedderField(int index, Object value) {
  EmbedderDataSlot::store_tagged(*this, index, value);
}

void JSObject::SetEmbedderField(int index, Smi value) {
  EmbedderDataSlot(*this, index).store_smi(value);
}

// Access fast-case object properties at index. The use of these routines
// is needed to correctly distinguish between properties stored in-object and
// properties stored in the properties array.
Object JSObject::RawFastPropertyAt(FieldIndex index) const {
  PtrComprCageBase cage_base = GetPtrComprCageBase(*this);
  return RawFastPropertyAt(cage_base, index);
}

Object JSObject::RawFastPropertyAt(PtrComprCageBase cage_base,
                                   FieldIndex index) const {
  if (index.is_inobject()) {
    return TaggedField<Object>::load(cage_base, *this, index.offset());
  } else {
    return property_array(cage_base).get(cage_base,
                                         index.outobject_array_index());
  }
}

void JSObject::RawFastInobjectPropertyAtPut(FieldIndex index, Object value,
                                            WriteBarrierMode mode) {
  DCHECK(index.is_inobject());
  int offset = index.offset();
  WRITE_FIELD(*this, offset, value);
  CONDITIONAL_WRITE_BARRIER(*this, offset, value, mode);
}

void JSObject::FastPropertyAtPut(FieldIndex index, Object value,
                                 WriteBarrierMode mode) {
  if (index.is_inobject()) {
    RawFastInobjectPropertyAtPut(index, value, mode);
  } else {
    DCHECK_EQ(UPDATE_WRITE_BARRIER, mode);
    property_array().set(index.outobject_array_index(), value);
  }
}

void JSObject::WriteToField(InternalIndex descriptor, PropertyDetails details,
                            Object value) {
  DCHECK_EQ(kField, details.location());
  DCHECK_EQ(kData, details.kind());
  DisallowGarbageCollection no_gc;
  FieldIndex index = FieldIndex::ForDescriptor(map(), descriptor);
  if (details.representation().IsDouble()) {
    // Manipulating the signaling NaN used for the hole and uninitialized
    // double field sentinel in C++, e.g. with bit_cast or value()/set_value(),
    // will change its value on ia32 (the x87 stack is used to return values
    // and stores to the stack silently clear the signalling bit).
    uint64_t bits;
    if (value.IsSmi()) {
      bits = bit_cast<uint64_t>(static_cast<double>(Smi::ToInt(value)));
    } else if (value.IsUninitialized()) {
      bits = kHoleNanInt64;
    } else {
      DCHECK(value.IsHeapNumber());
      bits = HeapNumber::cast(value).value_as_bits();
    }
    auto box = HeapNumber::cast(RawFastPropertyAt(index));
    box.set_value_as_bits(bits);
  } else {
    FastPropertyAtPut(index, value);
  }
}

int JSObject::GetInObjectPropertyOffset(int index) {
  return map().GetInObjectPropertyOffset(index);
}

Object JSObject::InObjectPropertyAt(int index) {
  int offset = GetInObjectPropertyOffset(index);
  return TaggedField<Object>::load(*this, offset);
}

Object JSObject::InObjectPropertyAtPut(int index, Object value,
                                       WriteBarrierMode mode) {
  // Adjust for the number of properties stored in the object.
  int offset = GetInObjectPropertyOffset(index);
  WRITE_FIELD(*this, offset, value);
  CONDITIONAL_WRITE_BARRIER(*this, offset, value, mode);
  return value;
}

void JSObject::InitializeBody(Map map, int start_offset,
                              bool is_slack_tracking_in_progress,
                              MapWord filler_map, Object undefined_filler) {
  int size = map.instance_size();
  int offset = start_offset;
  if (is_slack_tracking_in_progress) {
    int end_of_pre_allocated_offset =
        size - (map.UnusedPropertyFields() * kTaggedSize);
    DCHECK_LE(kHeaderSize, end_of_pre_allocated_offset);
    // fill start with references to the undefined value object
    while (offset < end_of_pre_allocated_offset) {
      WRITE_FIELD(*this, offset, undefined_filler);
      offset += kTaggedSize;
    }
    // fill the remainder with one word filler objects (ie just a map word)
    while (offset < size) {
      Object fm = Object(filler_map.ptr());
      WRITE_FIELD(*this, offset, fm);
      offset += kTaggedSize;
    }
  } else {
    while (offset < size) {
      // fill with references to the undefined value object
      WRITE_FIELD(*this, offset, undefined_filler);
      offset += kTaggedSize;
    }
  }
}

ACCESSORS(JSGlobalObject, native_context, NativeContext, kNativeContextOffset)
ACCESSORS(JSGlobalObject, global_proxy, JSGlobalProxy, kGlobalProxyOffset)

DEF_GETTER(JSGlobalObject, native_context_unchecked, Object) {
  return TaggedField<Object, kNativeContextOffset>::load(cage_base, *this);
}

bool JSMessageObject::DidEnsureSourcePositionsAvailable() const {
  return shared_info().IsUndefined();
}

int JSMessageObject::GetStartPosition() const {
  DCHECK(DidEnsureSourcePositionsAvailable());
  return start_position();
}

int JSMessageObject::GetEndPosition() const {
  DCHECK(DidEnsureSourcePositionsAvailable());
  return end_position();
}

MessageTemplate JSMessageObject::type() const {
  return MessageTemplateFromInt(raw_type());
}

void JSMessageObject::set_type(MessageTemplate value) {
  set_raw_type(static_cast<int>(value));
}

ACCESSORS(JSMessageObject, argument, Object, kArgumentsOffset)
ACCESSORS(JSMessageObject, script, Script, kScriptOffset)
ACCESSORS(JSMessageObject, stack_frames, Object, kStackFramesOffset)
ACCESSORS(JSMessageObject, shared_info, HeapObject, kSharedInfoOffset)
ACCESSORS(JSMessageObject, bytecode_offset, Smi, kBytecodeOffsetOffset)
SMI_ACCESSORS(JSMessageObject, start_position, kStartPositionOffset)
SMI_ACCESSORS(JSMessageObject, end_position, kEndPositionOffset)
SMI_ACCESSORS(JSMessageObject, error_level, kErrorLevelOffset)
SMI_ACCESSORS(JSMessageObject, raw_type, kMessageTypeOffset)

DEF_GETTER(JSObject, GetElementsKind, ElementsKind) {
  ElementsKind kind = map(cage_base).elements_kind();
#if VERIFY_HEAP && DEBUG
  FixedArrayBase fixed_array = FixedArrayBase::unchecked_cast(
      TaggedField<HeapObject, kElementsOffset>::load(cage_base, *this));

  // If a GC was caused while constructing this object, the elements
  // pointer may point to a one pointer filler map.
  if (ElementsAreSafeToExamine(cage_base)) {
    Map map = fixed_array.map(cage_base);
    if (IsSmiOrObjectElementsKind(kind)) {
      DCHECK(map == GetReadOnlyRoots(cage_base).fixed_array_map() ||
             map == GetReadOnlyRoots(cage_base).fixed_cow_array_map());
    } else if (IsDoubleElementsKind(kind)) {
      DCHECK(fixed_array.IsFixedDoubleArray(cage_base) ||
             fixed_array == GetReadOnlyRoots(cage_base).empty_fixed_array());
    } else if (kind == DICTIONARY_ELEMENTS) {
      DCHECK(fixed_array.IsFixedArray(cage_base));
      DCHECK(fixed_array.IsNumberDictionary(cage_base));
    } else {
      DCHECK(kind > DICTIONARY_ELEMENTS ||
             IsAnyNonextensibleElementsKind(kind));
    }
    DCHECK(!IsSloppyArgumentsElementsKind(kind) ||
           elements(cage_base).IsSloppyArgumentsElements());
  }
#endif
  return kind;
}

DEF_GETTER(JSObject, GetElementsAccessor, ElementsAccessor*) {
  return ElementsAccessor::ForKind(GetElementsKind(cage_base));
}

DEF_GETTER(JSObject, HasObjectElements, bool) {
  return IsObjectElementsKind(GetElementsKind(cage_base));
}

DEF_GETTER(JSObject, HasSmiElements, bool) {
  return IsSmiElementsKind(GetElementsKind(cage_base));
}

DEF_GETTER(JSObject, HasSmiOrObjectElements, bool) {
  return IsSmiOrObjectElementsKind(GetElementsKind(cage_base));
}

DEF_GETTER(JSObject, HasDoubleElements, bool) {
  return IsDoubleElementsKind(GetElementsKind(cage_base));
}

DEF_GETTER(JSObject, HasHoleyElements, bool) {
  return IsHoleyElementsKind(GetElementsKind(cage_base));
}

DEF_GETTER(JSObject, HasFastElements, bool) {
  return IsFastElementsKind(GetElementsKind(cage_base));
}

DEF_GETTER(JSObject, HasFastPackedElements, bool) {
  return IsFastPackedElementsKind(GetElementsKind(cage_base));
}

DEF_GETTER(JSObject, HasDictionaryElements, bool) {
  return IsDictionaryElementsKind(GetElementsKind(cage_base));
}

DEF_GETTER(JSObject, HasPackedElements, bool) {
  return GetElementsKind(cage_base) == PACKED_ELEMENTS;
}

DEF_GETTER(JSObject, HasAnyNonextensibleElements, bool) {
  return IsAnyNonextensibleElementsKind(GetElementsKind(cage_base));
}

DEF_GETTER(JSObject, HasSealedElements, bool) {
  return IsSealedElementsKind(GetElementsKind(cage_base));
}

DEF_GETTER(JSObject, HasNonextensibleElements, bool) {
  return IsNonextensibleElementsKind(GetElementsKind(cage_base));
}

DEF_GETTER(JSObject, HasFastArgumentsElements, bool) {
  return IsFastArgumentsElementsKind(GetElementsKind(cage_base));
}

DEF_GETTER(JSObject, HasSlowArgumentsElements, bool) {
  return IsSlowArgumentsElementsKind(GetElementsKind(cage_base));
}

DEF_GETTER(JSObject, HasSloppyArgumentsElements, bool) {
  return IsSloppyArgumentsElementsKind(GetElementsKind(cage_base));
}

DEF_GETTER(JSObject, HasStringWrapperElements, bool) {
  return IsStringWrapperElementsKind(GetElementsKind(cage_base));
}

DEF_GETTER(JSObject, HasFastStringWrapperElements, bool) {
  return GetElementsKind(cage_base) == FAST_STRING_WRAPPER_ELEMENTS;
}

DEF_GETTER(JSObject, HasSlowStringWrapperElements, bool) {
  return GetElementsKind(cage_base) == SLOW_STRING_WRAPPER_ELEMENTS;
}

DEF_GETTER(JSObject, HasTypedArrayElements, bool) {
  DCHECK(!elements(cage_base).is_null());
  return map(cage_base).has_typed_array_elements();
}

#define FIXED_TYPED_ELEMENTS_CHECK(Type, type, TYPE, ctype)   \
  DEF_GETTER(JSObject, HasFixed##Type##Elements, bool) {      \
    return map(cage_base).elements_kind() == TYPE##_ELEMENTS; \
  }

TYPED_ARRAYS(FIXED_TYPED_ELEMENTS_CHECK)

#undef FIXED_TYPED_ELEMENTS_CHECK

DEF_GETTER(JSObject, HasNamedInterceptor, bool) {
  return map(cage_base).has_named_interceptor();
}

DEF_GETTER(JSObject, HasIndexedInterceptor, bool) {
  return map(cage_base).has_indexed_interceptor();
}

RELEASE_ACQUIRE_ACCESSORS_CHECKED2(JSGlobalObject, global_dictionary,
                                   GlobalDictionary, kPropertiesOrHashOffset,
                                   !HasFastProperties(cage_base), true)

DEF_GETTER(JSObject, element_dictionary, NumberDictionary) {
  DCHECK(HasDictionaryElements(cage_base) ||
         HasSlowStringWrapperElements(cage_base));
  return NumberDictionary::cast(elements(cage_base));
}

void JSReceiver::initialize_properties(Isolate* isolate) {
  ReadOnlyRoots roots(isolate);
  DCHECK(!ObjectInYoungGeneration(roots.empty_fixed_array()));
  DCHECK(!ObjectInYoungGeneration(roots.empty_property_dictionary()));
  DCHECK(!ObjectInYoungGeneration(roots.empty_ordered_property_dictionary()));
  if (map(isolate).is_dictionary_map()) {
    if (V8_ENABLE_SWISS_NAME_DICTIONARY_BOOL) {
      WRITE_FIELD(*this, kPropertiesOrHashOffset,
                  roots.empty_swiss_property_dictionary());
    } else {
      WRITE_FIELD(*this, kPropertiesOrHashOffset,
                  roots.empty_property_dictionary());
    }
  } else {
    WRITE_FIELD(*this, kPropertiesOrHashOffset, roots.empty_fixed_array());
  }
}

DEF_GETTER(JSReceiver, HasFastProperties, bool) {
  DCHECK(raw_properties_or_hash(cage_base).IsSmi() ||
         ((raw_properties_or_hash(cage_base).IsGlobalDictionary(cage_base) ||
           raw_properties_or_hash(cage_base).IsNameDictionary(cage_base) ||
           raw_properties_or_hash(cage_base).IsSwissNameDictionary(
               cage_base)) == map(cage_base).is_dictionary_map()));
  return !map(cage_base).is_dictionary_map();
}

DEF_GETTER(JSReceiver, property_dictionary, NameDictionary) {
  DCHECK(!IsJSGlobalObject(cage_base));
  DCHECK(!HasFastProperties(cage_base));
  DCHECK(!V8_ENABLE_SWISS_NAME_DICTIONARY_BOOL);

  Object prop = raw_properties_or_hash(cage_base);
  if (prop.IsSmi()) {
    return GetReadOnlyRoots(cage_base).empty_property_dictionary();
  }
  return NameDictionary::cast(prop);
}

DEF_GETTER(JSReceiver, property_dictionary_swiss, SwissNameDictionary) {
  DCHECK(!IsJSGlobalObject(cage_base));
  DCHECK(!HasFastProperties(cage_base));
  DCHECK(V8_ENABLE_SWISS_NAME_DICTIONARY_BOOL);

  Object prop = raw_properties_or_hash(cage_base);
  if (prop.IsSmi()) {
    return GetReadOnlyRoots(cage_base).empty_swiss_property_dictionary();
  }
  return SwissNameDictionary::cast(prop);
}

// TODO(gsathya): Pass isolate directly to this function and access
// the heap from this.
DEF_GETTER(JSReceiver, property_array, PropertyArray) {
  DCHECK(HasFastProperties(cage_base));
  Object prop = raw_properties_or_hash(cage_base);
  if (prop.IsSmi() || prop == GetReadOnlyRoots(cage_base).empty_fixed_array()) {
    return GetReadOnlyRoots(cage_base).empty_property_array();
  }
  return PropertyArray::cast(prop);
}

Maybe<bool> JSReceiver::HasProperty(Handle<JSReceiver> object,
                                    Handle<Name> name) {
  Isolate* isolate = object->GetIsolate();
  LookupIterator::Key key(isolate, name);
  LookupIterator it(isolate, object, key, object);
  return HasProperty(&it);
}

Maybe<bool> JSReceiver::HasOwnProperty(Handle<JSReceiver> object,
                                       uint32_t index) {
  if (object->IsJSModuleNamespace()) return Just(false);

  if (object->IsJSObject()) {  // Shortcut.
    LookupIterator it(object->GetIsolate(), object, index, object,
                      LookupIterator::OWN);
    return HasProperty(&it);
  }

  Maybe<PropertyAttributes> attributes =
      JSReceiver::GetOwnPropertyAttributes(object, index);
  MAYBE_RETURN(attributes, Nothing<bool>());
  return Just(attributes.FromJust() != ABSENT);
}

Maybe<PropertyAttributes> JSReceiver::GetPropertyAttributes(
    Handle<JSReceiver> object, Handle<Name> name) {
  Isolate* isolate = object->GetIsolate();
  LookupIterator::Key key(isolate, name);
  LookupIterator it(isolate, object, key, object);
  return GetPropertyAttributes(&it);
}

Maybe<PropertyAttributes> JSReceiver::GetOwnPropertyAttributes(
    Handle<JSReceiver> object, Handle<Name> name) {
  Isolate* isolate = object->GetIsolate();
  LookupIterator::Key key(isolate, name);
  LookupIterator it(isolate, object, key, object, LookupIterator::OWN);
  return GetPropertyAttributes(&it);
}

Maybe<PropertyAttributes> JSReceiver::GetOwnPropertyAttributes(
    Handle<JSReceiver> object, uint32_t index) {
  LookupIterator it(object->GetIsolate(), object, index, object,
                    LookupIterator::OWN);
  return GetPropertyAttributes(&it);
}

Maybe<bool> JSReceiver::HasElement(Handle<JSReceiver> object, uint32_t index) {
  LookupIterator it(object->GetIsolate(), object, index, object);
  return HasProperty(&it);
}

Maybe<PropertyAttributes> JSReceiver::GetElementAttributes(
    Handle<JSReceiver> object, uint32_t index) {
  Isolate* isolate = object->GetIsolate();
  LookupIterator it(isolate, object, index, object);
  return GetPropertyAttributes(&it);
}

Maybe<PropertyAttributes> JSReceiver::GetOwnElementAttributes(
    Handle<JSReceiver> object, uint32_t index) {
  Isolate* isolate = object->GetIsolate();
  LookupIterator it(isolate, object, index, object, LookupIterator::OWN);
  return GetPropertyAttributes(&it);
}

bool JSGlobalObject::IsDetached() {
  return global_proxy().IsDetachedFrom(*this);
}

bool JSGlobalProxy::IsDetachedFrom(JSGlobalObject global) const {
  const PrototypeIterator iter(this->GetIsolate(), *this);
  return iter.GetCurrent() != global;
}

inline int JSGlobalProxy::SizeWithEmbedderFields(int embedder_field_count) {
  DCHECK_GE(embedder_field_count, 0);
  return kHeaderSize + embedder_field_count * kEmbedderDataSlotSize;
}

ACCESSORS(JSIteratorResult, value, Object, kValueOffset)
ACCESSORS(JSIteratorResult, done, Object, kDoneOffset)

// If the fast-case backing storage takes up much more memory than a dictionary
// backing storage would, the object should have slow elements.
// static
static inline bool ShouldConvertToSlowElements(uint32_t used_elements,
                                               uint32_t new_capacity) {
  uint32_t size_threshold = NumberDictionary::kPreferFastElementsSizeFactor *
                            NumberDictionary::ComputeCapacity(used_elements) *
                            NumberDictionary::kEntrySize;
  return size_threshold <= new_capacity;
}

static inline bool ShouldConvertToSlowElements(JSObject object,
                                               uint32_t capacity,
                                               uint32_t index,
                                               uint32_t* new_capacity) {
  STATIC_ASSERT(JSObject::kMaxUncheckedOldFastElementsLength <=
                JSObject::kMaxUncheckedFastElementsLength);
  if (index < capacity) {
    *new_capacity = capacity;
    return false;
  }
  if (index - capacity >= JSObject::kMaxGap) return true;
  *new_capacity = JSObject::NewElementsCapacity(index + 1);
  DCHECK_LT(index, *new_capacity);
  if (*new_capacity <= JSObject::kMaxUncheckedOldFastElementsLength ||
      (*new_capacity <= JSObject::kMaxUncheckedFastElementsLength &&
       ObjectInYoungGeneration(object))) {
    return false;
  }
  return ShouldConvertToSlowElements(object.GetFastElementsUsage(),
                                     *new_capacity);
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_JS_OBJECTS_INL_H_
