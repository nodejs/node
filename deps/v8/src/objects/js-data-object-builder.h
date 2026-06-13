// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_JS_DATA_OBJECT_BUILDER_H_
#define V8_OBJECTS_JS_DATA_OBJECT_BUILDER_H_

#include "src/common/globals.h"
#include "src/handles/handles.h"
#include "src/objects/elements-kind.h"
#include "src/objects/map.h"

namespace v8 {
namespace internal {

// An iterator for JSDataObjectBuilder which can lazily create internalized
// strings for the keys, and otherwise presents a vector of characters. Useful
// for creating objects in parsers.
//
// ```
// struct Iterator {
//   static constexpr bool kSupportsRawKeys = true;
//
//   // Whether the iterator might return the same key multiple times.
//   static constexpr bool kMayHaveDuplicateKeys;
//
//   void Advance();
//   bool Done();
//
//   // Get the characters of the key of the current property, assuming that
//   // the heap string is not yet allocated (e.g. during parsing).
//   base::Vector<const Char> GetKeyChars();
//
//   // Get the key of the current property, optionally returning the hinted
//   // expected key if applicable, to avoid an allocation.
//   Handle<InternalizedString> GetKey(
//       Handle<InternalizedString> expected_key_hint);
//
//   // Get the value of the current property. `will_revisit_value` is true
//   // if this value will need to be revisited later via RevisitValues().
//   Handle<Object> GetValue(bool will_revisit_value);
//
//   // Return an iterator over the values that were already visited by
//   // GetValue. Might require caching those values if necessary.
//   ValueIterator RevisitValues();
// }
// ```
template <typename PropertyIterator>
concept JSDataPropertyIteratorWithRawKeys =
    std::remove_reference_t<PropertyIterator>::kSupportsRawKeys &&
    requires(PropertyIterator it, Handle<InternalizedString> key_hint) {
      it.Advance();
      it.Done();
      it.kMayHaveDuplicateKeys;
      it.GetKeyChars();
      it.GetKey(key_hint);
      it.GetValue(true);
      it.RevisitValues();
    };

// An iterator for JSDataObjectBuilder which has already eagerly created
// internalized strings for the keys, Useful for creating objects from existing
// descriptors.
//
// ```
// struct Iterator {
//   static constexpr bool kSupportsRawKeys = false;
//
//   // Whether the iterator might return the same key multiple times.
//   static constexpr bool kMayHaveDuplicateKeys;
//
//   void Advance();
//   bool Done();
//
//   // Get the key of the current property, already allocated and
//   // internalized.
//   Handle<InternalizedString> GetKey();
//
//   // Get the value of the current property. `will_revisit_value` is true
//   // if this value will need to be revisited later via RevisitValues().
//   Handle<Object> GetValue(bool will_revisit_value);
//
//   // Return an iterator over the values that were already visited by
//   // GetValue. Might require caching those values if necessary.
//   ValueIterator RevisitValues();
// }
// ```
template <typename PropertyIterator>
concept JSDataPropertyIteratorWithEagerKeys =
    !std::remove_reference_t<PropertyIterator>::kSupportsRawKeys &&
    requires(PropertyIterator it) {
      it.Advance();
      it.Done();
      it.kMayHaveDuplicateKeys;
      it.GetKey();
      it.GetValue(true);
      it.RevisitValues();
    };

class JSDataObjectBuilder {
 public:
  // HeapNumberMode determines whether incoming HeapNumber values will be
  // guaranteed to be uniquely owned by this object, and therefore can be used
  // directly as mutable HeapNumbers for double representation fields.
  enum HeapNumberMode {
    kNormalHeapNumbers,
    kHeapNumbersGuaranteedUniquelyOwned
  };
  JSDataObjectBuilder(Isolate* isolate, ElementsKind elements_kind,
                      int expected_named_properties,
                      DirectHandle<Map> expected_final_map,
                      HeapNumberMode heap_number_mode);

  // Builds and returns an object whose properties are based on a property
  // iterator.
  template <JSDataPropertyIteratorWithRawKeys PropertyIterator>
  V8_INLINE Handle<JSObject> BuildFromIterator(
      PropertyIterator&& it,
      MaybeDirectHandle<FixedArrayBase> maybe_elements = {});

  template <JSDataPropertyIteratorWithEagerKeys PropertyIterator>
  V8_INLINE Handle<JSObject> BuildFromIterator(
      PropertyIterator&& it,
      MaybeDirectHandle<FixedArrayBase> maybe_elements = {});

 private:
  template <typename ValueIterator>
  V8_INLINE void CreateAndInitialiseObject(
      ValueIterator&& value_it, DirectHandle<FixedArrayBase> elements);

  void AddSlowProperty(DirectHandle<InternalizedString> key,
                       Handle<Object> value);

  template <typename Char, typename GetKeyFunction, typename GetValueFunction>
  V8_INLINE bool TryAddFastPropertyForValue(
      base::Vector<const Char> key_chars, GetKeyFunction&& get_key,
      GetValueFunction&& get_value, DirectHandle<InternalizedString>* out_key);

  V8_INLINE void RecalculateExtraHeapNumbersNeeded();

  V8_INLINE bool TryAddFastPropertyForValue(
      DirectHandle<InternalizedString> key, DirectHandle<Object> value);

  template <typename Char, typename GetKeyFunction>
  V8_INLINE bool TryFastTransitionToPropertyKey(
      base::Vector<const Char> key_chars, GetKeyFunction&& get_key,
      DirectHandle<InternalizedString>* out_key);

  V8_INLINE bool TryFastTransitionToPropertyKey(
      DirectHandle<InternalizedString> key);

  V8_INLINE bool TryAddFastPropertyTransitionForValue(
      DirectHandle<InternalizedString> key, DirectHandle<Object> value);

  V8_INLINE bool TryGeneralizeFieldToValue(DirectHandle<Object> value);

  bool TryInitializeMapFromExpectedFinalMap();

  void InitializeMapFromZero();

  V8_INLINE bool IsOnExpectedFinalMapFastPath() const;

  void RewindExpectedFinalMapFastPathToBeforeCurrent();

  void RewindExpectedFinalMapFastPathToIncludeCurrent();

  V8_INLINE void RegisterFieldNeedsFreshHeapNumber();
  V8_INLINE void RegisterFieldNeedsFreshHeapNumber(DirectHandle<Object> value);
  V8_INLINE void AdvanceToNextProperty();

  Isolate* isolate_;
  ElementsKind elements_kind_;
  int expected_property_count_;
  HeapNumberMode heap_number_mode_;
  bool may_have_duplicate_keys_ = true;

  DirectHandle<Map> map_;
  int current_property_index_ = 0;
  uint32_t extra_heap_numbers_needed_ = 0;

  Handle<JSObject> object_;

  DirectHandle<Map> expected_final_map_ = {};
  int property_count_in_expected_final_map_ = 0;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_OBJECTS_JS_DATA_OBJECT_BUILDER_H_
