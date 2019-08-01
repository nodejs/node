// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_LOOKUP_INL_H_
#define V8_OBJECTS_LOOKUP_INL_H_

#include "src/objects/lookup.h"

#include "src/handles/handles-inl.h"
#include "src/heap/factory-inl.h"
#include "src/objects/api-callbacks.h"
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
    : configuration_(ComputeConfiguration(configuration, name)),
      interceptor_state_(InterceptorState::kUninitialized),
      property_details_(PropertyDetails::Empty()),
      isolate_(isolate),
      name_(isolate_->factory()->InternalizeName(name)),
      receiver_(receiver),
      initial_holder_(holder),
      // kMaxUInt32 isn't a valid index.
      index_(kMaxUInt32),
      number_(static_cast<uint32_t>(DescriptorArray::kNotFound)) {
#ifdef DEBUG
  uint32_t index;  // Assert that the name is not an array index.
  DCHECK(!name->AsArrayIndex(&index));
#endif  // DEBUG
  Start<false>();
}

LookupIterator::LookupIterator(Isolate* isolate, Handle<Object> receiver,
                               uint32_t index, Configuration configuration)
    : LookupIterator(isolate, receiver, index,
                     GetRoot(isolate, receiver, index), configuration) {}

LookupIterator LookupIterator::PropertyOrElement(Isolate* isolate,
                                                 Handle<Object> receiver,
                                                 Handle<Name> name,
                                                 Handle<JSReceiver> holder,
                                                 Configuration configuration) {
  uint32_t index;
  if (name->AsArrayIndex(&index)) {
    LookupIterator it =
        LookupIterator(isolate, receiver, index, holder, configuration);
    it.name_ = name;
    return it;
  }
  return LookupIterator(isolate, receiver, name, holder, configuration);
}

LookupIterator LookupIterator::PropertyOrElement(Isolate* isolate,
                                                 Handle<Object> receiver,
                                                 Handle<Name> name,
                                                 Configuration configuration) {
  uint32_t index;
  if (name->AsArrayIndex(&index)) {
    LookupIterator it = LookupIterator(isolate, receiver, index, configuration);
    it.name_ = name;
    return it;
  }
  return LookupIterator(isolate, receiver, name, configuration);
}

Handle<Name> LookupIterator::GetName() {
  if (name_.is_null()) {
    DCHECK(IsElement());
    name_ = factory()->Uint32ToString(index_);
  }
  return name_;
}

bool LookupIterator::is_dictionary_holder() const {
  return !holder_->HasFastProperties();
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
  return !receiver->map().is_extensible() &&
         (IsElement() || !name_->IsPrivate());
}

bool LookupIterator::IsCacheableTransition() {
  DCHECK_EQ(TRANSITION, state_);
  return transition_->IsPropertyCell() ||
         (transition_map()->is_dictionary_map() &&
          !GetStoreTarget<JSReceiver>()->HasFastProperties()) ||
         transition_map()->GetBackPointer().IsMap();
}

void LookupIterator::UpdateProtector() {
  if (IsElement()) return;
  // This list must be kept in sync with
  // CodeStubAssembler::CheckForAssociatedProtector!
  ReadOnlyRoots roots(heap());
  if (*name_ == roots.is_concat_spreadable_symbol() ||
      *name_ == roots.constructor_string() || *name_ == roots.next_string() ||
      *name_ == roots.species_symbol() || *name_ == roots.iterator_symbol() ||
      *name_ == roots.resolve_string() || *name_ == roots.then_string()) {
    InternalUpdateProtector();
  }
}

int LookupIterator::descriptor_number() const {
  DCHECK(!IsElement());
  DCHECK(has_property_);
  DCHECK(holder_->HasFastProperties());
  return number_;
}

int LookupIterator::dictionary_entry() const {
  DCHECK(!IsElement());
  DCHECK(has_property_);
  DCHECK(!holder_->HasFastProperties());
  return number_;
}

LookupIterator::Configuration LookupIterator::ComputeConfiguration(
    Configuration configuration, Handle<Name> name) {
  return name->IsPrivate() ? OWN_SKIP_INTERCEPTOR : configuration;
}

Handle<JSReceiver> LookupIterator::GetRoot(Isolate* isolate,
                                           Handle<Object> receiver,
                                           uint32_t index) {
  if (receiver->IsJSReceiver()) return Handle<JSReceiver>::cast(receiver);
  return GetRootForNonJSReceiver(isolate, receiver, index);
}

template <class T>
Handle<T> LookupIterator::GetStoreTarget() const {
  DCHECK(receiver_->IsJSReceiver());
  if (receiver_->IsJSGlobalProxy()) {
    Map map = JSGlobalProxy::cast(*receiver_).map();
    if (map.has_hidden_prototype()) {
      return handle(JSGlobalObject::cast(map.prototype()), isolate_);
    }
  }
  return Handle<T>::cast(receiver_);
}

template <bool is_element>
InterceptorInfo LookupIterator::GetInterceptor(JSObject holder) {
  return is_element ? holder.GetIndexedInterceptor()
                    : holder.GetNamedInterceptor();
}

inline Handle<InterceptorInfo> LookupIterator::GetInterceptor() const {
  DCHECK_EQ(INTERCEPTOR, state_);
  InterceptorInfo result =
      IsElement() ? GetInterceptor<true>(JSObject::cast(*holder_))
                  : GetInterceptor<false>(JSObject::cast(*holder_));
  return handle(result, isolate_);
}

}  // namespace internal
}  // namespace v8

#endif  // V8_OBJECTS_LOOKUP_INL_H_
