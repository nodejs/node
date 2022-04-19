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

#if V8_ENABLE_WEBASSEMBLY
#include "src/wasm/value-type.h"
#endif  // V8_ENABLE_WEBASSEMBLY

namespace v8 {
namespace internal {

class PropertyKey {
 public:
  inline PropertyKey(Isolate* isolate, double index);
  // {name} might be a string representation of an element index.
  inline PropertyKey(Isolate* isolate, Handle<Name> name);
  // {valid_key} is a Name or Number.
  inline PropertyKey(Isolate* isolate, Handle<Object> valid_key);
  // {key} could be anything.
  PropertyKey(Isolate* isolate, Handle<Object> key, bool* success);

  inline bool is_element() const;
  Handle<Name> name() const { return name_; }
  size_t index() const { return index_; }
  inline Handle<Name> GetName(Isolate* isolate);

 private:
  Handle<Name> name_;
  size_t index_;
};

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

  // {name} is guaranteed to be a property name (and not e.g. "123").
  inline LookupIterator(Isolate* isolate, Handle<Object> receiver,
                        Handle<Name> name,
                        Configuration configuration = DEFAULT);
  inline LookupIterator(Isolate* isolate, Handle<Object> receiver,
                        Handle<Name> name, Handle<Object> lookup_start_object,
                        Configuration configuration = DEFAULT);

  inline LookupIterator(Isolate* isolate, Handle<Object> receiver, size_t index,
                        Configuration configuration = DEFAULT);
  inline LookupIterator(Isolate* isolate, Handle<Object> receiver, size_t index,
                        Handle<Object> lookup_start_object,
                        Configuration configuration = DEFAULT);

  inline LookupIterator(Isolate* isolate, Handle<Object> receiver,
                        const PropertyKey& key,
                        Configuration configuration = DEFAULT);
  inline LookupIterator(Isolate* isolate, Handle<Object> receiver,
                        const PropertyKey& key,
                        Handle<Object> lookup_start_object,
                        Configuration configuration = DEFAULT);

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

  Handle<Object> lookup_start_object() const { return lookup_start_object_; }

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
  FieldIndex GetFieldIndex() const;
  int GetFieldDescriptorIndex() const;
  int GetAccessorIndex() const;
  Handle<PropertyCell> GetPropertyCell() const;
  Handle<Object> GetAccessors() const;
  inline Handle<InterceptorInfo> GetInterceptor() const;
  Handle<InterceptorInfo> GetInterceptorForFailedAccessCheck() const;
  Handle<Object> GetDataValue(AllocationPolicy allocation_policy =
                                  AllocationPolicy::kAllocationAllowed) const;
  void WriteDataValue(Handle<Object> value, bool initializing_store);
  Handle<Object> GetDataValue(SeqCstAccessTag tag) const;
  void WriteDataValue(Handle<Object> value, SeqCstAccessTag tag);
  Handle<Object> SwapDataValue(Handle<Object> value, SeqCstAccessTag tag);
  inline void UpdateProtector();
  static inline void UpdateProtector(Isolate* isolate, Handle<Object> receiver,
                                     Handle<Name> name);

#if V8_ENABLE_WEBASSEMBLY
  // Fetches type of WasmStruct's field or WasmArray's elements, it
  // is used for preparing the value for storing into WasmObjects.
  wasm::ValueType wasm_value_type() const;
  void WriteDataValueToWasmObject(Handle<Object> value);
#endif  // V8_ENABLE_WEBASSEMBLY

  // Lookup a 'cached' private property for an accessor.
  // If not found returns false and leaves the LookupIterator unmodified.
  bool TryLookupCachedProperty(Handle<AccessorPair> accessor);
  bool TryLookupCachedProperty();

 private:
  friend PropertyKey;

  static const size_t kInvalidIndex = std::numeric_limits<size_t>::max();

