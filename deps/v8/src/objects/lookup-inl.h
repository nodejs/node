// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_LOOKUP_INL_H_
#define V8_OBJECTS_LOOKUP_INL_H_

#include "src/objects/lookup.h"

// Include other inline headers *after* including lookup.h, such that e.g. the
// definition of LookupIterator is available (and this comment prevents
// clang-format from merging that include into the following ones).
#include "src/handles/handles-inl.h"
#include "src/heap/factory-inl.h"
#include "src/logging/runtime-call-stats-scope.h"
#include "src/objects/api-callbacks.h"
#include "src/objects/internal-index.h"
#include "src/objects/map-inl.h"
#include "src/objects/name-inl.h"
#include "src/objects/objects-inl.h"

namespace v8 {
namespace internal {

LookupIterator::LookupIterator(Isolate* isolate, Handle<Object> receiver,
                               Handle<Name> name, Configuration configuration)
    : LookupIterator(isolate, receiver, name, kInvalidIndex, receiver,
                     configuration) {}

LookupIterator::LookupIterator(Isolate* isolate, Handle<Object> receiver,
                               Handle<Name> name,
                               Handle<Object> lookup_start_object,
                               Configuration configuration)
    : LookupIterator(isolate, receiver, name, kInvalidIndex,
                     lookup_start_object, configuration) {}

LookupIterator::LookupIterator(Isolate* isolate, Handle<Object> receiver,
                               size_t index, Configuration configuration)
    : LookupIterator(isolate, receiver, Handle<Name>(), index, receiver,
                     configuration) {
  DCHECK_NE(index, kInvalidIndex);
}

LookupIterator::LookupIterator(Isolate* isolate, Handle<Object> receiver,
                               size_t index, Handle<Object> lookup_start_object,
                               Configuration configuration)
    : LookupIterator(isolate, receiver, Handle<Name>(), index,
                     lookup_start_object, configuration) {
  DCHECK_NE(index, kInvalidIndex);
}

LookupIterator::LookupIterator(Isolate* isolate, Handle<Object> receiver,
                               const PropertyKey& key,
                               Configuration configuration)
    : LookupIterator(isolate, receiver, key.name(), key.index(), receiver,
                     configuration) {}

LookupIterator::LookupIterator(Isolate* isolate, Handle<Object> receiver,
                               const PropertyKey& key,
                               Handle<Object> lookup_start_object,
                               Configuration configuration)
    : LookupIterator(isolate, receiver, key.name(), key.index(),
                     lookup_start_object, configuration) {}

// This private constructor is the central bottleneck that all the other
// constructors use.
LookupIterator::LookupIterator(Isolate* isolate, Handle<Object> receiver,
                               Handle<Name> name, size_t index,
                               Handle<Object> lookup_start_object,
                               Configuration configuration)
    : configuration_(ComputeConfiguration(isolate, configuration, name)),
      isolate_(isolate),
      name_(name),
      receiver_(receiver),
      lookup_start_object_(lookup_start_object),
      index_(index) {
  if (IsElement()) {
    // If we're not looking at a TypedArray, we will need the key represented
    // as an internalized string.
    if (index_ > JSObject::kMaxElementIndex &&
        !lookup_start_object->IsJSTypedArray(isolate_)
#if V8_ENABLE_WEBASSEMBLY
        && !lookup_start_object->IsWasmArray(isolate_)
#endif  // V8_ENABLE_WEBASSEMBLY
    ) {
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
    DCHECK(!name_.is_null());
    name_ = isolate->factory()->InternalizeName(name_);
#ifdef DEBUG
    // Assert that the name is not an index.
    // If we're not looking at the prototype chain and the lookup start object
    // is not a typed array, then this means "array index", otherwise we need to
    // ensure the full generality so that typed arrays are handled correctly.
    if (!check_prototype_chain() && !lookup_start_object->IsJSTypedArray()) {
      uint32_t array_index;
      DCHECK(!name_->AsArrayIndex(&array_index));
    } else {
      size_t integer_index;
      DCHECK(!name_->AsIntegerIndex(&integer_index));
    }
#endif  // DEBUG
    Start<false>();
  }
}

PropertyKey::PropertyKey(Isolate* isolate, double index) {
  DCHECK_EQ(index, static_cast<uint64_t>(index));
#if V8_TARGET_ARCH_32_BIT
  if (index <= JSObject::kMaxElementIndex) {
    static_assert(JSObject::kMaxElementIndex <=
                  std::numeric_limits<size_t>::max());
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

PropertyKey::PropertyKey(Isolate* isolate, Handle<Name> name) {
  if (name->AsIntegerIndex(&index_)) {
    name_ = name;
  } else {
    index_ = LookupIterator::kInvalidIndex;
    name_ = isolate->factory()->InternalizeName(name);
  }
}

PropertyKey::PropertyKey(Isolate* isolate, Handle<Object> valid_key) {
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

bool PropertyKey::is_element() const {
  return index_ != LookupIterator::kInvalidIndex;
}

Handle<Name> PropertyKey::GetName(Isolate* isolate) {
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
  return index_ <= JSObject::kMaxElementIndex ||
         (index_ != kInvalidIndex &&
          object.map().has_any_typed_array_or_wasm_array_elements());
}

bool LookupIterator::IsPrivateName() const {
  return !IsElement() && name()->IsPrivateName(isolate());
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
  // Shared objects have fixed layout. No properties may be added to them, not
  // even private symbols.
  return !receiver->map(isolate_).is_extensible() &&
         (IsElement() || (!name_->IsPrivate(isolate_) ||
                          receiver->IsAlwaysSharedSpaceJSObject()));
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
  RCS_SCOPE(isolate, RuntimeCallCounterId::kUpdateProtector);
  DCHECK(name->IsInternalizedString() || name->IsSymbol());

  // This check must be kept in sync with
  // CodeStubAssembler::CheckForAssociatedProtector!
  ReadOnlyRoots roots(isolate);
  bool maybe_protector = roots.IsNameForProtector(*name);

#if DEBUG
  bool debug_maybe_protector =
      *name == roots.constructor_string() || *name == roots.next_string() ||
      *name == roots.resolve_string() || *name == roots.then_string() ||
      *name == roots.is_concat_spreadable_symbol() ||
      *name == roots.iterator_symbol() || *name == roots.species_symbol() ||
      *name == roots.replace_symbol();
  DCHECK_EQ(maybe_protector, debug_maybe_protector);
#endif  // DEBUG

  if (maybe_protector) {
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
                                           Handle<Object> lookup_start_object,
                                           size_t index) {
  if (lookup_start_object->IsJSReceiver(isolate)) {
    return Handle<JSReceiver>::cast(lookup_start_object);
  }
  return GetRootForNonJSReceiver(isolate, lookup_start_object, index);
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
  if (is_element && index_ <= JSObject::kMaxElementIndex) {
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
