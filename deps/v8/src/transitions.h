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
// constant, and element changes. "Simple" transitions storing only a single
// property transition are stored inline (i.e. the target map is stored
// directly); otherwise a full transition array is used that has
// prototype transitions and multiple property transitons. The details related
// to property transitions are accessed in the descriptor array of the target
// map. In the case of a simple transition, the key is also read from the
// descriptor array of the target map.
//
// This class provides a static interface that operates directly on maps
// and handles the distinction between simple and full transitions storage.
//
// The full format is:
// [0] Smi(0) or fixed array of prototype transitions
// [1] Number of transitions
// [2] First transition
// [2 + number of transitions * kTransitionSize]: start of slack
class TransitionArray: public FixedArray {
 public:
  // Insert a new transition into |map|'s transition array, extending it
  // as necessary.
  static void Insert(Handle<Map> map, Handle<Name> name, Handle<Map> target,
                     SimpleTransitionFlag flag);

  static Map* SearchTransition(Map* map, PropertyKind kind, Name* name,
                               PropertyAttributes attributes);
  static MaybeHandle<Map> SearchTransition(Handle<Map> map, PropertyKind kind,
                                           Handle<Name> name,
                                           PropertyAttributes attributes) {
    if (Map* transition = SearchTransition(*map, kind, *name, attributes)) {
      return handle(transition);
    }
    return MaybeHandle<Map>();
  }

  static Map* SearchSpecial(Map* map, Symbol* name);

  static Handle<Map> FindTransitionToField(Handle<Map> map, Handle<Name> name);

  static Handle<String> ExpectedTransitionKey(Handle<Map> map);

  static Handle<Map> ExpectedTransitionTarget(Handle<Map> map) {
    DCHECK(!ExpectedTransitionKey(map).is_null());
    return Handle<Map>(GetSimpleTransition(map->raw_transitions()));
  }
  // Returns true if |raw_transition| can be overwritten with a simple
  // transition (because it's either uninitialized, or has been cleared).
  static inline bool CanStoreSimpleTransition(Object* raw_transition) {
    return raw_transition->IsSmi() ||
           (raw_transition->IsWeakCell() &&
            WeakCell::cast(raw_transition)->cleared());
  }
  static inline bool IsSimpleTransition(Object* raw_transition) {
    DCHECK(!raw_transition->IsWeakCell() ||
           WeakCell::cast(raw_transition)->cleared() ||
           WeakCell::cast(raw_transition)->value()->IsMap());
    return raw_transition->IsWeakCell() &&
           !WeakCell::cast(raw_transition)->cleared();
  }
  static inline Map* GetSimpleTransition(Object* raw_transition) {
    DCHECK(IsSimpleTransition(raw_transition));
    DCHECK(raw_transition->IsWeakCell());
    return Map::cast(WeakCell::cast(raw_transition)->value());
  }
  static inline bool IsFullTransitionArray(Object* raw_transitions) {
    return raw_transitions->IsTransitionArray();
  }

  // The size of transition arrays are limited so they do not end up in large
  // object space. Otherwise ClearNonLiveReferences would leak memory while
  // applying in-place right trimming.
  static bool CanHaveMoreTransitions(Handle<Map> map);

  // ===== PROTOTYPE TRANSITIONS =====
  // When you set the prototype of an object using the __proto__ accessor you
  // need a new map for the object (the prototype is stored in the map).  In
  // order not to multiply maps unnecessarily we store these as transitions in
  // the original map.  That way we can transition to the same map if the same
  // prototype is set, rather than creating a new map every time.  The
  // transitions are in the form of a map where the keys are prototype objects
  // and the values are the maps they transition to.
  // Cache format:
  //    0: finger - index of the first free cell in the cache
  //    1 + i: target map
  static const int kMaxCachedPrototypeTransitions = 256;
  static void PutPrototypeTransition(Handle<Map> map, Handle<Object> prototype,
                                     Handle<Map> target_map);

