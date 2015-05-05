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

class LookupIterator FINAL BASE_EMBEDDED {
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
    PROTOTYPE_CHAIN = kHidden | kPrototypeChain | kInterceptor
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
                 Configuration configuration = PROTOTYPE_CHAIN)
      : configuration_(ComputeConfiguration(configuration, name)),
        state_(NOT_FOUND),
        exotic_index_state_(ExoticIndexState::kUninitialized),
        interceptor_state_(InterceptorState::kUninitialized),
        property_details_(PropertyDetails::Empty()),
        isolate_(name->GetIsolate()),
        name_(name),
        receiver_(receiver),
        holder_(GetRoot(receiver_, isolate_)),
        holder_map_(holder_->map(), isolate_),
        initial_holder_(holder_),
        number_(DescriptorArray::kNotFound) {
    Next();
  }

  LookupIterator(Handle<Object> receiver, Handle<Name> name,
                 Handle<JSReceiver> holder,
                 Configuration configuration = PROTOTYPE_CHAIN)
      : configuration_(ComputeConfiguration(configuration, name)),
        state_(NOT_FOUND),
        exotic_index_state_(ExoticIndexState::kUninitialized),
        interceptor_state_(InterceptorState::kUninitialized),
        property_details_(PropertyDetails::Empty()),
        isolate_(name->GetIsolate()),
        name_(name),
        receiver_(receiver),
        holder_(holder),
        holder_map_(holder_->map(), isolate_),
        initial_holder_(holder_),
        number_(DescriptorArray::kNotFound) {
    Next();
  }

  Isolate* isolate() const { return isolate_; }
  State state() const { return state_; }
  Handle<Name> name() const { return name_; }

  bool IsFound() const { return state_ != NOT_FOUND; }
  void Next();
  void NotFound() {
    has_property_ = false;
    state_ = NOT_FOUND;
  }

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
  static Handle<JSReceiver> GetRoot(Handle<Object> receiver, Isolate* isolate);
  bool HolderIsReceiverOrHiddenPrototype() const;

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
           transition_map()->GetBackPointer()->IsMap();
  }
  void ApplyTransitionToDataProperty();
  void ReconfigureDataProperty(Handle<Object> value,
                               PropertyAttributes attributes);
  void TransitionToAccessorProperty(AccessorComponent component,
                                    Handle<Object> accessor,
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
  Handle<Object> GetDataValue() const;
  // Usually returns the value that was passed in, but may perform
  // non-observable modifications on it, such as internalize strings.
  Handle<Object> WriteDataValue(Handle<Object> value);
  void InternalizeName();

 private:
  enum class InterceptorState {
    kUninitialized,
    kSkipNonMasking,
    kProcessNonMasking
  };

  Handle<Map> GetReceiverMap() const;

  MUST_USE_RESULT inline JSReceiver* NextHolder(Map* map);
  inline State LookupInHolder(Map* map, JSReceiver* holder);
  void RestartLookupForNonMaskingInterceptors();
  State LookupNonMaskingInterceptorInHolder(Map* map, JSReceiver* holder);
  Handle<Object> FetchValue() const;
  void ReloadPropertyInformation();
  bool SkipInterceptor(JSObject* holder);

  bool IsBootstrapping() const;

  bool check_hidden() const { return (configuration_ & kHidden) != 0; }
  bool check_interceptor() const {
    return !IsBootstrapping() && (configuration_ & kInterceptor) != 0;
  }
  bool check_prototype_chain() const {
    return (configuration_ & kPrototypeChain) != 0;
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
    if (name->IsOwn()) {
      return static_cast<Configuration>(configuration &
                                        HIDDEN_SKIP_INTERCEPTOR);
    } else {
      return configuration;
    }
  }

  enum class ExoticIndexState { kUninitialized, kNoIndex, kIndex };
  bool IsIntegerIndexedExotic(JSReceiver* holder);

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
  Handle<Object> transition_;
  const Handle<Object> receiver_;
  Handle<JSReceiver> holder_;
  Handle<Map> holder_map_;
  const Handle<JSReceiver> initial_holder_;
  int number_;
};


} }  // namespace v8::internal

#endif  // V8_LOOKUP_H_
