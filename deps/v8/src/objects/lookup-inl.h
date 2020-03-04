// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_LOOKUP_INL_H_
#define V8_OBJECTS_LOOKUP_INL_H_

#include "src/objects/lookup.h"

#include "src/handles/handles-inl.h"
#include "src/heap/factory-inl.h"
#include "src/objects/api-callbacks.h"
#include "src/objects/internal-index.h"
#include "src/objects/map-inl.h"
#include "src/objects/name-inl.h"
#include "src/objects/objects-inl.h"

namespace v8 {
namespace internal {

LookupIterator::LookupIterator(Isolate* isolate, Handle<Object> receiver,
                               Handle<Name> name, Configuration configuration)
    : LookupIterator(isolate, receiver, name, GetRoot(isolate, receiver),
                     configuration) {}

LookupIterator::LookupIterator(Handle<Object> receiver, Handle<Name> name,
                               Handle<JSReceiver> holder,
                               Configuration configuration)
    : LookupIterator(holder->GetIsolate(), receiver, name, holder,
                     configuration) {}

LookupIterator::LookupIterator(Isolate* isolate, Handle<Object> receiver,
                               Handle<Name> name, Handle<JSReceiver> holder,
                               Configuration configuration)
    : configuration_(ComputeConfiguration(isolate, configuration, name)),
      interceptor_state_(InterceptorState::kUninitialized),
      property_details_(PropertyDetails::Empty()),
      isolate_(isolate),
      name_(isolate_->factory()->InternalizeName(name)),
      receiver_(receiver),
      initial_holder_(holder),
      index_(kInvalidIndex) {
#ifdef DEBUG
  // Assert that the name is not an index.
  // If we're not looking at the prototype chain and the initial holder is
  // not a typed array, then this means "array index", otherwise we need to
  // ensure the full generality so that typed arrays are handled correctly.
  if (!check_prototype_chain() && !holder->IsJSTypedArray()) {
    uint32_t index;
    DCHECK(!name->AsArrayIndex(&index));
  } else {
    size_t index;
    DCHECK(!name->AsIntegerIndex(&index));
  }
#endif  // DEBUG
  Start<false>();
}

LookupIterator::LookupIterator(Isolate* isolate, Handle<Object> receiver,
                               size_t index, Configuration configuration,
                               Handle<Name> key_as_string)
    : LookupIterator(isolate, receiver, index,
                     GetRoot(isolate, receiver, index), configuration,
                     key_as_string) {}

LookupIterator::LookupIterator(Isolate* isolate, Handle<Object> receiver,
                               size_t index, Handle<JSReceiver> holder,
                               Configuration configuration,
                               Handle<Name> key_as_string)
    : configuration_(configuration),
      interceptor_state_(InterceptorState::kUninitialized),
      property_details_(PropertyDetails::Empty()),
      isolate_(isolate),
      receiver_(receiver),
      initial_holder_(holder),
      index_(index) {
  DCHECK_NE(index, kInvalidIndex);
  // If we're not looking at a TypedArray, we will need the key represented
  // as an internalized string.
  if (index_ > JSArray::kMaxArrayIndex && !holder->IsJSTypedArray()) {
    if (key_as_string.is_null()) {
      key_as_string = isolate->factory()->SizeToString(index_);
    }
    name_ = isolate->factory()->InternalizeName(key_as_string);
  } else if (!key_as_string.is_null() &&
             key_as_string->IsInternalizedString()) {
    // Even for TypedArrays: if we have a name, keep it. ICs will need it.
    name_ = key_as_string;
  }
  Start<true>();
}

LookupIterator LookupIterator::PropertyOrElement(Isolate* isolate,
                                                 Handle<Object> receiver,
                                                 Handle<Name> name,
                                                 Handle<JSReceiver> holder,
                                                 Configuration configuration) {
  size_t index;
  if (name->AsIntegerIndex(&index)) {
    return LookupIterator(isolate, receiver, index, holder, configuration,
                          name);
  }
  return LookupIterator(isolate, receiver, name, holder, configuration);
}

LookupIterator LookupIterator::PropertyOrElement(Isolate* isolate,
                                                 Handle<Object> receiver,
                                                 Handle<Name> name,
                                                 Configuration configuration) {
  size_t index;
  if (name->AsIntegerIndex(&index)) {
    return LookupIterator(isolate, receiver, index, configuration, name);
  }
  return LookupIterator(isolate, receiver, name, configuration);
}

Handle<Name> LookupIterator::name() const {
  DCHECK(!IsElement(*holder_));
  return name_;
}

Handle<Name> LookupIterator::GetName() {
  if (name_.is_null()) {
    DCHECK(IsElement());
    name_ = factory()->SizeToString(index_);
  }
  return name_;
}

bool LookupIterator::IsElement(JSReceiver object) const {
  return index_ <= JSArray::kMaxArrayIndex ||
         (index_ != kInvalidIndex && object.map().has_typed_array_elements());
}

bool LookupIterator::is_dictionary_holder() const {
  return !holder_->HasFastProperties(isolate_);
}

Handle<Map> LookupIterator::transition_map() const {
  DCHECK_EQ(TRANSITION, state_);
  return Handle<Map>::cast(transition_);
}

Handle<PropertyCell> LookupIterator::transition_cell() const {
  DCHECK_EQ(TRANSITION, state_);
  return Handle<PropertyCell>::cast(transition_);
}

template <class T>
Handle<T> LookupIterator::GetHolder() const {
  DCHECK(IsFound());
  return Handle<T>::cast(holder_);
}

bool LookupIterator::ExtendingNonExtensible(Handle<JSReceiver> receiver) {
  DCHECK(receiver.is_identical_to(GetStoreTarget<JSReceiver>()));
  return !receiver->map(isolate_).is_extensible() &&
         (IsElement() || !name_->IsPrivate(isolate_));
}

bool LookupIterator::IsCacheableTransition() {
  DCHECK_EQ(TRANSITION, state_);
  return transition_->IsPropertyCell(isolate_) ||
         (transition_map()->is_dictionary_map() &&
          !GetStoreTarget<JSReceiver>()->HasFastProperties(isolate_)) ||
         transition_map()->GetBackPointer(isolate_).IsMap(isolate_);
}

// static
void LookupIterator::UpdateProtector(Isolate* isolate, Handle<Object> receiver,
                                     Handle<Name> name) {
  // This list must be kept in sync with
  // CodeStubAssembler::CheckForAssociatedProtector!
  ReadOnlyRoots roots(isolate);
  if (*name == roots.is_concat_spreadable_symbol() ||
      *name == roots.constructor_string() || *name == roots.next_string() ||
      *name == roots.species_symbol() || *name == roots.iterator_symbol() ||
      *name == roots.resolve_string() || *name == roots.then_string()) {
    InternalUpdateProtector(isolate, receiver, name);
  }
}

void LookupIterator::UpdateProtector() {
  if (IsElement()) return;
  UpdateProtector(isolate_, receiver_, name_);
}

InternalIndex LookupIterator::descriptor_number() const {
  DCHECK(!IsElement(*holder_));
  DCHECK(has_property_);
  DCHECK(holder_->HasFastProperties(isolate_));
  return number_;
}

InternalIndex LookupIterator::dictionary_entry() const {
  DCHECK(!IsElement(*holder_));
  DCHECK(has_property_);
  DCHECK(!holder_->HasFastProperties(isolate_));
  return number_;
}

// static
LookupIterator::Configuration LookupIterator::ComputeConfiguration(
    Isolate* isolate, Configuration configuration, Handle<Name> name) {
  return name->IsPrivate(isolate) ? OWN_SKIP_INTERCEPTOR : configuration;
}

// static
Handle<JSReceiver> LookupIterator::GetRoot(Isolate* isolate,
                                           Handle<Object> receiver,
                                           size_t index) {
  if (receiver->IsJSReceiver(isolate)) {
    return Handle<JSReceiver>::cast(receiver);
  }
  return GetRootForNonJSReceiver(isolate, receiver, index);
}

template <class T>
Handle<T> LookupIterator::GetStoreTarget() const {
  DCHECK(receiver_->IsJSReceiver(isolate_));
  if (receiver_->IsJSGlobalProxy(isolate_)) {
    HeapObject prototype =
        JSGlobalProxy::cast(*receiver_).map(isolate_).prototype(isolate_);
    if (prototype.IsJSGlobalObject(isolate_)) {
      return handle(JSGlobalObject::cast(prototype), isolate_);
    }
  }
  return Handle<T>::cast(receiver_);
}

template <bool is_element>
InterceptorInfo LookupIterator::GetInterceptor(JSObject holder) const {
  if (is_element && index_ <= JSArray::kMaxArrayIndex) {
    return holder.GetIndexedInterceptor(isolate_);
  } else {
    return holder.GetNamedInterceptor(isolate_);
  }
}

inline Handle<InterceptorInfo> LookupIterator::GetInterceptor() const {
  DCHECK_EQ(INTERCEPTOR, state_);
  JSObject holder = JSObject::cast(*holder_);
  InterceptorInfo result = IsElement(holder) ? GetInterceptor<true>(holder)
                                             : GetInterceptor<false>(holder);
  return handle(result, isolate_);
}

}  // namespace internal
}  // namespace v8

#endif  // V8_OBJECTS_LOOKUP_INL_H_