  bool LookupCachedProperty(Handle<AccessorPair> accessor);
  inline LookupIterator(Isolate* isolate, Handle<Object> receiver,
                        Handle<Name> name, size_t index,
                        Handle<Object> lookup_start_object,
                        Configuration configuration);

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
  Handle<Object> FetchValue(AllocationPolicy allocation_policy =
                                AllocationPolicy::kAllocationAllowed) const;
  bool IsConstFieldValueEqualTo(Object value) const;
  bool IsConstDictValueEqualTo(Object value) const;

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
      Isolate* isolate, Handle<Object> lookup_start_object,
      size_t index = kInvalidIndex);
  static inline Handle<JSReceiver> GetRoot(Isolate* isolate,
                                           Handle<Object> lookup_start_object,
                                           size_t index = kInvalidIndex);

  State NotFound(JSReceiver const holder) const;

  // If configuration_ becomes mutable, update
  // HolderIsReceiverOrHiddenPrototype.
  const Configuration configuration_;
  State state_ = NOT_FOUND;
  bool has_property_ = false;
  InterceptorState interceptor_state_ = InterceptorState::kUninitialized;
  PropertyDetails property_details_ = PropertyDetails::Empty();
  Isolate* const isolate_;
  Handle<Name> name_;
  Handle<Object> transition_;
  const Handle<Object> receiver_;
  Handle<JSReceiver> holder_;
  const Handle<Object> lookup_start_object_;
  const size_t index_;
  InternalIndex number_ = InternalIndex::NotFound();
};

// Similar to the LookupIterator, but for concurrent accesses from a background
// thread.
//
// Note: This is a work in progress, intended to bundle code related to
// concurrent lookups here. In its current state, the class is obviously not an
// 'iterator'. Still, keeping the name for now, with the intent to clarify
// names and implementation once we've gotten some experience with more
// involved logic.
// TODO(jgruber, v8:7790): Consider using a LookupIterator-style interface.
// TODO(jgruber, v8:7790): Consider merging back into the LookupIterator once
// functionality and constraints are better known.
class ConcurrentLookupIterator final : public AllStatic {
 public:
  // Tri-state to distinguish between 'not-present' and 'who-knows' failures.
  enum Result {
    kPresent,     // The value was found.
    kNotPresent,  // No value exists.
    kGaveUp,      // The operation can't be completed.
  };

  // Implements the own data property lookup for the specialized case of
  // fixed_cow_array backing stores (these are only in use for array literal
  // boilerplates). The contract is that the elements, elements kind, and array
  // length passed to this function should all be read from the same JSArray
  // instance; but due to concurrency it's possible that they may not be
  // consistent among themselves (e.g. the elements kind may not match the
  // given elements backing store). We are thus extra-careful to handle
  // exceptional situations.
  V8_EXPORT_PRIVATE static base::Optional<Object> TryGetOwnCowElement(
      Isolate* isolate, FixedArray array_elements, ElementsKind elements_kind,
      int array_length, size_t index);

  // As above, the contract is that the elements and elements kind should be
  // read from the same holder, but this function is implemented defensively to
  // tolerate concurrency issues.
  V8_EXPORT_PRIVATE static Result TryGetOwnConstantElement(
      Object* result_out, Isolate* isolate, LocalIsolate* local_isolate,
      JSObject holder, FixedArrayBase elements, ElementsKind elements_kind,
      size_t index);

  // Implements the own data property lookup for the specialized case of
  // strings.
  V8_EXPORT_PRIVATE static Result TryGetOwnChar(String* result_out,
                                                Isolate* isolate,
                                                LocalIsolate* local_isolate,
                                                String string, size_t index);

  // This method reimplements the following sequence in a concurrent setting:
  //
  // LookupIterator it(holder, isolate, name, LookupIterator::OWN);
  // it.TryLookupCachedProperty();
  // if (it.state() == LookupIterator::DATA) it.GetPropertyCell();
  V8_EXPORT_PRIVATE static base::Optional<PropertyCell> TryGetPropertyCell(
      Isolate* isolate, LocalIsolate* local_isolate,
      Handle<JSGlobalObject> holder, Handle<Name> name);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_OBJECTS_LOOKUP_H_
