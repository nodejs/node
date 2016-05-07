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


void LookupIterator::Next() {
  DCHECK_NE(JSPROXY, state_);
  DCHECK_NE(TRANSITION, state_);
  DisallowHeapAllocation no_gc;
  has_property_ = false;

  JSReceiver* holder = *holder_;
  Map* map = holder->map();

  // Perform lookup on current holder.
  state_ = LookupInHolder(map, holder);
  if (IsFound()) return;

  // Continue lookup if lookup on current holder failed.
  do {
    JSReceiver* maybe_holder = NextHolder(map);
    if (maybe_holder == nullptr) {
      if (interceptor_state_ == InterceptorState::kSkipNonMasking) {
        RestartLookupForNonMaskingInterceptors();
        return;
      }
      break;
    }
    holder = maybe_holder;
    map = holder->map();
    state_ = LookupInHolder(map, holder);
  } while (!IsFound());

  if (holder != *holder_) holder_ = handle(holder, isolate_);
}


void LookupIterator::RestartInternal(InterceptorState interceptor_state) {
  state_ = NOT_FOUND;
  interceptor_state_ = interceptor_state;
  property_details_ = PropertyDetails::Empty();
  holder_ = initial_holder_;
  number_ = DescriptorArray::kNotFound;
  Next();
}


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
  auto root = handle(receiver->GetRootMap(isolate)->prototype(), isolate);
  if (root->IsNull()) {
    unsigned int magic = 0xbbbbbbbb;
    isolate->PushStackTraceAndDie(magic, *receiver, NULL, magic);
  }
  return Handle<JSReceiver>::cast(root);
}


Handle<Map> LookupIterator::GetReceiverMap() const {
  if (receiver_->IsNumber()) return factory()->heap_number_map();
  return handle(Handle<HeapObject>::cast(receiver_)->map(), isolate_);
}


Handle<JSObject> LookupIterator::GetStoreTarget() const {
  if (receiver_->IsJSGlobalProxy()) {
    Object* prototype = JSGlobalProxy::cast(*receiver_)->map()->prototype();
    if (!prototype->IsNull()) {
      return handle(JSGlobalObject::cast(prototype), isolate_);
    }
  }
  return Handle<JSObject>::cast(receiver_);
}


bool LookupIterator::HasAccess() const {
  DCHECK_EQ(ACCESS_CHECK, state_);
  return isolate_->MayAccess(handle(isolate_->context()),
                             GetHolder<JSObject>());
}


void LookupIterator::ReloadPropertyInformation() {
  state_ = BEFORE_PROPERTY;
  interceptor_state_ = InterceptorState::kUninitialized;
  state_ = LookupInHolder(holder_->map(), *holder_);
  DCHECK(IsFound() || !holder_->HasFastProperties());
}

bool LookupIterator::HolderIsInContextIndex(uint32_t index) const {
  DisallowHeapAllocation no_gc;

  Object* context = heap()->native_contexts_list();
  while (!context->IsUndefined()) {
    Context* current_context = Context::cast(context);
    if (current_context->get(index) == *holder_) {
      return true;
    }
    context = current_context->get(Context::NEXT_CONTEXT_LINK);
  }
  return false;
}

