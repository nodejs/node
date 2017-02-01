// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_LOOKUP_H_
#define V8_LOOKUP_H_

#include "src/factory.h"
#include "src/isolate.h"
#include "src/objects.h"

namespace v8 {
namespace internal {

class LookupIterator final BASE_EMBEDDED {
 public:
  enum Configuration {
    // Configuration bits.
    kInterceptor = 1 << 0,
    kPrototypeChain = 1 << 1,

    // Convience combinations of bits.
    OWN_SKIP_INTERCEPTOR = 0,
    OWN = kInterceptor,
    PROTOTYPE_CHAIN_SKIP_INTERCEPTOR = kPrototypeChain,
    PROTOTYPE_CHAIN = kPrototypeChain | kInterceptor,
    DEFAULT = PROTOTYPE_CHAIN
  };

  enum State {
    ACCESS_CHECK,
    INTEGER_INDEXED_EXOTIC,
    INTERCEPTOR,
    JSPROXY,
    NOT_FOUND,
    ACCESSOR,
    DATA,
    TRANSITION,
    // Set state_ to BEFORE_PROPERTY to ensure that the next lookup will be a
    // PROPERTY lookup.
    BEFORE_PROPERTY = INTERCEPTOR
  };

  LookupIterator(Handle<Object> receiver, Handle<Name> name,
                 Configuration configuration = DEFAULT)
      : LookupIterator(name->GetIsolate(), receiver, name, configuration) {}

  LookupIterator(Isolate* isolate, Handle<Object> receiver, Handle<Name> name,
                 Configuration configuration = DEFAULT)
      : LookupIterator(isolate, receiver, name, GetRoot(isolate, receiver),
                       configuration) {}

  LookupIterator(Handle<Object> receiver, Handle<Name> name,
                 Handle<JSReceiver> holder,
                 Configuration configuration = DEFAULT)
      : LookupIterator(name->GetIsolate(), receiver, name, holder,
                       configuration) {}

  LookupIterator(Isolate* isolate, Handle<Object> receiver, Handle<Name> name,
                 Handle<JSReceiver> holder,
                 Configuration configuration = DEFAULT)
      : configuration_(ComputeConfiguration(configuration, name)),
        interceptor_state_(InterceptorState::kUninitialized),
        property_details_(PropertyDetails::Empty()),
        isolate_(isolate),
        name_(isolate_->factory()->InternalizeName(name)),
        receiver_(receiver),
        initial_holder_(holder),
        // kMaxUInt32 isn't a valid index.
        index_(kMaxUInt32),
        number_(DescriptorArray::kNotFound) {
#ifdef DEBUG
    uint32_t index;  // Assert that the name is not an array index.
    DCHECK(!name->AsArrayIndex(&index));
#endif  // DEBUG
    Start<false>();
  }

  LookupIterator(Isolate* isolate, Handle<Object> receiver, uint32_t index,
                 Configuration configuration = DEFAULT)
      : LookupIterator(isolate, receiver, index,
                       GetRoot(isolate, receiver, index), configuration) {}

  LookupIterator(Isolate* isolate, Handle<Object> receiver, uint32_t index,
                 Handle<JSReceiver> holder,
                 Configuration configuration = DEFAULT)
      : configuration_(configuration),
        interceptor_state_(InterceptorState::kUninitialized),
        property_details_(PropertyDetails::Empty()),
        isolate_(isolate),
        receiver_(receiver),
        initial_holder_(holder),
        index_(index),
        number_(DescriptorArray::kNotFound) {
    // kMaxUInt32 isn't a valid index.
    DCHECK_NE(kMaxUInt32, index_);
    Start<true>();
  }

  static LookupIterator PropertyOrElement(
      Isolate* isolate, Handle<Object> receiver, Handle<Name> name,
      Configuration configuration = DEFAULT) {
    uint32_t index;
    if (name->AsArrayIndex(&index)) {
      LookupIterator it =
          LookupIterator(isolate, receiver, index, configuration);
      it.name_ = name;
      return it;
    }
    return LookupIterator(receiver, name, configuration);
  }

  static LookupIterator PropertyOrElement(
      Isolate* isolate, Handle<Object> receiver, Handle<Name> name,
      Handle<JSReceiver> holder, Configuration configuration = DEFAULT) {
    uint32_t index;
    if (name->AsArrayIndex(&index)) {
      LookupIterator it =
          LookupIterator(isolate, receiver, index, holder, configuration);
      it.name_ = name;
      return it;
    }
    return LookupIterator(receiver, name, holder, configuration);
  }

