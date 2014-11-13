// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TRANSITIONS_H_
#define V8_TRANSITIONS_H_

#include "src/checks.h"
#include "src/elements-kind.h"
#include "src/heap/heap.h"
#include "src/isolate.h"
#include "src/objects.h"

namespace v8 {
namespace internal {


// TransitionArrays are fixed arrays used to hold map transitions for property,
// constant, and element changes. They can either be simple transition arrays
// that store a single property transition, or a full transition array that has
// prototype transitions and multiple property transitons. The details related
// to property transitions are accessed in the descriptor array of the target
// map. In the case of a simple transition, the key is also read from the
// descriptor array of the target map.
//
// The simple format of the these objects is:
// [0] Undefined or back pointer map
// [1] Single transition
//
// The full format is:
// [0] Undefined or back pointer map
// [1] Smi(0) or fixed array of prototype transitions
// [2] Number of transitions
// [3] First transition
// [3 + number of transitions * kTransitionSize]: start of slack
class TransitionArray: public FixedArray {
 public:
  // Accessors for fetching instance transition at transition number.
  inline Name* GetKey(int transition_number);
  inline void SetKey(int transition_number, Name* value);
  inline Object** GetKeySlot(int transition_number);
  int GetSortedKeyIndex(int transition_number) { return transition_number; }

  Name* GetSortedKey(int transition_number) {
    return GetKey(transition_number);
  }

  inline Map* GetTarget(int transition_number);
  inline void SetTarget(int transition_number, Map* target);

  inline PropertyDetails GetTargetDetails(int transition_number);
  inline Object* GetTargetValue(int transition_number);

  inline bool HasElementsTransition();