void LookupIterator::UpdateProtector() {
  if (!FLAG_harmony_species) return;

  if (IsElement()) return;
  if (isolate_->bootstrapper()->IsActive()) return;
  if (!isolate_->IsArraySpeciesLookupChainIntact()) return;

  if (*name_ == *isolate_->factory()->constructor_string()) {
    // Setting the constructor property could change an instance's @@species
    if (holder_->IsJSArray()) {
      isolate_->CountUsage(
          v8::Isolate::UseCounterFeature::kArrayInstanceConstructorModified);
      isolate_->InvalidateArraySpeciesProtector();
    } else if (holder_->map()->is_prototype_map()) {
      // Setting the constructor of Array.prototype of any realm also needs
      // to invalidate the species protector
      if (HolderIsInContextIndex(Context::INITIAL_ARRAY_PROTOTYPE_INDEX)) {
        isolate_->CountUsage(v8::Isolate::UseCounterFeature::
                                 kArrayPrototypeConstructorModified);
        isolate_->InvalidateArraySpeciesProtector();
      }
    }
  } else if (*name_ == *isolate_->factory()->species_symbol()) {
    // Setting the Symbol.species property of any Array constructor invalidates
    // the species protector
    if (HolderIsInContextIndex(Context::ARRAY_FUNCTION_INDEX)) {
      isolate_->CountUsage(
          v8::Isolate::UseCounterFeature::kArraySpeciesModified);
      isolate_->InvalidateArraySpeciesProtector();
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

  if (!holder->HasFastProperties()) return;

  Handle<Map> old_map(holder->map(), isolate_);
  Handle<Map> new_map =
      Map::PrepareForDataProperty(old_map, descriptor_number(), value);

  if (old_map.is_identical_to(new_map)) {
    // Update the property details if the representation was None.
    if (representation().IsNone()) {
      property_details_ =
          new_map->instance_descriptors()->GetDetails(descriptor_number());
    }
    return;
  }

  JSObject::MigrateToMap(holder, new_map);
  ReloadPropertyInformation();
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
  } else if (!holder->HasFastProperties()) {
    PropertyDetails details(attributes, v8::internal::DATA, 0,
                            PropertyCellType::kMutable);
    JSObject::SetNormalizedProperty(holder, name(), value, details);
  } else {
    Handle<Map> old_map(holder->map(), isolate_);
    Handle<Map> new_map = Map::ReconfigureExistingProperty(
        old_map, descriptor_number(), i::kData, attributes);
    new_map = Map::PrepareForDataProperty(new_map, descriptor_number(), value);
    JSObject::MigrateToMap(holder, new_map);
  }

  ReloadPropertyInformation();
  WriteDataValue(value);

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
      auto cell = JSGlobalObject::EnsurePropertyCell(
          Handle<JSGlobalObject>::cast(receiver), name());
      DCHECK(cell->value()->IsTheHole());
      transition_ = cell;
    } else {
      transition_ = map;
    }
    return;
  }

  Handle<Map> transition =
      Map::TransitionToDataProperty(map, name_, value, attributes, store_mode);
  state_ = TRANSITION;
  transition_ = transition;

  if (!transition->is_dictionary_map()) {
    property_details_ = transition->GetLastDescriptorDetails();
    has_property_ = true;
  }
}

void LookupIterator::ApplyTransitionToDataProperty(Handle<JSObject> receiver) {
  DCHECK_EQ(TRANSITION, state_);

  DCHECK(receiver.is_identical_to(GetStoreTarget()));

  if (receiver->IsJSGlobalObject()) return;
  holder_ = receiver;
  Handle<Map> transition = transition_map();
  bool simple_transition = transition->GetBackPointer() == receiver->map();
  JSObject::MigrateToMap(receiver, transition);

  if (simple_transition) {
    int number = transition->LastAdded();
    number_ = static_cast<uint32_t>(number);
    property_details_ = transition->GetLastDescriptorDetails();
    state_ = DATA;
  } else {
    ReloadPropertyInformation();
  }
}