  static LookupIterator PropertyOrElement(
      Isolate* isolate, Handle<Object> receiver, Handle<Object> key,
      bool* success, Configuration configuration = DEFAULT);

  void Restart() {
    InterceptorState state = InterceptorState::kUninitialized;
    IsElement() ? RestartInternal<true>(state) : RestartInternal<false>(state);
  }

  Isolate* isolate() const { return isolate_; }
  State state() const { return state_; }

  Handle<Name> name() const {
    DCHECK(!IsElement());
    return name_;
  }
  Handle<Name> GetName() {
    if (name_.is_null()) {
      DCHECK(IsElement());
      name_ = factory()->Uint32ToString(index_);
    }
    return name_;
  }
  uint32_t index() const { return index_; }

  bool IsElement() const { return index_ != kMaxUInt32; }

  bool IsFound() const { return state_ != NOT_FOUND; }
  void Next();
  void NotFound() {
    has_property_ = false;
    state_ = NOT_FOUND;
  }

  Heap* heap() const { return isolate_->heap(); }
  Factory* factory() const { return isolate_->factory(); }
  Handle<Object> GetReceiver() const { return receiver_; }

  Handle<JSObject> GetStoreTarget() const {
    DCHECK(receiver_->IsJSObject());
    if (receiver_->IsJSGlobalProxy()) {
      Map* map = JSGlobalProxy::cast(*receiver_)->map();
      if (map->has_hidden_prototype()) {
        return handle(JSGlobalObject::cast(map->prototype()), isolate_);
      }
    }
    return Handle<JSObject>::cast(receiver_);
  }

  bool is_dictionary_holder() const { return !holder_->HasFastProperties(); }
  Handle<Map> transition_map() const {
    DCHECK_EQ(TRANSITION, state_);
    return Handle<Map>::cast(transition_);
  }
  Handle<PropertyCell> transition_cell() const {
    DCHECK_EQ(TRANSITION, state_);
    return Handle<PropertyCell>::cast(transition_);
  }
  template <class T>
  Handle<T> GetHolder() const {
    DCHECK(IsFound());
    return Handle<T>::cast(holder_);
  }

  bool HolderIsReceiverOrHiddenPrototype() const;

  bool check_prototype_chain() const {
    return (configuration_ & kPrototypeChain) != 0;
  }

  /* ACCESS_CHECK */
  bool HasAccess() const;

  /* PROPERTY */
  bool ExtendingNonExtensible(Handle<JSObject> receiver) {
    DCHECK(receiver.is_identical_to(GetStoreTarget()));
    return !receiver->map()->is_extensible() &&
           (IsElement() || !name_->IsPrivate());
  }
  void PrepareForDataProperty(Handle<Object> value);
  void PrepareTransitionToDataProperty(Handle<JSObject> receiver,
                                       Handle<Object> value,
                                       PropertyAttributes attributes,
                                       Object::StoreFromKeyed store_mode);
  bool IsCacheableTransition() {
    DCHECK_EQ(TRANSITION, state_);
    return transition_->IsPropertyCell() ||
           (!transition_map()->is_dictionary_map() &&
            transition_map()->GetBackPointer()->IsMap());
  }
  void ApplyTransitionToDataProperty(Handle<JSObject> receiver);
  void ReconfigureDataProperty(Handle<Object> value,
                               PropertyAttributes attributes);
  void Delete();
  void TransitionToAccessorProperty(Handle<Object> getter,
                                    Handle<Object> setter,
                                    PropertyAttributes attributes);
  void TransitionToAccessorPair(Handle<Object> pair,
                                PropertyAttributes attributes);
  PropertyDetails property_details() const {
    DCHECK(has_property_);
    return property_details_;
  }
  PropertyAttributes property_attributes() const {
    return property_details().attributes();
  }
  bool IsConfigurable() const { return property_details().IsConfigurable(); }
  bool IsReadOnly() const { return property_details().IsReadOnly(); }
  bool IsEnumerable() const { return property_details().IsEnumerable(); }
  Representation representation() const {
    return property_details().representation();
  }
  FieldIndex GetFieldIndex() const;
  Handle<FieldType> GetFieldType() const;
  int GetAccessorIndex() const;
  int GetConstantIndex() const;
  Handle<PropertyCell> GetPropertyCell() const;
  Handle<Object> GetAccessors() const;
  inline Handle<InterceptorInfo> GetInterceptor() const {
    DCHECK_EQ(INTERCEPTOR, state_);
    InterceptorInfo* result =
        IsElement() ? GetInterceptor<true>(JSObject::cast(*holder_))
                    : GetInterceptor<false>(JSObject::cast(*holder_));
    return handle(result, isolate_);
  }
  Handle<InterceptorInfo> GetInterceptorForFailedAccessCheck() const;
  Handle<Object> GetDataValue() const;
  void WriteDataValue(Handle<Object> value);
  inline void UpdateProtector() {
    if (IsElement()) return;
    if (*name_ == heap()->is_concat_spreadable_symbol() ||
        *name_ == heap()->constructor_string() ||
        *name_ == heap()->species_symbol() ||
        *name_ == heap()->has_instance_symbol()) {
      InternalUpdateProtector();
    }
  }

