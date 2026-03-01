// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_JS_DATA_OBJECT_BUILDER_H_
#define V8_OBJECTS_JS_DATA_OBJECT_BUILDER_H_

#include "src/common/globals.h"
#include "src/handles/handles.h"
#include "src/objects/elements-kind.h"
#include "src/objects/js-objects.h"
#include "src/objects/map.h"

namespace v8 {
namespace internal {

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
  //
  // Expects an iterator of the form:
  //
  // struct Iterator {
  //   void Advance();
  //   bool Done();
  //
  //   // Get the key of the current property, optionally returning the hinted
  //   // expected key if applicable.
  //   Handle<String> GetKey(Handle<String> expected_key_hint);
  //
  //   // Get the value of the current property. `will_revisit_value` is true
  //   // if this value will need to be revisited later via RevisitValues().
  //   Handle<Object> GetValue(bool will_revisit_value);
  //
  //   // Return an iterator over the values that were already visited by
  //   // GetValue. Might require caching those values if necessary.
  //   ValueIterator RevisitValues();
  // }
  template <typename PropertyIterator>
  Handle<JSObject> BuildFromIterator(
      PropertyIterator&& it, MaybeHandle<FixedArrayBase> maybe_elements = {});

  template <typename ValueIterator>
  V8_INLINE void CreateAndInitialiseObject(
      ValueIterator value_it, DirectHandle<FixedArrayBase> elements);

  void AddSlowProperty(DirectHandle<String> key, Handle<Object> value);

  Handle<JSObject> object() {
    DCHECK(!object_.is_null());
    return object_;
  }

 private:
  template <typename Char, typename GetKeyFunction, typename GetValueFunction>
  V8_INLINE bool TryAddFastPropertyForValue(base::Vector<const Char> key_chars,
                                            GetKeyFunction&& get_key,
                                            GetValueFunction&& get_value);

  template <typename Char, typename GetKeyFunction>
  V8_INLINE bool TryFastTransitionToPropertyKey(
      base::Vector<const Char> key_chars, GetKeyFunction&& get_key,
      Handle<String>* key_out);

  V8_INLINE bool TryGeneralizeFieldToValue(DirectHandle<Object> value);

  bool TryInitializeMapFromExpectedFinalMap();

  void InitializeMapFromZero();

  V8_INLINE bool IsOnExpectedFinalMapFastPath() const {
    DCHECK_IMPLIES(property_count_in_expected_final_map_ > 0,
                   !expected_final_map_.is_null());
    return current_property_index_ < property_count_in_expected_final_map_;
  }

  void RewindExpectedFinalMapFastPathToBeforeCurrent();

  void RewindExpectedFinalMapFastPathToIncludeCurrent();

  V8_INLINE void RegisterFieldNeedsFreshHeapNumber(DirectHandle<Object> value);

  V8_INLINE void AdvanceToNextProperty() { current_property_index_++; }

  Isolate* isolate_;
  ElementsKind elements_kind_;
  int expected_property_count_;
  HeapNumberMode heap_number_mode_;

  DirectHandle<Map> map_;
  int current_property_index_ = 0;
  int extra_heap_numbers_needed_ = 0;

  Handle<JSObject> object_;

  DirectHandle<Map> expected_final_map_ = {};
  int property_count_in_expected_final_map_ = 0;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_OBJECTS_JS_DATA_OBJECT_BUILDER_H_
