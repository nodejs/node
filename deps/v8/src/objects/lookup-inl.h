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
    : LookupIterator(isolate, receiver, name, kInvalidIndex,
                     GetRoot(isolate, receiver, kInvalidIndex), configuration) {
}

LookupIterator::LookupIterator(Isolate* isolate, Handle<Object> receiver,
                               Handle<Name> name, Handle<JSReceiver> holder,
                               Configuration configuration)
    : LookupIterator(isolate, receiver, name, kInvalidIndex, holder,
                     configuration) {}

LookupIterator::LookupIterator(Isolate* isolate, Handle<Object> receiver,
                               size_t index, Configuration configuration)
    : LookupIterator(isolate, receiver, Handle<Name>(), index,
                     GetRoot(isolate, receiver, index), configuration) {
  DCHECK_NE(index, kInvalidIndex);
}

LookupIterator::LookupIterator(Isolate* isolate, Handle<Object> receiver,
                               size_t index, Handle<JSReceiver> holder,
                               Configuration configuration)
    : LookupIterator(isolate, receiver, Handle<Name>(), index, holder,
                     configuration) {
  DCHECK_NE(index, kInvalidIndex);
}

LookupIterator::LookupIterator(Isolate* isolate, Handle<Object> receiver,
                               const Key& key, Configuration configuration)
    : LookupIterator(isolate, receiver, key.name(), key.index(),
                     GetRoot(isolate, receiver, key.index()), configuration) {}

LookupIterator::LookupIterator(Isolate* isolate, Handle<Object> receiver,
                               const Key& key, Handle<JSReceiver> holder,
                               Configuration configuration)
    : LookupIterator(isolate, receiver, key.name(), key.index(), holder,
                     configuration) {}

// This private constructor is the central bottleneck that all the other
// constructors use.
LookupIterator::LookupIterator(Isolate* isolate, Handle<Object> receiver,
                               Handle<Name> name, size_t index,
                               Handle<JSReceiver> holder,
                               Configuration configuration)
    : configuration_(ComputeConfiguration(isolate, configuration, name)),
      isolate_(isolate),
      name_(name),
      receiver_(receiver),
      initial_holder_(holder),
      index_(index) {
  if (IsElement()) {
    // If we're not looking at a TypedArray, we will need the key represented
    // as an internalized string.
    if (index_ > JSArray::kMaxArrayIndex && !holder->IsJSTypedArray()) {
      if (name_.is_null()) {
        name_ = isolate->factory()->SizeToString(index_);
      }
      name_ = isolate->factory()->InternalizeName(name_);
    } else if (!name_.is_null() && !name_->IsInternalizedString()) {
      // Maintain the invariant that if name_ is present, it is internalized.
      name_ = Handle<Name>();
    }
    Start<true>();
  } else {
    name_ = isolate->factory()->InternalizeName(name_);
#ifdef DEBUG
    // Assert that the name is not an index.
    // If we're not looking at the prototype chain and the initial holder is
    // not a typed array, then this means "array index", otherwise we need to
    // ensure the full generality so that typed arrays are handled correctly.
    if (!check_prototype_chain() && !holder->IsJSTypedArray()) {
      uint32_t index;
      DCHECK(!name_->AsArrayIndex(&index));
    } else {
      size_t index;
      DCHECK(!name_->AsIntegerIndex(&index));
    }
#endif  // DEBUG
    Start<false>();
  }
}

LookupIterator::Key::Key(Isolate* isolate, double index) {
  DCHECK_EQ(index, static_cast<uint64_t>(index));
#if V8_TARGET_ARCH_32_BIT
  if (index <= JSArray::kMaxArrayIndex) {
    index_ = static_cast<size_t>(index);
  } else {
    index_ = LookupIterator::kInvalidIndex;
    name_ = isolate->factory()->InternalizeString(
        isolate->factory()->HeapNumberToString(
            isolate->factory()->NewHeapNumber(index), index));
  }
#else
  index_ = static_cast<size_t>(index);
#endif
}

LookupIterator::Key::Key(Isolate* isolate, Handle<Name> name) {
  if (name->AsIntegerIndex(&index_)) {
    name_ = name;
  } else {
    index_ = LookupIterator::kInvalidIndex;
    name_ = isolate->factory()->InternalizeName(name);
  }
}

LookupIterator::Key::Key(Isolate* isolate, Handle<Object> valid_key) {
  DCHECK(valid_key->IsName() || valid_key->IsNumber());
  if (valid_key->ToIntegerIndex(&index_)) return;
  if (valid_key->IsNumber()) {
    // Negative or out of range -> treat as named property.
    valid_key = isolate->factory()->NumberToString(valid_key);
  }
  DCHECK(valid_key->IsName());
  name_ = Handle<Name>::cast(valid_key);
  if (!name_->AsIntegerIndex(&index_)) {
    index_ = LookupIterator::kInvalidIndex;
    name_ = isolate->factory()->InternalizeName(name_);
  }
}

Handle<Name> LookupIterator::Key::GetName(Isolate* isolate) {
  if (name_.is_null()) {
    DCHECK(is_element());
    name_ = isolate->factory()->SizeToString(index_);
  }
  return name_;
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
  return (!name.is_null() && name->IsPrivate(isolate)) ? OWN_SKIP_INTERCEPTOR
                                                       : configuration;
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
