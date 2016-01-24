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
    kHidden = 1 << 0,
    kInterceptor = 1 << 1,
    kPrototypeChain = 1 << 2,

    // Convience combinations of bits.
    OWN_SKIP_INTERCEPTOR = 0,
    OWN = kInterceptor,
    HIDDEN_SKIP_INTERCEPTOR = kHidden,
    HIDDEN = kHidden | kInterceptor,
    PROTOTYPE_CHAIN_SKIP_INTERCEPTOR = kHidden | kPrototypeChain,
    PROTOTYPE_CHAIN = kHidden | kPrototypeChain | kInterceptor,
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
      : configuration_(ComputeConfiguration(configuration, name)),
        state_(NOT_FOUND),
        exotic_index_state_(ExoticIndexState::kUninitialized),
        interceptor_state_(InterceptorState::kUninitialized),
        property_details_(PropertyDetails::Empty()),
        isolate_(name->GetIsolate()),
        name_(Name::Flatten(name)),
        // kMaxUInt32 isn't a valid index.
        index_(kMaxUInt32),
        receiver_(receiver),
        holder_(GetRoot(isolate_, receiver)),
        holder_map_(holder_->map(), isolate_),
        initial_holder_(holder_),
        number_(DescriptorArray::kNotFound) {
#ifdef DEBUG
    uint32_t index;  // Assert that the name is not an array index.
    DCHECK(!name->AsArrayIndex(&index));
#endif  // DEBUG
    Next();
  }

  LookupIterator(Handle<Object> receiver, Handle<Name> name,
                 Handle<JSReceiver> holder,
                 Configuration configuration = DEFAULT)
      : configuration_(ComputeConfiguration(configuration, name)),
        state_(NOT_FOUND),
        exotic_index_state_(ExoticIndexState::kUninitialized),
        interceptor_state_(InterceptorState::kUninitialized),
        property_details_(PropertyDetails::Empty()),
        isolate_(name->GetIsolate()),
        name_(Name::Flatten(name)),
        // kMaxUInt32 isn't a valid index.
        index_(kMaxUInt32),
        receiver_(receiver),
        holder_(holder),
        holder_map_(holder_->map(), isolate_),
        initial_holder_(holder_),
        number_(DescriptorArray::kNotFound) {
#ifdef DEBUG
    uint32_t index;  // Assert that the name is not an array index.
    DCHECK(!name->AsArrayIndex(&index));
#endif  // DEBUG
    Next();
  }

  LookupIterator(Isolate* isolate, Handle<Object> receiver, uint32_t index,
                 Configuration configuration = DEFAULT)
      : configuration_(configuration),
        state_(NOT_FOUND),
        exotic_index_state_(ExoticIndexState::kUninitialized),
        interceptor_state_(InterceptorState::kUninitialized),
        property_details_(PropertyDetails::Empty()),
        isolate_(isolate),
        name_(),
        index_(index),
        receiver_(receiver),
        holder_(GetRoot(isolate, receiver, index)),
        holder_map_(holder_->map(), isolate_),
        initial_holder_(holder_),
        number_(DescriptorArray::kNotFound) {
    // kMaxUInt32 isn't a valid index.
    DCHECK_NE(kMaxUInt32, index_);
    Next();
  }

  LookupIterator(Isolate* isolate, Handle<Object> receiver, uint32_t index,
                 Handle<JSReceiver> holder,
                 Configuration configuration = DEFAULT)
      : configuration_(configuration),
        state_(NOT_FOUND),
        exotic_index_state_(ExoticIndexState::kUninitialized),
        interceptor_state_(InterceptorState::kUninitialized),
        property_details_(PropertyDetails::Empty()),
        isolate_(isolate),
        name_(),
        index_(index),
        receiver_(receiver),
        holder_(holder),
        holder_map_(holder_->map(), isolate_),
        initial_holder_(holder_),
        number_(DescriptorArray::kNotFound) {
    // kMaxUInt32 isn't a valid index.
    DCHECK_NE(kMaxUInt32, index_);
    Next();
  }

  static LookupIterator PropertyOrElement(
      Isolate* isolate, Handle<Object> receiver, Handle<Name> name,
      Configuration configuration = DEFAULT) {
    name = Name::Flatten(name);
    uint32_t index;
    LookupIterator it =
        name->AsArrayIndex(&index)
            ? LookupIterator(isolate, receiver, index, configuration)
            : LookupIterator(receiver, name, configuration);
    it.name_ = name;
    return it;
  }

  static LookupIterator PropertyOrElement(
      Isolate* isolate, Handle<Object> receiver, Handle<Name> name,
      Handle<JSReceiver> holder, Configuration configuration = DEFAULT) {
    name = Name::Flatten(name);
    uint32_t index;
    LookupIterator it =
        name->AsArrayIndex(&index)
            ? LookupIterator(isolate, receiver, index, holder, configuration)
            : LookupIterator(receiver, name, holder, configuration);
    it.name_ = name;
    return it;
  }

  static LookupIterator PropertyOrElement(
      Isolate* isolate, Handle<Object> receiver, Handle<Object> key,
      bool* success, Configuration configuration = DEFAULT);

  void Restart() { RestartInternal(InterceptorState::kUninitialized); }

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
  Handle<JSObject> GetStoreTarget() const;
  bool is_dictionary_holder() const { return holder_map_->is_dictionary_map(); }
  Handle<Map> transition_map() const {
    DCHECK_EQ(TRANSITION, state_);
    return Handle<Map>::cast(transition_);
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
  void PrepareForDataProperty(Handle<Object> value);
  void PrepareTransitionToDataProperty(Handle<Object> value,
                                       PropertyAttributes attributes,
                                       Object::StoreFromKeyed store_mode);
  bool IsCacheableTransition() {
    if (state_ != TRANSITION) return false;
    return transition_->IsPropertyCell() ||
           (!transition_map()->is_dictionary_map() &&
            transition_map()->GetBackPointer()->IsMap());
  }
  void ApplyTransitionToDataProperty();
  void ReconfigureDataProperty(Handle<Object> value,
                               PropertyAttributes attributes);
  void Delete();
  void TransitionToAccessorProperty(AccessorComponent component,
                                    Handle<Object> accessor,
                                    PropertyAttributes attributes);
  void TransitionToAccessorPair(Handle<Object> pair,
                                PropertyAttributes attributes);
  PropertyDetails property_details() const {
    DCHECK(has_property_);
    return property_details_;
  }
  bool IsConfigurable() const { return property_details().IsConfigurable(); }
  bool IsReadOnly() const { return property_details().IsReadOnly(); }
  Representation representation() const {
    return property_details().representation();
  }
  FieldIndex GetFieldIndex() const;
  Handle<HeapType> GetFieldType() const;
  int GetAccessorIndex() const;
  int GetConstantIndex() const;
  Handle<PropertyCell> GetPropertyCell() const;
  Handle<Object> GetAccessors() const;
  inline Handle<InterceptorInfo> GetInterceptor() const {
    DCHECK_EQ(INTERCEPTOR, state_);
    return handle(GetInterceptor(JSObject::cast(*holder_)), isolate_);
  }
  Handle<Object> GetDataValue() const;
  void WriteDataValue(Handle<Object> value);
  void InternalizeName();
  void ReloadHolderMap();

 private:
  enum class InterceptorState {
    kUninitialized,
    kSkipNonMasking,
    kProcessNonMasking
  };

  Handle<Map> GetReceiverMap() const;

  MUST_USE_RESULT inline JSReceiver* NextHolder(Map* map);
  inline State LookupInHolder(Map* map, JSReceiver* holder);
  void RestartLookupForNonMaskingInterceptors() {
    RestartInternal(InterceptorState::kProcessNonMasking);
  }
  void RestartInternal(InterceptorState interceptor_state);
  State LookupNonMaskingInterceptorInHolder(Map* map, JSReceiver* holder);
  Handle<Object> FetchValue() const;
  void ReloadPropertyInformation();
  inline bool SkipInterceptor(JSObject* holder);
  bool HasInterceptor(Map* map) const;
  bool InternalHolderIsReceiverOrHiddenPrototype() const;
  inline InterceptorInfo* GetInterceptor(JSObject* holder) const {
    if (IsElement()) return holder->GetIndexedInterceptor();
    return holder->GetNamedInterceptor();
  }

  bool check_hidden() const { return (configuration_ & kHidden) != 0; }
  bool check_interceptor() const {
    return (configuration_ & kInterceptor) != 0;
  }
  int descriptor_number() const {
    DCHECK(has_property_);
    DCHECK(!holder_map_->is_dictionary_map());
    return number_;
  }
  int dictionary_entry() const {
    DCHECK(has_property_);
    DCHECK(holder_map_->is_dictionary_map());
    return number_;
  }

  static Configuration ComputeConfiguration(
      Configuration configuration, Handle<Name> name) {
    if (name->IsPrivate()) {
      return static_cast<Configuration>(configuration &
                                        HIDDEN_SKIP_INTERCEPTOR);
    } else {
      return configuration;
    }
  }

  static Handle<JSReceiver> GetRootForNonJSReceiver(
      Isolate* isolate, Handle<Object> receiver, uint32_t index = kMaxUInt32);
  inline static Handle<JSReceiver> GetRoot(Isolate* isolate,
                                           Handle<Object> receiver,
                                           uint32_t index = kMaxUInt32) {
    if (receiver->IsJSReceiver()) return Handle<JSReceiver>::cast(receiver);
    return GetRootForNonJSReceiver(isolate, receiver, index);
  }

  enum class ExoticIndexState { kUninitialized, kNotExotic, kExotic };
  inline bool IsIntegerIndexedExotic(JSReceiver* holder);

  // If configuration_ becomes mutable, update
  // HolderIsReceiverOrHiddenPrototype.
  const Configuration configuration_;
  State state_;
  bool has_property_;
  ExoticIndexState exotic_index_state_;
  InterceptorState interceptor_state_;
  PropertyDetails property_details_;
  Isolate* const isolate_;
  Handle<Name> name_;
  uint32_t index_;
  Handle<Object> transition_;
  const Handle<Object> receiver_;
  Handle<JSReceiver> holder_;
  Handle<Map> holder_map_;
  const Handle<JSReceiver> initial_holder_;
  uint32_t number_;
};


}  // namespace internal
}  // namespace v8

#endif  // V8_LOOKUP_H_
