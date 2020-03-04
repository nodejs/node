// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_LOOKUP_H_
#define V8_OBJECTS_LOOKUP_H_

#include "src/common/globals.h"
#include "src/execution/isolate.h"
#include "src/heap/factory.h"
#include "src/objects/descriptor-array.h"
#include "src/objects/js-objects.h"
#include "src/objects/map.h"
#include "src/objects/objects.h"

namespace v8 {
namespace internal {

class V8_EXPORT_PRIVATE LookupIterator final {
 public:
  enum Configuration {
    // Configuration bits.
    kInterceptor = 1 << 0,
    kPrototypeChain = 1 << 1,

    // Convenience combinations of bits.
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

  inline LookupIterator(Isolate* isolate, Handle<Object> receiver,
                        Handle<Name> name,
                        Configuration configuration = DEFAULT);

  inline LookupIterator(Handle<Object> receiver, Handle<Name> name,
                        Handle<JSReceiver> holder,
                        Configuration configuration = DEFAULT);

  inline LookupIterator(Isolate* isolate, Handle<Object> receiver,
                        Handle<Name> name, Handle<JSReceiver> holder,
                        Configuration configuration = DEFAULT);

  inline LookupIterator(Isolate* isolate, Handle<Object> receiver, size_t index,
                        Configuration configuration = DEFAULT,
                        Handle<Name> key_as_string = Handle<Name>());

  inline LookupIterator(Isolate* isolate, Handle<Object> receiver, size_t index,
                        Handle<JSReceiver> holder,
                        Configuration configuration = DEFAULT,
                        Handle<Name> key_as_string = Handle<Name>());

  static inline LookupIterator PropertyOrElement(
      Isolate* isolate, Handle<Object> receiver, Handle<Name> name,
      Configuration configuration = DEFAULT);

  static inline LookupIterator PropertyOrElement(
      Isolate* isolate, Handle<Object> receiver, Handle<Name> name,
      Handle<JSReceiver> holder, Configuration configuration = DEFAULT);

  static LookupIterator PropertyOrElement(
      Isolate* isolate, Handle<Object> receiver, Handle<Object> key,
      bool* success, Handle<JSReceiver> holder,
      Configuration configuration = DEFAULT);

  static LookupIterator PropertyOrElement(
      Isolate* isolate, Handle<Object> receiver, Handle<Object> key,
      bool* success, Configuration configuration = DEFAULT);

  void Restart() {
    InterceptorState state = InterceptorState::kUninitialized;
    IsElement() ? RestartInternal<true>(state) : RestartInternal<false>(state);
  }

  Isolate* isolate() const { return isolate_; }
  State state() const { return state_; }

  inline Handle<Name> name() const;
  inline Handle<Name> GetName();
  size_t index() const { return index_; }
  uint32_t array_index() const {
    DCHECK_LE(index_, JSArray::kMaxArrayIndex);
    return static_cast<uint32_t>(index_);
  }

  // Returns true if this LookupIterator has an index in the range
  // [0, size_t::max).
  bool IsElement() const { return index_ != kInvalidIndex; }
  // Returns true if this LookupIterator has an index that counts as an
  // element for the given object (up to kMaxArrayIndex for JSArrays,
  // any integer for JSTypedArrays).
  inline bool IsElement(JSReceiver object) const;

  bool IsFound() const { return state_ != NOT_FOUND; }
  void Next();
  void NotFound() {
    has_property_ = false;
    state_ = NOT_FOUND;
  }

  Heap* heap() const { return isolate_->heap(); }
  Factory* factory() const { return isolate_->factory(); }
  Handle<Object> GetReceiver() const { return receiver_; }

  template <class T>
  inline Handle<T> GetStoreTarget() const;
  inline bool is_dictionary_holder() const;
  inline Handle<Map> transition_map() const;
  inline Handle<PropertyCell> transition_cell() const;
  template <class T>
  inline Handle<T> GetHolder() const;

  bool HolderIsReceiver() const;
  bool HolderIsReceiverOrHiddenPrototype() const;

  bool check_prototype_chain() const {
    return (configuration_ & kPrototypeChain) != 0;
  }

  /* ACCESS_CHECK */
  bool HasAccess() const;