 private:
  void InternalUpdateProtector();

  enum class InterceptorState {
    kUninitialized,
    kSkipNonMasking,
    kProcessNonMasking
  };

  Handle<Map> GetReceiverMap() const;

  MUST_USE_RESULT inline JSReceiver* NextHolder(Map* map);

  template <bool is_element>
  V8_EXPORT_PRIVATE void Start();
  template <bool is_element>
  void NextInternal(Map* map, JSReceiver* holder);
  template <bool is_element>
  inline State LookupInHolder(Map* map, JSReceiver* holder) {
    return map->instance_type() <= LAST_SPECIAL_RECEIVER_TYPE
               ? LookupInSpecialHolder<is_element>(map, holder)
               : LookupInRegularHolder<is_element>(map, holder);
  }
  template <bool is_element>
  State LookupInRegularHolder(Map* map, JSReceiver* holder);
  template <bool is_element>
  State LookupInSpecialHolder(Map* map, JSReceiver* holder);
  template <bool is_element>
  void RestartLookupForNonMaskingInterceptors() {
    RestartInternal<is_element>(InterceptorState::kProcessNonMasking);
  }
  template <bool is_element>
  void RestartInternal(InterceptorState interceptor_state);
  Handle<Object> FetchValue() const;
  template <bool is_element>
  void ReloadPropertyInformation();

  template <bool is_element>
  bool SkipInterceptor(JSObject* holder);
  template <bool is_element>
  inline InterceptorInfo* GetInterceptor(JSObject* holder) const {
    return is_element ? holder->GetIndexedInterceptor()
                      : holder->GetNamedInterceptor();
  }

  bool check_interceptor() const {
    return (configuration_ & kInterceptor) != 0;
  }
  int descriptor_number() const {
    DCHECK(!IsElement());
    DCHECK(has_property_);
    DCHECK(holder_->HasFastProperties());
    return number_;
  }
  int dictionary_entry() const {
    DCHECK(!IsElement());
    DCHECK(has_property_);
    DCHECK(!holder_->HasFastProperties());
    return number_;
  }

  static Configuration ComputeConfiguration(
      Configuration configuration, Handle<Name> name) {
    return name->IsPrivate() ? OWN_SKIP_INTERCEPTOR : configuration;
  }

  static Handle<JSReceiver> GetRootForNonJSReceiver(
      Isolate* isolate, Handle<Object> receiver, uint32_t index = kMaxUInt32);
  inline static Handle<JSReceiver> GetRoot(Isolate* isolate,
                                           Handle<Object> receiver,
                                           uint32_t index = kMaxUInt32) {
    if (receiver->IsJSReceiver()) return Handle<JSReceiver>::cast(receiver);
    return GetRootForNonJSReceiver(isolate, receiver, index);
  }

  State NotFound(JSReceiver* const holder) const;

  // If configuration_ becomes mutable, update
  // HolderIsReceiverOrHiddenPrototype.
  const Configuration configuration_;
  State state_;
  bool has_property_;
  InterceptorState interceptor_state_;
  PropertyDetails property_details_;
  Isolate* const isolate_;
  Handle<Name> name_;
  Handle<Object> transition_;
  const Handle<Object> receiver_;
  Handle<JSReceiver> holder_;
  const Handle<JSReceiver> initial_holder_;
  const uint32_t index_;
  uint32_t number_;
};


}  // namespace internal
}  // namespace v8

#endif  // V8_LOOKUP_H_
