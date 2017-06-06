// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/lookup.h"

#include "src/bootstrapper.h"
#include "src/deoptimizer.h"
#include "src/elements.h"
#include "src/field-type.h"
#include "src/isolate-inl.h"

namespace v8 {
namespace internal {

// static
LookupIterator LookupIterator::PropertyOrElement(Isolate* isolate,
                                                 Handle<Object> receiver,
                                                 Handle<Object> key,
                                                 bool* success,
                                                 Configuration configuration) {
  uint32_t index = 0;
  if (key->ToArrayIndex(&index)) {
    *success = true;
    return LookupIterator(isolate, receiver, index, configuration);
  }

  Handle<Name> name;
  *success = Object::ToName(isolate, key).ToHandle(&name);
  if (!*success) {
    DCHECK(isolate->has_pending_exception());
    // Return an unusable dummy.
    return LookupIterator(receiver, isolate->factory()->empty_string());
  }

  if (name->AsArrayIndex(&index)) {
    LookupIterator it(isolate, receiver, index, configuration);
    // Here we try to avoid having to rebuild the string later
    // by storing it on the indexed LookupIterator.
    it.name_ = name;
    return it;
  }

  return LookupIterator(receiver, name, configuration);
}

template <bool is_element>
void LookupIterator::Start() {
  DisallowHeapAllocation no_gc;

  has_property_ = false;
  state_ = NOT_FOUND;
  holder_ = initial_holder_;

  JSReceiver* holder = *holder_;
  Map* map = holder->map();

  state_ = LookupInHolder<is_element>(map, holder);
  if (IsFound()) return;

  NextInternal<is_element>(map, holder);
}

template void LookupIterator::Start<true>();
template void LookupIterator::Start<false>();

void LookupIterator::Next() {
  DCHECK_NE(JSPROXY, state_);
  DCHECK_NE(TRANSITION, state_);
  DisallowHeapAllocation no_gc;
  has_property_ = false;

  JSReceiver* holder = *holder_;
  Map* map = holder->map();

  if (map->IsSpecialReceiverMap()) {
    state_ = IsElement() ? LookupInSpecialHolder<true>(map, holder)
                         : LookupInSpecialHolder<false>(map, holder);
    if (IsFound()) return;
  }

  IsElement() ? NextInternal<true>(map, holder)
              : NextInternal<false>(map, holder);
}

template <bool is_element>
void LookupIterator::NextInternal(Map* map, JSReceiver* holder) {
  do {
    JSReceiver* maybe_holder = NextHolder(map);
    if (maybe_holder == nullptr) {
      if (interceptor_state_ == InterceptorState::kSkipNonMasking) {
        RestartLookupForNonMaskingInterceptors<is_element>();
        return;
      }
      state_ = NOT_FOUND;
      if (holder != *holder_) holder_ = handle(holder, isolate_);
      return;
    }
    holder = maybe_holder;
    map = holder->map();
    state_ = LookupInHolder<is_element>(map, holder);
  } while (!IsFound());

  holder_ = handle(holder, isolate_);
}

template <bool is_element>
void LookupIterator::RestartInternal(InterceptorState interceptor_state) {
  interceptor_state_ = interceptor_state;
  property_details_ = PropertyDetails::Empty();
  number_ = static_cast<uint32_t>(DescriptorArray::kNotFound);
  Start<is_element>();
}

template void LookupIterator::RestartInternal<true>(InterceptorState);
template void LookupIterator::RestartInternal<false>(InterceptorState);

// static
Handle<JSReceiver> LookupIterator::GetRootForNonJSReceiver(
    Isolate* isolate, Handle<Object> receiver, uint32_t index) {
  // Strings are the only objects with properties (only elements) directly on
  // the wrapper. Hence we can skip generating the wrapper for all other cases.
  if (index != kMaxUInt32 && receiver->IsString() &&
      index < static_cast<uint32_t>(String::cast(*receiver)->length())) {
    // TODO(verwaest): Speed this up. Perhaps use a cached wrapper on the native
    // context, ensuring that we don't leak it into JS?
    Handle<JSFunction> constructor = isolate->string_function();
    Handle<JSObject> result = isolate->factory()->NewJSObject(constructor);
    Handle<JSValue>::cast(result)->set_value(*receiver);
    return result;
  }
  auto root =
      handle(receiver->GetPrototypeChainRootMap(isolate)->prototype(), isolate);
  if (root->IsNull(isolate)) {
    unsigned int magic = 0xbbbbbbbb;
    isolate->PushStackTraceAndDie(magic, *receiver, NULL, magic);
  }
  return Handle<JSReceiver>::cast(root);
}


Handle<Map> LookupIterator::GetReceiverMap() const {
  if (receiver_->IsNumber()) return factory()->heap_number_map();
  return handle(Handle<HeapObject>::cast(receiver_)->map(), isolate_);
}

bool LookupIterator::HasAccess() const {
  DCHECK_EQ(ACCESS_CHECK, state_);
  return isolate_->MayAccess(handle(isolate_->context()),
                             GetHolder<JSObject>());
}

template <bool is_element>
void LookupIterator::ReloadPropertyInformation() {
  state_ = BEFORE_PROPERTY;
  interceptor_state_ = InterceptorState::kUninitialized;
  state_ = LookupInHolder<is_element>(holder_->map(), *holder_);
  DCHECK(IsFound() || !holder_->HasFastProperties());
}

void LookupIterator::InternalUpdateProtector() {
  if (isolate_->bootstrapper()->IsActive()) return;

  if (*name_ == heap()->constructor_string()) {
    if (!isolate_->IsArraySpeciesLookupChainIntact()) return;
    // Setting the constructor property could change an instance's @@species
    if (holder_->IsJSArray()) {
      isolate_->CountUsage(
          v8::Isolate::UseCounterFeature::kArrayInstanceConstructorModified);
      isolate_->InvalidateArraySpeciesProtector();
    } else if (holder_->map()->is_prototype_map()) {
      DisallowHeapAllocation no_gc;
      // Setting the constructor of Array.prototype of any realm also needs
      // to invalidate the species protector
      if (isolate_->IsInAnyContext(*holder_,
                                   Context::INITIAL_ARRAY_PROTOTYPE_INDEX)) {
        isolate_->CountUsage(v8::Isolate::UseCounterFeature::
                                 kArrayPrototypeConstructorModified);
        isolate_->InvalidateArraySpeciesProtector();
      }
    }
  } else if (*name_ == heap()->species_symbol()) {
    if (!isolate_->IsArraySpeciesLookupChainIntact()) return;
    // Setting the Symbol.species property of any Array constructor invalidates
    // the species protector
    if (isolate_->IsInAnyContext(*holder_, Context::ARRAY_FUNCTION_INDEX)) {
      isolate_->CountUsage(
          v8::Isolate::UseCounterFeature::kArraySpeciesModified);
      isolate_->InvalidateArraySpeciesProtector();
    }
  } else if (*name_ == heap()->is_concat_spreadable_symbol()) {
    if (!isolate_->IsIsConcatSpreadableLookupChainIntact()) return;
    isolate_->InvalidateIsConcatSpreadableProtector();
  } else if (*name_ == heap()->iterator_symbol()) {
    if (!isolate_->IsArrayIteratorLookupChainIntact()) return;
    if (holder_->IsJSArray()) {
      isolate_->InvalidateArrayIteratorProtector();
    }
  }
}

void LookupIterator::PrepareForDataProperty(Handle<Object> value) {
  DCHECK(state_ == DATA || state_ == ACCESSOR);
  DCHECK(HolderIsReceiverOrHiddenPrototype());

  Handle<JSObject> holder = GetHolder<JSObject>();

  if (IsElement()) {
    ElementsKind kind = holder->GetElementsKind();
    ElementsKind to = value->OptimalElementsKind();
    if (IsHoleyElementsKind(kind)) to = GetHoleyElementsKind(to);
    to = GetMoreGeneralElementsKind(kind, to);

    if (kind != to) {
      JSObject::TransitionElementsKind(holder, to);
    }

    // Copy the backing store if it is copy-on-write.
    if (IsFastSmiOrObjectElementsKind(to)) {
      JSObject::EnsureWritableFastElements(holder);
    }
    return;
  }

  if (holder->IsJSGlobalObject()) {
    Handle<GlobalDictionary> dictionary(holder->global_dictionary());
    Handle<PropertyCell> cell(
        PropertyCell::cast(dictionary->ValueAt(dictionary_entry())));
    DCHECK(!cell->IsTheHole(isolate_));
    property_details_ = cell->property_details();
    PropertyCell::PrepareForValue(dictionary, dictionary_entry(), value,
                                  property_details_);
    return;
  }
  if (!holder->HasFastProperties()) return;

  PropertyConstness new_constness = kConst;
  if (FLAG_track_constant_fields) {
    if (constness() == kConst) {
      DCHECK_EQ(kData, property_details_.kind());
      // Check that current value matches new value otherwise we should make
      // the property mutable.
      if (!IsConstFieldValueEqualTo(*value)) new_constness = kMutable;
    }
  } else {
    new_constness = kMutable;
  }

  Handle<Map> old_map(holder->map(), isolate_);
  Handle<Map> new_map = Map::PrepareForDataProperty(
      old_map, descriptor_number(), new_constness, value);

  if (old_map.is_identical_to(new_map)) {
    // Update the property details if the representation was None.
    if (constness() != new_constness || representation().IsNone()) {
      property_details_ =
          new_map->instance_descriptors()->GetDetails(descriptor_number());
    }
    return;
  }

  JSObject::MigrateToMap(holder, new_map);
  ReloadPropertyInformation<false>();
}


void LookupIterator::ReconfigureDataProperty(Handle<Object> value,
                                             PropertyAttributes attributes) {
  DCHECK(state_ == DATA || state_ == ACCESSOR);
  DCHECK(HolderIsReceiverOrHiddenPrototype());
  Handle<JSObject> holder = GetHolder<JSObject>();
  if (IsElement()) {
    DCHECK(!holder->HasFixedTypedArrayElements());
    DCHECK(attributes != NONE || !holder->HasFastElements());
    Handle<FixedArrayBase> elements(holder->elements());
    holder->GetElementsAccessor()->Reconfigure(holder, elements, number_, value,
                                               attributes);
    ReloadPropertyInformation<true>();
  } else if (holder->HasFastProperties()) {
    Handle<Map> old_map(holder->map(), isolate_);
    Handle<Map> new_map = Map::ReconfigureExistingProperty(
        old_map, descriptor_number(), i::kData, attributes);
    // Force mutable to avoid changing constant value by reconfiguring
    // kData -> kAccessor -> kData.
    new_map = Map::PrepareForDataProperty(new_map, descriptor_number(),
                                          kMutable, value);
    JSObject::MigrateToMap(holder, new_map);
    ReloadPropertyInformation<false>();
  }

  if (!IsElement() && !holder->HasFastProperties()) {
    PropertyDetails details(kData, attributes, 0, PropertyCellType::kMutable);
    if (holder->IsJSGlobalObject()) {
      Handle<GlobalDictionary> dictionary(holder->global_dictionary());

      Handle<PropertyCell> cell = PropertyCell::PrepareForValue(
          dictionary, dictionary_entry(), value, details);
      cell->set_value(*value);
      property_details_ = cell->property_details();
    } else {
      Handle<NameDictionary> dictionary(holder->property_dictionary());
      PropertyDetails original_details =
          dictionary->DetailsAt(dictionary_entry());
      int enumeration_index = original_details.dictionary_index();
      DCHECK(enumeration_index > 0);
      details = details.set_index(enumeration_index);
      dictionary->SetEntry(dictionary_entry(), name(), value, details);
      property_details_ = details;
    }
    state_ = DATA;
  }

  WriteDataValue(value, true);

#if VERIFY_HEAP
  if (FLAG_verify_heap) {
    holder->JSObjectVerify();
  }
#endif
}

// Can only be called when the receiver is a JSObject. JSProxy has to be handled
// via a trap. Adding properties to primitive values is not observable.
void LookupIterator::PrepareTransitionToDataProperty(
    Handle<JSObject> receiver, Handle<Object> value,
    PropertyAttributes attributes, Object::StoreFromKeyed store_mode) {
  DCHECK(receiver.is_identical_to(GetStoreTarget()));
  if (state_ == TRANSITION) return;

  if (!IsElement() && name()->IsPrivate()) {
    attributes = static_cast<PropertyAttributes>(attributes | DONT_ENUM);
  }

  DCHECK(state_ != LookupIterator::ACCESSOR ||
         (GetAccessors()->IsAccessorInfo() &&
          AccessorInfo::cast(*GetAccessors())->is_special_data_property()));
  DCHECK_NE(INTEGER_INDEXED_EXOTIC, state_);
  DCHECK(state_ == NOT_FOUND || !HolderIsReceiverOrHiddenPrototype());

  Handle<Map> map(receiver->map(), isolate_);

  // Dictionary maps can always have additional data properties.
  if (map->is_dictionary_map()) {
    state_ = TRANSITION;
    if (map->IsJSGlobalObjectMap()) {
      // Install a property cell.
      Handle<JSGlobalObject> global = Handle<JSGlobalObject>::cast(receiver);
      int entry;
      Handle<PropertyCell> cell = JSGlobalObject::EnsureEmptyPropertyCell(
          global, name(), PropertyCellType::kUninitialized, &entry);
      Handle<GlobalDictionary> dictionary(global->global_dictionary(),
                                          isolate_);
      DCHECK(cell->value()->IsTheHole(isolate_));
      DCHECK(!value->IsTheHole(isolate_));
      transition_ = cell;
      // Assign an enumeration index to the property and update
      // SetNextEnumerationIndex.
      int index = dictionary->NextEnumerationIndex();
      dictionary->SetNextEnumerationIndex(index + 1);
      property_details_ = PropertyDetails(kData, attributes, index,
                                          PropertyCellType::kUninitialized);
      PropertyCellType new_type =
          PropertyCell::UpdatedType(cell, value, property_details_);
      property_details_ = property_details_.set_cell_type(new_type);
      cell->set_property_details(property_details_);
      number_ = entry;
      has_property_ = true;
    } else {
      // Don't set enumeration index (it will be set during value store).
      property_details_ =
          PropertyDetails(kData, attributes, 0, PropertyCellType::kNoCell);
      transition_ = map;
    }
    return;
  }

  Handle<Map> transition = Map::TransitionToDataProperty(
      map, name_, value, attributes, kDefaultFieldConstness, store_mode);
  state_ = TRANSITION;
  transition_ = transition;

  if (transition->is_dictionary_map()) {
    // Don't set enumeration index (it will be set during value store).
    property_details_ =
        PropertyDetails(kData, attributes, 0, PropertyCellType::kNoCell);
  } else {
    property_details_ = transition->GetLastDescriptorDetails();
    has_property_ = true;
  }
}

void LookupIterator::ApplyTransitionToDataProperty(Handle<JSObject> receiver) {
  DCHECK_EQ(TRANSITION, state_);

  DCHECK(receiver.is_identical_to(GetStoreTarget()));
  holder_ = receiver;
  if (receiver->IsJSGlobalObject()) {
    state_ = DATA;
    return;
  }
  Handle<Map> transition = transition_map();
  bool simple_transition = transition->GetBackPointer() == receiver->map();
  JSObject::MigrateToMap(receiver, transition);

  if (simple_transition) {
    int number = transition->LastAdded();
    number_ = static_cast<uint32_t>(number);
    property_details_ = transition->GetLastDescriptorDetails();
    state_ = DATA;
  } else if (receiver->map()->is_dictionary_map()) {
    Handle<NameDictionary> dictionary(receiver->property_dictionary(),
                                      isolate_);
    int entry;
    dictionary = NameDictionary::Add(dictionary, name(),
                                     isolate_->factory()->uninitialized_value(),
                                     property_details_, &entry);
    receiver->set_properties(*dictionary);
    // Reload details containing proper enumeration index value.
    property_details_ = dictionary->DetailsAt(entry);
    number_ = entry;
    has_property_ = true;
    state_ = DATA;

  } else {
    ReloadPropertyInformation<false>();
  }
}


void LookupIterator::Delete() {
  Handle<JSReceiver> holder = Handle<JSReceiver>::cast(holder_);
  if (IsElement()) {
    Handle<JSObject> object = Handle<JSObject>::cast(holder);
    ElementsAccessor* accessor = object->GetElementsAccessor();
    accessor->Delete(object, number_);
  } else {
    bool is_prototype_map = holder->map()->is_prototype_map();
    RuntimeCallTimerScope stats_scope(
        isolate_, is_prototype_map
                      ? &RuntimeCallStats::PrototypeObject_DeleteProperty
                      : &RuntimeCallStats::Object_DeleteProperty);

    PropertyNormalizationMode mode =
        is_prototype_map ? KEEP_INOBJECT_PROPERTIES : CLEAR_INOBJECT_PROPERTIES;

    if (holder->HasFastProperties()) {
      JSObject::NormalizeProperties(Handle<JSObject>::cast(holder), mode, 0,
                                    "DeletingProperty");
      ReloadPropertyInformation<false>();
    }
    // TODO(verwaest): Get rid of the name_ argument.
    JSReceiver::DeleteNormalizedProperty(holder, name_, number_);
    if (holder->IsJSObject()) {
      JSObject::ReoptimizeIfPrototype(Handle<JSObject>::cast(holder));
    }
  }
  state_ = NOT_FOUND;
}

void LookupIterator::TransitionToAccessorProperty(
    Handle<Object> getter, Handle<Object> setter,
    PropertyAttributes attributes) {
  DCHECK(!getter->IsNull(isolate_) || !setter->IsNull(isolate_));
  // Can only be called when the receiver is a JSObject. JSProxy has to be
  // handled via a trap. Adding properties to primitive values is not
  // observable.
  Handle<JSObject> receiver = GetStoreTarget();
  if (!IsElement() && name()->IsPrivate()) {
    attributes = static_cast<PropertyAttributes>(attributes | DONT_ENUM);
  }

  if (!IsElement() && !receiver->map()->is_dictionary_map()) {
    Handle<Map> old_map(receiver->map(), isolate_);

    if (!holder_.is_identical_to(receiver)) {
      holder_ = receiver;
      state_ = NOT_FOUND;
    } else if (state_ == INTERCEPTOR) {
      LookupInRegularHolder<false>(*old_map, *holder_);
    }
    int descriptor =
        IsFound() ? static_cast<int>(number_) : DescriptorArray::kNotFound;

    Handle<Map> new_map = Map::TransitionToAccessorProperty(
        isolate_, old_map, name_, descriptor, getter, setter, attributes);
    bool simple_transition = new_map->GetBackPointer() == receiver->map();
    JSObject::MigrateToMap(receiver, new_map);

    if (simple_transition) {
      int number = new_map->LastAdded();
      number_ = static_cast<uint32_t>(number);
      property_details_ = new_map->GetLastDescriptorDetails();
      state_ = ACCESSOR;
      return;
    }

    ReloadPropertyInformation<false>();
    if (!new_map->is_dictionary_map()) return;
  }

  Handle<AccessorPair> pair;
  if (state() == ACCESSOR && GetAccessors()->IsAccessorPair()) {
    pair = Handle<AccessorPair>::cast(GetAccessors());
    // If the component and attributes are identical, nothing has to be done.
    if (pair->Equals(*getter, *setter)) {
      if (property_details().attributes() == attributes) {
        if (!IsElement()) JSObject::ReoptimizeIfPrototype(receiver);
        return;
      }
    } else {
      pair = AccessorPair::Copy(pair);
      pair->SetComponents(*getter, *setter);
    }
  } else {
    pair = factory()->NewAccessorPair();
    pair->SetComponents(*getter, *setter);
  }

  TransitionToAccessorPair(pair, attributes);

#if VERIFY_HEAP
  if (FLAG_verify_heap) {
    receiver->JSObjectVerify();
  }
#endif
}


void LookupIterator::TransitionToAccessorPair(Handle<Object> pair,
                                              PropertyAttributes attributes) {
  Handle<JSObject> receiver = GetStoreTarget();
  holder_ = receiver;

  PropertyDetails details(kAccessor, attributes, 0, PropertyCellType::kMutable);

  if (IsElement()) {
    // TODO(verwaest): Move code into the element accessor.
    Handle<SeededNumberDictionary> dictionary =
        JSObject::NormalizeElements(receiver);

    dictionary = SeededNumberDictionary::Set(dictionary, index_, pair, details,
                                             receiver);
    receiver->RequireSlowElements(*dictionary);

    if (receiver->HasSlowArgumentsElements()) {
      FixedArray* parameter_map = FixedArray::cast(receiver->elements());
      uint32_t length = parameter_map->length() - 2;
      if (number_ < length) {
        parameter_map->set(number_ + 2, heap()->the_hole_value());
      }
      FixedArray::cast(receiver->elements())->set(1, *dictionary);
    } else {
      receiver->set_elements(*dictionary);
    }

    ReloadPropertyInformation<true>();
  } else {
    PropertyNormalizationMode mode = receiver->map()->is_prototype_map()
                                         ? KEEP_INOBJECT_PROPERTIES
                                         : CLEAR_INOBJECT_PROPERTIES;
    // Normalize object to make this operation simple.
    JSObject::NormalizeProperties(receiver, mode, 0,
                                  "TransitionToAccessorPair");

    JSObject::SetNormalizedProperty(receiver, name_, pair, details);
    JSObject::ReoptimizeIfPrototype(receiver);

    ReloadPropertyInformation<false>();
  }
}


bool LookupIterator::HolderIsReceiverOrHiddenPrototype() const {
  DCHECK(has_property_ || state_ == INTERCEPTOR || state_ == JSPROXY);
  // Optimization that only works if configuration_ is not mutable.
  if (!check_prototype_chain()) return true;
  DisallowHeapAllocation no_gc;
  if (*receiver_ == *holder_) return true;
  if (!receiver_->IsJSReceiver()) return false;
  JSReceiver* current = JSReceiver::cast(*receiver_);
  JSReceiver* object = *holder_;
  if (!current->map()->has_hidden_prototype()) return false;
  // JSProxy do not occur as hidden prototypes.
  if (object->IsJSProxy()) return false;
  PrototypeIterator iter(isolate(), current, kStartAtPrototype,
                         PrototypeIterator::END_AT_NON_HIDDEN);
  while (!iter.IsAtEnd()) {
    if (iter.GetCurrent<JSReceiver>() == object) return true;
    iter.Advance();
  }
  return false;
}


Handle<Object> LookupIterator::FetchValue() const {
  Object* result = NULL;
  if (IsElement()) {
    Handle<JSObject> holder = GetHolder<JSObject>();
    ElementsAccessor* accessor = holder->GetElementsAccessor();
    return accessor->Get(holder, number_);
  } else if (holder_->IsJSGlobalObject()) {
    Handle<JSObject> holder = GetHolder<JSObject>();
    result = holder->global_dictionary()->ValueAt(number_);
    DCHECK(result->IsPropertyCell());
    result = PropertyCell::cast(result)->value();
  } else if (!holder_->HasFastProperties()) {
    result = holder_->property_dictionary()->ValueAt(number_);
  } else if (property_details_.location() == kField) {
    DCHECK_EQ(kData, property_details_.kind());
    Handle<JSObject> holder = GetHolder<JSObject>();
    FieldIndex field_index = FieldIndex::ForDescriptor(holder->map(), number_);
    return JSObject::FastPropertyAt(holder, property_details_.representation(),
                                    field_index);
  } else {
    result = holder_->map()->instance_descriptors()->GetValue(number_);
  }
  return handle(result, isolate_);
}

bool LookupIterator::IsConstFieldValueEqualTo(Object* value) const {
  DCHECK(!IsElement());
  DCHECK(holder_->HasFastProperties());
  DCHECK_EQ(kField, property_details_.location());
  DCHECK_EQ(kConst, property_details_.constness());
  Handle<JSObject> holder = GetHolder<JSObject>();
  FieldIndex field_index = FieldIndex::ForDescriptor(holder->map(), number_);
  if (property_details_.representation().IsDouble()) {
    if (!value->IsNumber()) return false;
    uint64_t bits;
    if (holder->IsUnboxedDoubleField(field_index)) {
      bits = holder->RawFastDoublePropertyAsBitsAt(field_index);
    } else {
      Object* current_value = holder->RawFastPropertyAt(field_index);
      DCHECK(current_value->IsMutableHeapNumber());
      bits = HeapNumber::cast(current_value)->value_as_bits();
    }
    // Use bit representation of double to to check for hole double, since
    // manipulating the signaling NaN used for the hole in C++, e.g. with
    // bit_cast or value(), will change its value on ia32 (the x87 stack is
    // used to return values and stores to the stack silently clear the
    // signalling bit).
    if (bits == kHoleNanInt64) {
      // Uninitialized double field.
      return true;
    }
    return bit_cast<double>(bits) == value->Number();
  } else {
    Object* current_value = holder->RawFastPropertyAt(field_index);
    return current_value->IsUninitialized(isolate()) || current_value == value;
  }
}

int LookupIterator::GetFieldDescriptorIndex() const {
  DCHECK(has_property_);
  DCHECK(holder_->HasFastProperties());
  DCHECK_EQ(kField, property_details_.location());
  DCHECK_EQ(kData, property_details_.kind());
  return descriptor_number();
}

int LookupIterator::GetAccessorIndex() const {
  DCHECK(has_property_);
  DCHECK(holder_->HasFastProperties());
  DCHECK_EQ(kDescriptor, property_details_.location());
  DCHECK_EQ(kAccessor, property_details_.kind());
  return descriptor_number();
}


int LookupIterator::GetConstantIndex() const {
  DCHECK(has_property_);
  DCHECK(holder_->HasFastProperties());
  DCHECK_EQ(kDescriptor, property_details_.location());
  DCHECK_EQ(kData, property_details_.kind());
  DCHECK(!FLAG_track_constant_fields);
  DCHECK(!IsElement());
  return descriptor_number();
}

Handle<Map> LookupIterator::GetFieldOwnerMap() const {
  DCHECK(has_property_);
  DCHECK(holder_->HasFastProperties());
  DCHECK_EQ(kField, property_details_.location());
  DCHECK(!IsElement());
  Map* holder_map = holder_->map();
  return handle(holder_map->FindFieldOwner(descriptor_number()), isolate_);
}

FieldIndex LookupIterator::GetFieldIndex() const {
  DCHECK(has_property_);
  DCHECK(holder_->HasFastProperties());
  DCHECK_EQ(kField, property_details_.location());
  DCHECK(!IsElement());
  Map* holder_map = holder_->map();
  int index =
      holder_map->instance_descriptors()->GetFieldIndex(descriptor_number());
  bool is_double = representation().IsDouble();
  return FieldIndex::ForPropertyIndex(holder_map, index, is_double);
}

Handle<FieldType> LookupIterator::GetFieldType() const {
  DCHECK(has_property_);
  DCHECK(holder_->HasFastProperties());
  DCHECK_EQ(kField, property_details_.location());
  return handle(
      holder_->map()->instance_descriptors()->GetFieldType(descriptor_number()),
      isolate_);
}


Handle<PropertyCell> LookupIterator::GetPropertyCell() const {
  DCHECK(!IsElement());
  Handle<JSGlobalObject> holder = GetHolder<JSGlobalObject>();
  Object* value = holder->global_dictionary()->ValueAt(dictionary_entry());
  DCHECK(value->IsPropertyCell());
  return handle(PropertyCell::cast(value), isolate_);
}


Handle<Object> LookupIterator::GetAccessors() const {
  DCHECK_EQ(ACCESSOR, state_);
  return FetchValue();
}


Handle<Object> LookupIterator::GetDataValue() const {
  DCHECK_EQ(DATA, state_);
  Handle<Object> value = FetchValue();
  return value;
}

void LookupIterator::WriteDataValue(Handle<Object> value,
                                    bool initializing_store) {
  DCHECK_EQ(DATA, state_);
  Handle<JSReceiver> holder = GetHolder<JSReceiver>();
  if (IsElement()) {
    Handle<JSObject> object = Handle<JSObject>::cast(holder);
    ElementsAccessor* accessor = object->GetElementsAccessor();
    accessor->Set(object, number_, *value);
  } else if (holder->HasFastProperties()) {
    if (property_details_.location() == kField) {
      // Check that in case of kConst field the existing value is equal to
      // |value|.
      DCHECK_IMPLIES(
          !initializing_store && property_details_.constness() == kConst,
          IsConstFieldValueEqualTo(*value));
      JSObject::cast(*holder)->WriteToField(descriptor_number(),
                                            property_details_, *value);
    } else {
      DCHECK_EQ(kDescriptor, property_details_.location());
      DCHECK_EQ(kConst, property_details_.constness());
    }
  } else if (holder->IsJSGlobalObject()) {
    GlobalDictionary* dictionary = JSObject::cast(*holder)->global_dictionary();
    Object* cell = dictionary->ValueAt(dictionary_entry());
    DCHECK(cell->IsPropertyCell());
    PropertyCell::cast(cell)->set_value(*value);
  } else {
    NameDictionary* dictionary = holder->property_dictionary();
    dictionary->ValueAtPut(dictionary_entry(), *value);
  }
}

template <bool is_element>
bool LookupIterator::SkipInterceptor(JSObject* holder) {
  auto info = GetInterceptor<is_element>(holder);
  if (!is_element && name_->IsSymbol() && !info->can_intercept_symbols()) {
    return true;
  }
  if (info->non_masking()) {
    switch (interceptor_state_) {
      case InterceptorState::kUninitialized:
        interceptor_state_ = InterceptorState::kSkipNonMasking;
      // Fall through.
      case InterceptorState::kSkipNonMasking:
        return true;
      case InterceptorState::kProcessNonMasking:
        return false;
    }
  }
  return interceptor_state_ == InterceptorState::kProcessNonMasking;
}

JSReceiver* LookupIterator::NextHolder(Map* map) {
  DisallowHeapAllocation no_gc;
  if (map->prototype() == heap()->null_value()) return NULL;
  if (!check_prototype_chain() && !map->has_hidden_prototype()) return NULL;
  return JSReceiver::cast(map->prototype());
}

LookupIterator::State LookupIterator::NotFound(JSReceiver* const holder) const {
  DCHECK(!IsElement());
  if (!holder->IsJSTypedArray() || !name_->IsString()) return NOT_FOUND;

  Handle<String> name_string = Handle<String>::cast(name_);
  if (name_string->length() == 0) return NOT_FOUND;

  return IsSpecialIndex(isolate_->unicode_cache(), *name_string)
             ? INTEGER_INDEXED_EXOTIC
             : NOT_FOUND;
}

namespace {

template <bool is_element>
bool HasInterceptor(Map* map) {
  return is_element ? map->has_indexed_interceptor()
                    : map->has_named_interceptor();
}

}  // namespace

template <bool is_element>
LookupIterator::State LookupIterator::LookupInSpecialHolder(
    Map* const map, JSReceiver* const holder) {
  STATIC_ASSERT(INTERCEPTOR == BEFORE_PROPERTY);
  switch (state_) {
    case NOT_FOUND:
      if (map->IsJSProxyMap()) {
        if (is_element || !name_->IsPrivate()) return JSPROXY;
      }
      if (map->is_access_check_needed()) {
        if (is_element || !name_->IsPrivate()) return ACCESS_CHECK;
      }
    // Fall through.
    case ACCESS_CHECK:
      if (check_interceptor() && HasInterceptor<is_element>(map) &&
          !SkipInterceptor<is_element>(JSObject::cast(holder))) {
        if (is_element || !name_->IsPrivate()) return INTERCEPTOR;
      }
    // Fall through.
    case INTERCEPTOR:
      if (!is_element && map->IsJSGlobalObjectMap()) {
        GlobalDictionary* dict = JSObject::cast(holder)->global_dictionary();
        int number = dict->FindEntry(name_);
        if (number == GlobalDictionary::kNotFound) return NOT_FOUND;
        number_ = static_cast<uint32_t>(number);
        DCHECK(dict->ValueAt(number_)->IsPropertyCell());
        PropertyCell* cell = PropertyCell::cast(dict->ValueAt(number_));
        if (cell->value()->IsTheHole(isolate_)) return NOT_FOUND;
        property_details_ = cell->property_details();
        has_property_ = true;
        switch (property_details_.kind()) {
          case v8::internal::kData:
            return DATA;
          case v8::internal::kAccessor:
            return ACCESSOR;
        }
      }
      return LookupInRegularHolder<is_element>(map, holder);
    case ACCESSOR:
    case DATA:
      return NOT_FOUND;
    case INTEGER_INDEXED_EXOTIC:
    case JSPROXY:
    case TRANSITION:
      UNREACHABLE();
  }
  UNREACHABLE();
  return NOT_FOUND;
}

template <bool is_element>
LookupIterator::State LookupIterator::LookupInRegularHolder(
    Map* const map, JSReceiver* const holder) {
  DisallowHeapAllocation no_gc;
  if (interceptor_state_ == InterceptorState::kProcessNonMasking) {
    return NOT_FOUND;
  }

  if (is_element) {
    JSObject* js_object = JSObject::cast(holder);
    ElementsAccessor* accessor = js_object->GetElementsAccessor();
    FixedArrayBase* backing_store = js_object->elements();
    number_ =
        accessor->GetEntryForIndex(isolate_, js_object, backing_store, index_);
    if (number_ == kMaxUInt32) {
      return holder->IsJSTypedArray() ? INTEGER_INDEXED_EXOTIC : NOT_FOUND;
    }
    property_details_ = accessor->GetDetails(js_object, number_);
  } else if (!map->is_dictionary_map()) {
    DescriptorArray* descriptors = map->instance_descriptors();
    int number = descriptors->SearchWithCache(isolate_, *name_, map);
    if (number == DescriptorArray::kNotFound) return NotFound(holder);
    number_ = static_cast<uint32_t>(number);
    property_details_ = descriptors->GetDetails(number_);
  } else {
    NameDictionary* dict = holder->property_dictionary();
    int number = dict->FindEntry(name_);
    if (number == NameDictionary::kNotFound) return NotFound(holder);
    number_ = static_cast<uint32_t>(number);
    property_details_ = dict->DetailsAt(number_);
  }
  has_property_ = true;
  switch (property_details_.kind()) {
    case v8::internal::kData:
      return DATA;
    case v8::internal::kAccessor:
      return ACCESSOR;
  }

  UNREACHABLE();
  return state_;
}

Handle<InterceptorInfo> LookupIterator::GetInterceptorForFailedAccessCheck()
    const {
  DCHECK_EQ(ACCESS_CHECK, state_);
  DisallowHeapAllocation no_gc;
  AccessCheckInfo* access_check_info =
      AccessCheckInfo::Get(isolate_, Handle<JSObject>::cast(holder_));
  if (access_check_info) {
    Object* interceptor = IsElement() ? access_check_info->indexed_interceptor()
                                      : access_check_info->named_interceptor();
    if (interceptor) {
      return handle(InterceptorInfo::cast(interceptor), isolate_);
    }
  }
  return Handle<InterceptorInfo>();
}

bool LookupIterator::TryLookupCachedProperty() {
  return state() == LookupIterator::ACCESSOR &&
         GetAccessors()->IsAccessorPair() && LookupCachedProperty();
}

bool LookupIterator::LookupCachedProperty() {
  DCHECK_EQ(state(), LookupIterator::ACCESSOR);
  DCHECK(GetAccessors()->IsAccessorPair());

  AccessorPair* accessor_pair = AccessorPair::cast(*GetAccessors());
  Handle<Object> getter(accessor_pair->getter(), isolate());
  MaybeHandle<Name> maybe_name =
      FunctionTemplateInfo::TryGetCachedPropertyName(isolate(), getter);
  if (maybe_name.is_null()) return false;

  // We have found a cached property! Modify the iterator accordingly.
  name_ = maybe_name.ToHandleChecked();
  Restart();
  CHECK_EQ(state(), LookupIterator::DATA);
  return true;
}

}  // namespace internal
}  // namespace v8