  static Handle<Map> GetPrototypeTransition(Handle<Map> map,
                                            Handle<Object> prototype);

  static FixedArray* GetPrototypeTransitions(Map* map);

  static int NumberOfPrototypeTransitions(FixedArray* proto_transitions) {
    if (proto_transitions->length() == 0) return 0;
    Object* raw = proto_transitions->get(kProtoTransitionNumberOfEntriesOffset);
    return Smi::cast(raw)->value();
  }
  static int NumberOfPrototypeTransitionsForTest(Map* map);

  static void SetNumberOfPrototypeTransitions(FixedArray* proto_transitions,
                                              int value);

  inline FixedArray* GetPrototypeTransitions();
  inline void SetPrototypeTransitions(FixedArray* prototype_transitions);
  inline Object** GetPrototypeTransitionsSlot();
  inline bool HasPrototypeTransitions();

  // ===== ITERATION =====

  typedef void (*TraverseCallback)(Map* map, void* data);

  // Traverse the transition tree in postorder.
  static void TraverseTransitionTree(Map* map, TraverseCallback callback,
                                     void* data) {
    // Make sure that we do not allocate in the callback.
    DisallowHeapAllocation no_allocation;
    TraverseTransitionTreeInternal(map, callback, data);
  }

  // ===== LOW-LEVEL ACCESSORS =====

  // Accessors for fetching instance transition at transition number.
  static inline Name* GetKey(Object* raw_transitions, int transition_number);
  inline Name* GetKey(int transition_number);
  inline void SetKey(int transition_number, Name* value);
  inline Object** GetKeySlot(int transition_number);
  int GetSortedKeyIndex(int transition_number) { return transition_number; }

  Name* GetSortedKey(int transition_number) {
    return GetKey(transition_number);
  }

  static inline Map* GetTarget(Object* raw_transitions, int transition_number);
  inline Map* GetTarget(int transition_number);
  inline void SetTarget(int transition_number, Map* target);

  static inline PropertyDetails GetTargetDetails(Name* name, Map* target);

  // Returns the number of transitions in the array.
  static int NumberOfTransitions(Object* raw_transitions);
  // Required for templatized Search interface.
  inline int number_of_entries() { return number_of_transitions(); }

  inline void SetNumberOfTransitions(int number_of_transitions);

  static int Capacity(Object* raw_transitions);

  inline static TransitionArray* cast(Object* object);

  // This field should be used only by GC.
  inline void set_next_link(Object* next, WriteBarrierMode mode);
  inline Object* next_link();

  static const int kTransitionSize = 2;
  static const int kProtoTransitionHeaderSize = 1;

#if defined(DEBUG) || defined(OBJECT_PRINT)
  // For our gdb macros, we should perhaps change these in the future.
  void Print();

  // Print all the transitions.
  static void PrintTransitions(std::ostream& os, Object* transitions,
                               bool print_header = true);  // NOLINT
#endif

#ifdef OBJECT_PRINT
  void TransitionArrayPrint(std::ostream& os);  // NOLINT
#endif

#ifdef VERIFY_HEAP
  void TransitionArrayVerify();
#endif

#ifdef DEBUG
  bool IsSortedNoDuplicates(int valid_entries = -1);
  static bool IsSortedNoDuplicates(Map* map);
  static bool IsConsistentWithBackPointers(Map* map);

  // Returns true for a non-property transitions like elements kind, observed
  // or frozen transitions.
  static inline bool IsSpecialTransition(Name* name);
#endif

  // Constant for denoting key was not found.
  static const int kNotFound = -1;

  // The maximum number of transitions we want in a transition array (should
  // fit in a page).
  static const int kMaxNumberOfTransitions = 1024 + 512;

 private:
  // Layout for full transition arrays.
  static const int kNextLinkIndex = 0;
  static const int kPrototypeTransitionsIndex = 1;
  static const int kTransitionLengthIndex = 2;
  static const int kFirstIndex = 3;