  /* PROPERTY */
  inline bool ExtendingNonExtensible(Handle<JSReceiver> receiver);
  void PrepareForDataProperty(Handle<Object> value);
  void PrepareTransitionToDataProperty(Handle<JSReceiver> receiver,
                                       Handle<Object> value,
                                       PropertyAttributes attributes,
                                       StoreOrigin store_origin);
  inline bool IsCacheableTransition();
  void ApplyTransitionToDataProperty(Handle<JSReceiver> receiver);
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
  PropertyLocation location() const { return property_details().location(); }
  PropertyConstness constness() const { return property_details().constness(); }
  Handle<Map> GetFieldOwnerMap() const;
  FieldIndex GetFieldIndex() const;
  Handle<FieldType> GetFieldType() const;
  int GetFieldDescriptorIndex() const;
  int GetAccessorIndex() const;
  Handle<PropertyCell> GetPropertyCell() const;
  Handle<Object> GetAccessors() const;
  inline Handle<InterceptorInfo> GetInterceptor() const;
  Handle<InterceptorInfo> GetInterceptorForFailedAccessCheck() const;
  Handle<Object> GetDataValue() const;
  void WriteDataValue(Handle<Object> value, bool initializing_store);
  inline void UpdateProtector();
  static inline void UpdateProtector(Isolate* isolate, Handle<Object> receiver,
                                     Handle<Name> name);

  // Lookup a 'cached' private property for an accessor.
  // If not found returns false and leaves the LookupIterator unmodified.
  bool TryLookupCachedProperty();
  bool LookupCachedProperty();

 private:
  static const size_t kInvalidIndex = std::numeric_limits<size_t>::max();

  // For |ForTransitionHandler|.
  LookupIterator(Isolate* isolate, Handle<Object> receiver, Handle<Name> name,
                 Handle<Map> transition_map, PropertyDetails details,
                 bool has_property);

  static void InternalUpdateProtector(Isolate* isolate, Handle<Object> receiver,
                                      Handle<Name> name);

  enum class InterceptorState {
    kUninitialized,
    kSkipNonMasking,
    kProcessNonMasking
  };

  Handle<Map> GetReceiverMap() const;

  V8_WARN_UNUSED_RESULT inline JSReceiver NextHolder(Map map);

  bool is_js_array_element(bool is_element) const {
    return is_element && index_ <= JSArray::kMaxArrayIndex;
  }
  template <bool is_element>
  V8_EXPORT_PRIVATE void Start();
  template <bool is_element>
  void NextInternal(Map map, JSReceiver holder);
  template <bool is_element>
  inline State LookupInHolder(Map map, JSReceiver holder) {
    return map.IsSpecialReceiverMap()
               ? LookupInSpecialHolder<is_element>(map, holder)
               : LookupInRegularHolder<is_element>(map, holder);
  }
  template <bool is_element>
  State LookupInRegularHolder(Map map, JSReceiver holder);
  template <bool is_element>
  State LookupInSpecialHolder(Map map, JSReceiver holder);
  template <bool is_element>
  void RestartLookupForNonMaskingInterceptors() {
    RestartInternal<is_element>(InterceptorState::kProcessNonMasking);
  }
  template <bool is_element>
  void RestartInternal(InterceptorState interceptor_state);
  Handle<Object> FetchValue() const;
  bool IsConstFieldValueEqualTo(Object value) const;
  template <bool is_element>
  void ReloadPropertyInformation();

  template <bool is_element>
  bool SkipInterceptor(JSObject holder);
  template <bool is_element>
  inline InterceptorInfo GetInterceptor(JSObject holder) const;

  bool check_interceptor() const {
    return (configuration_ & kInterceptor) != 0;
  }
  inline InternalIndex descriptor_number() const;
  inline InternalIndex dictionary_entry() const;

  static inline Configuration ComputeConfiguration(Isolate* isolate,
                                                   Configuration configuration,
                                                   Handle<Name> name);

  static Handle<JSReceiver> GetRootForNonJSReceiver(
      Isolate* isolate, Handle<Object> receiver, size_t index = kInvalidIndex);
  static inline Handle<JSReceiver> GetRoot(Isolate* isolate,
                                           Handle<Object> receiver,
                                           size_t index = kInvalidIndex);

  State NotFound(JSReceiver const holder) const;

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
  const size_t index_;
  InternalIndex number_ = InternalIndex::NotFound();
};

}  // namespace internal
}  // namespace v8

#endif  // V8_OBJECTS_LOOKUP_H_
