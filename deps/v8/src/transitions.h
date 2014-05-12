// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TRANSITIONS_H_
#define V8_TRANSITIONS_H_

#include "elements-kind.h"
#include "heap.h"
#include "isolate.h"
#include "objects.h"
#include "v8checks.h"

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
// [2] First transition
// [length() - kTransitionSize] Last transition
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
    int len = length();
    return len <= kFirstIndex ? 0 : (len - kFirstIndex) / kTransitionSize;
  }

  inline int number_of_entries() { return number_of_transitions(); }

  // Creates a FullTransitionArray from a SimpleTransitionArray in
  // containing_map.
  static Handle<TransitionArray> ExtendToFullTransitionArray(
      Handle<Map> containing_map);

  // Create a transition array, copying from the owning map if it already has
  // one, otherwise creating a new one according to flag.
  // TODO(verwaest): This should not cause an existing transition to be
  // overwritten.
  static Handle<TransitionArray> CopyInsert(Handle<Map> map,
                                            Handle<Name> name,
                                            Handle<Map> target,
                                            SimpleTransitionFlag flag);

  // Search a transition for a given property name.
  inline int Search(Name* name);

  // Allocates a TransitionArray.
  static Handle<TransitionArray> Allocate(
      Isolate* isolate, int number_of_transitions);

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
  static const int kFirstIndex = 2;

  // Layout for simple transition arrays.
  static const int kSimpleTransitionTarget = 1;
  static const int kSimpleTransitionSize = 2;
  static const int kSimpleTransitionIndex = 0;
  STATIC_ASSERT(kSimpleTransitionIndex != kNotFound);

  static const int kBackPointerStorageOffset = FixedArray::kHeaderSize;

  // Layout for the full transition array header.
  static const int kPrototypeTransitionsOffset = kBackPointerStorageOffset +
                                                 kPointerSize;

  // Layout of map transition entries in full transition arrays.
  static const int kTransitionKey = 0;
  static const int kTransitionTarget = 1;
  static const int kTransitionSize = 2;

#ifdef OBJECT_PRINT
  // Print all the transitions.
  inline void PrintTransitions() {
    PrintTransitions(stdout);
  }
  void PrintTransitions(FILE* out);
#endif

#ifdef DEBUG
  bool IsSortedNoDuplicates(int valid_entries = -1);
  bool IsConsistentWithBackPointers(Map* current_map);
  bool IsEqualTo(TransitionArray* other);
#endif

  // The maximum number of transitions we want in a transition array (should
  // fit in a page).
  static const int kMaxNumberOfTransitions = 1024 + 512;

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