  // Layout of map transition entries in full transition arrays.
  static const int kTransitionKey = 0;
  static const int kTransitionTarget = 1;
  STATIC_ASSERT(kTransitionSize == 2);

  static const int kProtoTransitionNumberOfEntriesOffset = 0;
  STATIC_ASSERT(kProtoTransitionHeaderSize == 1);

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

  // Returns the fixed array length required to hold number_of_transitions
  // transitions.
  static int LengthFor(int number_of_transitions) {
    return ToKeyIndex(number_of_transitions);
  }

  // Allocates a TransitionArray.
  static Handle<TransitionArray> Allocate(Isolate* isolate,
                                          int number_of_transitions,
                                          int slack = 0);

  static void EnsureHasFullTransitionArray(Handle<Map> map);
  static void ReplaceTransitions(Handle<Map> map, Object* new_transitions);

  // Search a  transition for a given kind, property name and attributes.
  int Search(PropertyKind kind, Name* name, PropertyAttributes attributes,
             int* out_insertion_index = NULL);

  // Search a non-property transition (like elements kind, observe or frozen
  // transitions).
  inline int SearchSpecial(Symbol* symbol, int* out_insertion_index = NULL) {
    return SearchName(symbol, out_insertion_index);
  }
  // Search a first transition for a given property name.
  inline int SearchName(Name* name, int* out_insertion_index = NULL);
  int SearchDetails(int transition, PropertyKind kind,
                    PropertyAttributes attributes, int* out_insertion_index);

  int number_of_transitions() {
    if (length() < kFirstIndex) return 0;
    return Smi::cast(get(kTransitionLengthIndex))->value();
  }

  static inline PropertyDetails GetSimpleTargetDetails(Map* transition) {
    return transition->GetLastDescriptorDetails();
  }

  static inline Name* GetSimpleTransitionKey(Map* transition) {
    int descriptor = transition->LastAdded();
    return transition->instance_descriptors()->GetKey(descriptor);
  }

  static void TraverseTransitionTreeInternal(Map* map,
                                             TraverseCallback callback,
                                             void* data);

  static void SetPrototypeTransitions(Handle<Map> map,
                                      Handle<FixedArray> proto_transitions);

  static bool CompactPrototypeTransitionArray(FixedArray* array);

  static Handle<FixedArray> GrowPrototypeTransitionArray(
      Handle<FixedArray> array, int new_capacity, Isolate* isolate);

  // Compares two tuples <key, kind, attributes>, returns -1 if
  // tuple1 is "less" than tuple2, 0 if tuple1 equal to tuple2 and 1 otherwise.
  static inline int CompareKeys(Name* key1, uint32_t hash1, PropertyKind kind1,
                                PropertyAttributes attributes1, Name* key2,
                                uint32_t hash2, PropertyKind kind2,
                                PropertyAttributes attributes2);

  // Compares keys, returns -1 if key1 is "less" than key2,
  // 0 if key1 equal to key2 and 1 otherwise.
  static inline int CompareNames(Name* key1, uint32_t hash1, Name* key2,
                                 uint32_t hash2);

  // Compares two details, returns -1 if details1 is "less" than details2,
  // 0 if details1 equal to details2 and 1 otherwise.
  static inline int CompareDetails(PropertyKind kind1,
                                   PropertyAttributes attributes1,
                                   PropertyKind kind2,
                                   PropertyAttributes attributes2);

  inline void Set(int transition_number, Name* key, Map* target);

#ifdef DEBUG
  static void CheckNewTransitionsAreConsistent(Handle<Map> map,
                                               TransitionArray* old_transitions,
                                               Object* transitions);
#endif
  static void ZapTransitionArray(TransitionArray* transitions);

  DISALLOW_IMPLICIT_CONSTRUCTORS(TransitionArray);
};


}  // namespace internal
}  // namespace v8

#endif  // V8_TRANSITIONS_H_