  inline Object* back_pointer_storage();
  inline void set_back_pointer_storage(
      Object* back_pointer,
      WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline FixedArray* GetPrototypeTransitions();
  inline void SetPrototypeTransitions(
      FixedArray* prototype_transitions,
      WriteBarrierMode mode = UPDATE_WRITE_BARRIER);
  inline Object** GetPrototypeTransitionsSlot();
  inline bool HasPrototypeTransitions();

  // Returns the number of transitions in the array.
  int number_of_transitions() {
    if (IsSimpleTransition()) return 1;
    if (length() <= kFirstIndex) return 0;
    return Smi::cast(get(kTransitionLengthIndex))->value();
  }

  int number_of_transitions_storage() {
    if (IsSimpleTransition()) return 1;
    if (length() <= kFirstIndex) return 0;
    return (length() - kFirstIndex) / kTransitionSize;
  }

  int NumberOfSlackTransitions() {
    return number_of_transitions_storage() - number_of_transitions();
  }

  inline void SetNumberOfTransitions(int number_of_transitions);
  inline int number_of_entries() { return number_of_transitions(); }

  // Creates a FullTransitionArray from a SimpleTransitionArray in
  // containing_map.
  static Handle<TransitionArray> ExtendToFullTransitionArray(
      Handle<Map> containing_map);

  // Return a transition array, using the array from the owning map if it
  // already has one (copying into a larger array if necessary), otherwise
  // creating a new one according to flag.
  // TODO(verwaest): This should not cause an existing transition to be
  // overwritten.
  static Handle<TransitionArray> Insert(Handle<Map> map, Handle<Name> name,
                                        Handle<Map> target,
                                        SimpleTransitionFlag flag);
  // Search a  transition for a given type, property name and attributes.
  int Search(PropertyType type, Name* name, PropertyAttributes attributes,
             int* out_insertion_index = NULL);

  // Search a non-property transition (like elements kind, observe or frozen
  // transitions).
  inline int SearchSpecial(Symbol* symbol, int* out_insertion_index = NULL) {
    return SearchName(symbol, out_insertion_index);
  }

  static inline PropertyDetails GetTargetDetails(Name* name, Map* target);

  // Allocates a TransitionArray.
  static Handle<TransitionArray> Allocate(Isolate* isolate,
                                          int number_of_transitions,
                                          int slack = 0);

  bool IsSimpleTransition() {
    return length() == kSimpleTransitionSize &&
        get(kSimpleTransitionTarget)->IsHeapObject() &&
        // The IntrusivePrototypeTransitionIterator may have set the map of the
        // prototype transitions array to a smi. In that case, there are
        // prototype transitions, hence this transition array is a full
        // transition array.
        HeapObject::cast(get(kSimpleTransitionTarget))->map()->IsMap() &&
        get(kSimpleTransitionTarget)->IsMap();
  }

  bool IsFullTransitionArray() {
    return length() > kFirstIndex ||
        (length() == kFirstIndex && !IsSimpleTransition());
  }

  // Casting.
  static inline TransitionArray* cast(Object* obj);

  // Constant for denoting key was not found.
  static const int kNotFound = -1;

  static const int kBackPointerStorageIndex = 0;

  // Layout for full transition arrays.
  static const int kPrototypeTransitionsIndex = 1;
  static const int kTransitionLengthIndex = 2;
  static const int kFirstIndex = 3;

  // Layout for simple transition arrays.
  static const int kSimpleTransitionTarget = 1;
  static const int kSimpleTransitionSize = 2;
  static const int kSimpleTransitionIndex = 0;
  STATIC_ASSERT(kSimpleTransitionIndex != kNotFound);

  static const int kBackPointerStorageOffset = FixedArray::kHeaderSize;

  // Layout for the full transition array header.
  static const int kPrototypeTransitionsOffset = kBackPointerStorageOffset +
                                                 kPointerSize;
  static const int kTransitionLengthOffset =
      kPrototypeTransitionsOffset + kPointerSize;

  // Layout of map transition entries in full transition arrays.
  static const int kTransitionKey = 0;
  static const int kTransitionTarget = 1;
  static const int kTransitionSize = 2;

#ifdef OBJECT_PRINT
  // For our gdb macros, we should perhaps change these in the future.
  void Print();

  // Print all the transitions.
  void PrintTransitions(std::ostream& os, bool print_header = true);  // NOLINT
#endif

#ifdef DEBUG
  bool IsSortedNoDuplicates(int valid_entries = -1);
  bool IsConsistentWithBackPointers(Map* current_map);
  bool IsEqualTo(TransitionArray* other);

  // Returns true for a non-property transitions like elements kind, observed
  // or frozen transitions.
  static inline bool IsSpecialTransition(Name* name);
#endif

  // The maximum number of transitions we want in a transition array (should
  // fit in a page).
  static const int kMaxNumberOfTransitions = 1024 + 512;

  // Returns the fixed array length required to hold number_of_transitions
  // transitions.
  static int LengthFor(int number_of_transitions) {
    return ToKeyIndex(number_of_transitions);
  }

 private:
  // Conversion from transition number to array indices.
  static int ToKeyIndex(int transition_number) {
    return kFirstIndex +
           (transition_number * kTransitionSize) +
           kTransitionKey;
  }

  static int ToTargetIndex(int transition_number) {
    return kFirstIndex +
           (transition_number * kTransitionSize) +
           kTransitionTarget;
  }

  static Handle<TransitionArray> AllocateSimple(
      Isolate* isolate, Handle<Map> target);

  // Allocate a new transition array with a single entry.
  static Handle<TransitionArray> NewWith(Handle<Map> map,
                                         Handle<Name> name,
                                         Handle<Map> target,
                                         SimpleTransitionFlag flag);

  // Search a first transition for a given property name.
  inline int SearchName(Name* name, int* out_insertion_index = NULL);
  int SearchDetails(int transition, PropertyType type,
                    PropertyAttributes attributes, int* out_insertion_index);

  // Compares two tuples <key, is_data_property, attributes>, returns -1 if
  // tuple1 is "less" than tuple2, 0 if tuple1 equal to tuple2 and 1 otherwise.
  static inline int CompareKeys(Name* key1, uint32_t hash1,
                                bool is_data_property1,
                                PropertyAttributes attributes1, Name* key2,
                                uint32_t hash2, bool is_data_property2,
                                PropertyAttributes attributes2);

  // Compares keys, returns -1 if key1 is "less" than key2,
  // 0 if key1 equal to key2 and 1 otherwise.
  static inline int CompareNames(Name* key1, uint32_t hash1, Name* key2,
                                 uint32_t hash2);

  // Compares two details, returns -1 if details1 is "less" than details2,
  // 0 if details1 equal to details2 and 1 otherwise.
  static inline int CompareDetails(bool is_data_property1,
                                   PropertyAttributes attributes1,
                                   bool is_data_property2,
                                   PropertyAttributes attributes2);

  inline void NoIncrementalWriteBarrierSet(int transition_number,
                                           Name* key,
                                           Map* target);

  // Copy a single transition from the origin array.
  inline void NoIncrementalWriteBarrierCopyFrom(TransitionArray* origin,
                                                int origin_transition,
                                                int target_transition);

  DISALLOW_IMPLICIT_CONSTRUCTORS(TransitionArray);
};


} }  // namespace v8::internal

#endif  // V8_TRANSITIONS_H_