void LookupIterator::Delete() {
  Handle<JSReceiver> holder = Handle<JSReceiver>::cast(holder_);
  if (IsElement()) {
    Handle<JSObject> object = Handle<JSObject>::cast(holder);
    ElementsAccessor* accessor = object->GetElementsAccessor();
    accessor->Delete(object, number_);
  } else {
    PropertyNormalizationMode mode = holder->map()->is_prototype_map()
                                         ? KEEP_INOBJECT_PROPERTIES
                                         : CLEAR_INOBJECT_PROPERTIES;

    if (holder->HasFastProperties()) {
      JSObject::NormalizeProperties(Handle<JSObject>::cast(holder), mode, 0,
                                    "DeletingProperty");
      ReloadPropertyInformation();
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
    AccessorComponent component, Handle<Object> accessor,
    PropertyAttributes attributes) {
  DCHECK(!accessor->IsNull());
  // Can only be called when the receiver is a JSObject. JSProxy has to be
  // handled via a trap. Adding properties to primitive values is not
  // observable.
  Handle<JSObject> receiver = GetStoreTarget();

  if (!IsElement() && !receiver->map()->is_dictionary_map()) {
    holder_ = receiver;
    Handle<Map> old_map(receiver->map(), isolate_);
    Handle<Map> new_map = Map::TransitionToAccessorProperty(
        old_map, name_, component, accessor, attributes);
    JSObject::MigrateToMap(receiver, new_map);

    ReloadPropertyInformation();

    if (!new_map->is_dictionary_map()) return;
  }

  Handle<AccessorPair> pair;
  if (state() == ACCESSOR && GetAccessors()->IsAccessorPair()) {
    pair = Handle<AccessorPair>::cast(GetAccessors());
    // If the component and attributes are identical, nothing has to be done.
    if (pair->get(component) == *accessor) {
      if (property_details().attributes() == attributes) return;
    } else {
      pair = AccessorPair::Copy(pair);
      pair->set(component, *accessor);
    }
  } else {
    pair = factory()->NewAccessorPair();
    pair->set(component, *accessor);
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

  PropertyDetails details(attributes, ACCESSOR_CONSTANT, 0,
                          PropertyCellType::kMutable);

  if (IsElement()) {
    // TODO(verwaest): Move code into the element accessor.
    Handle<SeededNumberDictionary> dictionary =
        JSObject::NormalizeElements(receiver);

    // We unconditionally pass used_as_prototype=false here because the call
    // to RequireSlowElements takes care of the required IC clearing and
    // we don't want to walk the heap twice.
    dictionary =
        SeededNumberDictionary::Set(dictionary, index_, pair, details, false);
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
  } else {
    PropertyNormalizationMode mode = receiver->map()->is_prototype_map()
                                         ? KEEP_INOBJECT_PROPERTIES
                                         : CLEAR_INOBJECT_PROPERTIES;
    // Normalize object to make this operation simple.
    JSObject::NormalizeProperties(receiver, mode, 0,
                                  "TransitionToAccessorPair");

    JSObject::SetNormalizedProperty(receiver, name_, pair, details);
    JSObject::ReoptimizeIfPrototype(receiver);
  }

  ReloadPropertyInformation();
}


bool LookupIterator::HolderIsReceiverOrHiddenPrototype() const {
  DCHECK(has_property_ || state_ == INTERCEPTOR || state_ == JSPROXY);
  // Optimization that only works if configuration_ is not mutable.
  if (!check_prototype_chain()) return true;
  DisallowHeapAllocation no_gc;
  if (!receiver_->IsJSReceiver()) return false;
  JSReceiver* current = JSReceiver::cast(*receiver_);
  JSReceiver* object = *holder_;
  if (current == object) return true;
  if (!current->map()->has_hidden_prototype()) return false;
  // JSProxy do not occur as hidden prototypes.
  if (current->IsJSProxy()) return false;
  PrototypeIterator iter(isolate(), current,
                         PrototypeIterator::START_AT_PROTOTYPE,
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
  } else if (property_details_.type() == v8::internal::DATA) {
    Handle<JSObject> holder = GetHolder<JSObject>();
    FieldIndex field_index = FieldIndex::ForDescriptor(holder->map(), number_);
    return JSObject::FastPropertyAt(holder, property_details_.representation(),
                                    field_index);
  } else {
    result = holder_->map()->instance_descriptors()->GetValue(number_);
  }
  return handle(result, isolate_);
}


int LookupIterator::GetAccessorIndex() const {
  DCHECK(has_property_);
  DCHECK(holder_->HasFastProperties());
  DCHECK_EQ(v8::internal::ACCESSOR_CONSTANT, property_details_.type());
  return descriptor_number();
}


int LookupIterator::GetConstantIndex() const {
  DCHECK(has_property_);
  DCHECK(holder_->HasFastProperties());
  DCHECK_EQ(v8::internal::DATA_CONSTANT, property_details_.type());
  DCHECK(!IsElement());
  return descriptor_number();
}


FieldIndex LookupIterator::GetFieldIndex() const {
  DCHECK(has_property_);
  DCHECK(holder_->HasFastProperties());
  DCHECK_EQ(v8::internal::DATA, property_details_.type());
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
  DCHECK_EQ(v8::internal::DATA, property_details_.type());
  return handle(
      holder_->map()->instance_descriptors()->GetFieldType(descriptor_number()),
      isolate_);
}


Handle<PropertyCell> LookupIterator::GetPropertyCell() const {
  DCHECK(!IsElement());
  Handle<JSObject> holder = GetHolder<JSObject>();
  Handle<JSGlobalObject> global = Handle<JSGlobalObject>::cast(holder);
  Object* value = global->global_dictionary()->ValueAt(dictionary_entry());
  DCHECK(value->IsPropertyCell());
  return handle(PropertyCell::cast(value));
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


void LookupIterator::WriteDataValue(Handle<Object> value) {
  DCHECK_EQ(DATA, state_);
  Handle<JSReceiver> holder = GetHolder<JSReceiver>();
  if (IsElement()) {
    Handle<JSObject> object = Handle<JSObject>::cast(holder);
    ElementsAccessor* accessor = object->GetElementsAccessor();
    accessor->Set(object, number_, *value);
  } else if (holder->HasFastProperties()) {
    if (property_details_.type() == v8::internal::DATA) {
      JSObject::cast(*holder)->WriteToField(descriptor_number(),
                                            property_details_, *value);
    } else {
      DCHECK_EQ(v8::internal::DATA_CONSTANT, property_details_.type());
    }
  } else if (holder->IsJSGlobalObject()) {
    Handle<GlobalDictionary> property_dictionary =
        handle(JSObject::cast(*holder)->global_dictionary());
    PropertyCell::UpdateCell(property_dictionary, dictionary_entry(), value,
                             property_details_);
  } else {
    NameDictionary* property_dictionary = holder->property_dictionary();
    property_dictionary->ValueAtPut(dictionary_entry(), *value);
  }
}


bool LookupIterator::HasInterceptor(Map* map) const {
  if (IsElement()) return map->has_indexed_interceptor();
  return map->has_named_interceptor();
}


bool LookupIterator::SkipInterceptor(JSObject* holder) {
  auto info = GetInterceptor(holder);
  // TODO(dcarney): check for symbol/can_intercept_symbols here as well.
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
  if (!map->prototype()->IsJSReceiver()) return NULL;

  DCHECK(!map->IsJSGlobalProxyMap() || map->has_hidden_prototype());

  if (!check_prototype_chain() &&
      !(check_hidden() && map->has_hidden_prototype()) &&
      // Always lookup behind the JSGlobalProxy into the JSGlobalObject, even
      // when not checking other hidden prototypes.
      !map->IsJSGlobalProxyMap()) {
    return NULL;
  }

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

LookupIterator::State LookupIterator::LookupInHolder(Map* const map,
                                                     JSReceiver* const holder) {
  STATIC_ASSERT(INTERCEPTOR == BEFORE_PROPERTY);
  DisallowHeapAllocation no_gc;
  if (interceptor_state_ == InterceptorState::kProcessNonMasking) {
    return LookupNonMaskingInterceptorInHolder(map, holder);
  }
  switch (state_) {
    case NOT_FOUND:
      if (map->IsJSProxyMap()) {
        if (IsElement() || !name_->IsPrivate()) return JSPROXY;
      }
      if (map->is_access_check_needed()) {
        if (IsElement() || !name_->IsPrivate()) return ACCESS_CHECK;
      }
    // Fall through.
    case ACCESS_CHECK:
      if (check_interceptor() && HasInterceptor(map) &&
          !SkipInterceptor(JSObject::cast(holder))) {
        if (IsElement() || !name_->IsPrivate()) return INTERCEPTOR;
      }
    // Fall through.
    case INTERCEPTOR:
      if (IsElement()) {
        JSObject* js_object = JSObject::cast(holder);
        ElementsAccessor* accessor = js_object->GetElementsAccessor();
        FixedArrayBase* backing_store = js_object->elements();
        number_ = accessor->GetEntryForIndex(js_object, backing_store, index_);
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
      } else if (map->IsJSGlobalObjectMap()) {
        GlobalDictionary* dict = JSObject::cast(holder)->global_dictionary();
        int number = dict->FindEntry(name_);
        if (number == GlobalDictionary::kNotFound) return NOT_FOUND;
        number_ = static_cast<uint32_t>(number);
        DCHECK(dict->ValueAt(number_)->IsPropertyCell());
        PropertyCell* cell = PropertyCell::cast(dict->ValueAt(number_));
        if (cell->value()->IsTheHole()) return NOT_FOUND;
        property_details_ = cell->property_details();
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
    case ACCESSOR:
    case DATA:
      return NOT_FOUND;
    case INTEGER_INDEXED_EXOTIC:
    case JSPROXY:
    case TRANSITION:
      UNREACHABLE();
  }
  UNREACHABLE();
  return state_;
}


LookupIterator::State LookupIterator::LookupNonMaskingInterceptorInHolder(
    Map* const map, JSReceiver* const holder) {
  switch (state_) {
    case NOT_FOUND:
      if (check_interceptor() && HasInterceptor(map) &&
          !SkipInterceptor(JSObject::cast(holder))) {
        return INTERCEPTOR;
      }
    // Fall through.
    default:
      return NOT_FOUND;
  }
  UNREACHABLE();
  return state_;
}

}  // namespace internal
}  // namespace v8
