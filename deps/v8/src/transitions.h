// Copyright 2012 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

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
// space for elements transitions, prototype transitions and multiple property
// transitons. The details related to property transitions are accessed in the
// descriptor array of the target map. In the case of a simple transition, the
// key is also read from the descriptor array of the target map.
//
// The simple format of the these objects is:
// [0] Undefined or back pointer map
// [1] Single transition
//
// The full format is:
// [0] Undefined or back pointer map
// [1] Smi(0) or elements transition map
// [2] Smi(0) or fixed array of prototype transitions
// [3] First transition
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

  inline Map* elements_transition();
  inline void set_elements_transition(
      Map* target,
      WriteBarrierMode mode = UPDATE_WRITE_BARRIER);
  inline bool HasElementsTransition();
  inline void ClearElementsTransition();

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
  inline HeapObject* UncheckedPrototypeTransitions();

  // Returns the number of transitions in the array.
  int number_of_transitions() {
    if (IsSimpleTransition()) return 1;
    int len = length();
    return len <= kFirstIndex ? 0 : (len - kFirstIndex) / kTransitionSize;
  }

  inline int number_of_entries() { return number_of_transitions(); }

  // Allocate a new transition array with a single entry.
  static MUST_USE_RESULT MaybeObject* NewWith(
      SimpleTransitionFlag flag,
      Name* key,
      Map* target,
      Object* back_pointer);

  MUST_USE_RESULT MaybeObject* ExtendToFullTransitionArray();

  // Copy the transition array, inserting a new transition.
  // TODO(verwaest): This should not cause an existing transition to be
  // overwritten.
  MUST_USE_RESULT MaybeObject* CopyInsert(Name* name, Map* target);

  // Copy a single transition from the origin array.
  inline void NoIncrementalWriteBarrierCopyFrom(TransitionArray* origin,
                                                int origin_transition,
                                                int target_transition);

  // Search a transition for a given property name.
  inline int Search(Name* name);

  // Allocates a TransitionArray.
  MUST_USE_RESULT static MaybeObject* Allocate(int number_of_transitions);

  bool IsSimpleTransition() { return length() == kSimpleTransitionSize; }
  bool IsFullTransitionArray() { return length() >= kFirstIndex; }

  // Casting.
  static inline TransitionArray* cast(Object* obj);

  // Constant for denoting key was not found.
  static const int kNotFound = -1;

  static const int kBackPointerStorageIndex = 0;

  // Layout for full transition arrays.
  static const int kElementsTransitionIndex = 1;
  static const int kPrototypeTransitionsIndex = 2;
  static const int kFirstIndex = 3;

  // Layout for simple transition arrays.
  static const int kSimpleTransitionTarget = 1;
  static const int kSimpleTransitionSize = 2;
  static const int kSimpleTransitionIndex = 0;
  STATIC_ASSERT(kSimpleTransitionIndex != kNotFound);

  static const int kBackPointerStorageOffset = FixedArray::kHeaderSize;

  // Layout for the full transition array header.
  static const int kElementsTransitionOffset = kBackPointerStorageOffset +
                                               kPointerSize;
  static const int kPrototypeTransitionsOffset = kElementsTransitionOffset +
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

  inline void NoIncrementalWriteBarrierSet(int transition_number,
                                           Name* key,
                                           Map* target);

  DISALLOW_IMPLICIT_CONSTRUCTORS(TransitionArray);
};


} }  // namespace v8::internal

#endif  // V8_TRANSITIONS_H_
