// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TRANSITIONS_H_
#define V8_TRANSITIONS_H_

#include "src/checks.h"
#include "src/elements-kind.h"
#include "src/objects.h"
#include "src/objects/descriptor-array.h"
#include "src/objects/map.h"
#include "src/objects/maybe-object.h"
#include "src/objects/name.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

// TransitionsAccessor is a helper class to encapsulate access to the various
// ways a Map can store transitions to other maps in its respective field at
// Map::kTransitionsOrPrototypeInfo.
// It caches state information internally, which becomes stale when a Map's
// transitions storage changes or when a GC cycle clears dead transitions;
// so while a TransitionsAccessor instance can be used for several read-only
// operations in a row (provided no GC happens between them), it must be
// discarded and recreated after "Insert" and "UpdateHandler" operations.
//
// Internal details: a Map's field either holds an in-place weak reference to a
// transition target, or a StoreIC handler for a transitioning store (which in
// turn points to its target map), or a TransitionArray for several target maps
// and/or handlers as well as prototype and ElementsKind transitions.  Property
// details (and in case of inline target storage, the key) are retrieved from
// the target map's descriptor array.  Stored transitions are weak in the GC
// sense: both single transitions stored inline and TransitionArray fields are
// cleared when the map they refer to is not otherwise reachable.
class TransitionsAccessor {
 public:
  TransitionsAccessor(Isolate* isolate, Map map, DisallowHeapAllocation* no_gc)
      : isolate_(isolate), map_(map) {
    Initialize();
    USE(no_gc);
  }
  TransitionsAccessor(Isolate* isolate, Handle<Map> map)
      : isolate_(isolate), map_handle_(map), map_(*map) {
    Initialize();
  }

  // Insert a new transition into |map|'s transition array, extending it
  // as necessary.
  // Requires the constructor that takes a Handle<Map> to have been used.
  // This TransitionsAccessor instance is unusable after this operation.
  void Insert(Handle<Name> name, Handle<Map> target, SimpleTransitionFlag flag);

  Map SearchTransition(Name name, PropertyKind kind,
                       PropertyAttributes attributes);

  Map SearchSpecial(Symbol name);
  // Returns true for non-property transitions like elements kind, or
  // or frozen/sealed transitions.
  static bool IsSpecialTransition(ReadOnlyRoots roots, Name name);

  enum RequestedLocation { kAnyLocation, kFieldOnly };
  MaybeHandle<Map> FindTransitionToDataProperty(
      Handle<Name> name, RequestedLocation requested_location = kAnyLocation);

  MaybeHandle<Map> FindTransitionToField(Handle<Name> name) {
    return FindTransitionToDataProperty(name, kFieldOnly);
  }

  Handle<String> ExpectedTransitionKey();
  Handle<Map> ExpectedTransitionTarget();

  int NumberOfTransitions();
  // The size of transition arrays are limited so they do not end up in large
  // object space. Otherwise ClearNonLiveReferences would leak memory while
  // applying in-place right trimming.
  static const int kMaxNumberOfTransitions = 1024 + 512;
  bool CanHaveMoreTransitions();
  inline Name GetKey(int transition_number);
  inline Map GetTarget(int transition_number);
  static inline PropertyDetails GetTargetDetails(Name name, Map target);

  static bool IsMatchingMap(Map target, Name name, PropertyKind kind,
                            PropertyAttributes attributes);

  bool HasIntegrityLevelTransitionTo(
      Map to, Symbol* out_symbol = nullptr,
      PropertyAttributes* out_integrity_level = nullptr);

  // ===== ITERATION =====
  typedef void (*TraverseCallback)(Map map, void* data);

  // Traverse the transition tree in postorder.
  void TraverseTransitionTree(TraverseCallback callback, void* data) {
    // Make sure that we do not allocate in the callback.
    DisallowHeapAllocation no_allocation;
    TraverseTransitionTreeInternal(callback, data, &no_allocation);
  }

  // ===== PROTOTYPE TRANSITIONS =====
  // When you set the prototype of an object using the __proto__ accessor you
  // need a new map for the object (the prototype is stored in the map).  In
  // order not to multiply maps unnecessarily we store these as transitions in
  // the original map.  That way we can transition to the same map if the same
  // prototype is set, rather than creating a new map every time.  The
  // transitions are in the form of a map where the keys are prototype objects
  // and the values are the maps they transition to.
  void PutPrototypeTransition(Handle<Object> prototype, Handle<Map> target_map);
  Handle<Map> GetPrototypeTransition(Handle<Object> prototype);

  // During the first-time Map::Update and Map::TryUpdate, the migration target
  // map could be cached in the raw_transitions slot of the old map that is
  // deprecated from the map transition tree. The next time old map is updated,
  // we will check this cache slot as a shortcut to get the migration target
  // map.
  void SetMigrationTarget(Map migration_target);
  Map GetMigrationTarget();

#if DEBUG || OBJECT_PRINT
  void PrintTransitions(std::ostream& os);
  static void PrintOneTransition(std::ostream& os, Name key, Map target);
  void PrintTransitionTree();
  void PrintTransitionTree(std::ostream& os, int level,
                           DisallowHeapAllocation* no_gc);
#endif
#if DEBUG
  void CheckNewTransitionsAreConsistent(TransitionArray old_transitions,
                                        Object transitions);
  bool IsConsistentWithBackPointers();
  bool IsSortedNoDuplicates();
#endif

 protected:
  // Allow tests to use inheritance to access internals.
  enum Encoding {
    kPrototypeInfo,
    kUninitialized,
    kMigrationTarget,
    kWeakRef,
    kFullTransitionArray,
  };

  void Reload() {
    DCHECK(!map_handle_.is_null());
    map_ = *map_handle_;
    Initialize();
  }

  inline Encoding encoding() {
    DCHECK(!needs_reload_);
    return encoding_;
  }

 private:
  friend class MarkCompactCollector;  // For HasSimpleTransitionTo.
  friend class TransitionArray;

  static inline PropertyDetails GetSimpleTargetDetails(Map transition);

  static inline Name GetSimpleTransitionKey(Map transition);

  static inline Map GetTargetFromRaw(MaybeObject raw);

  void MarkNeedsReload() {
#if DEBUG
    needs_reload_ = true;
#endif
  }

  void Initialize();

  inline Map GetSimpleTransition();
  bool HasSimpleTransitionTo(Map map);

  void ReplaceTransitions(MaybeObject new_transitions);

  inline Map GetTargetMapFromWeakRef();

  void EnsureHasFullTransitionArray();
  void SetPrototypeTransitions(Handle<WeakFixedArray> proto_transitions);
  WeakFixedArray GetPrototypeTransitions();

  void TraverseTransitionTreeInternal(TraverseCallback callback, void* data,
                                      DisallowHeapAllocation* no_gc);

  inline TransitionArray transitions();

  Isolate* isolate_;
  Handle<Map> map_handle_;
  Map map_;
  MaybeObject raw_transitions_;
  Encoding encoding_;
#if DEBUG
  bool needs_reload_;
#endif

  DISALLOW_IMPLICIT_CONSTRUCTORS(TransitionsAccessor);
};

// TransitionArrays are fixed arrays used to hold map transitions for property,
// constant, and element changes.
// The TransitionArray class exposes a very low-level interface. Most clients
// should use TransitionsAccessors.
// TransitionArrays have the following format:
// [0] Link to next TransitionArray (for weak handling support) (strong ref)
// [1] Smi(0) or WeakFixedArray of prototype transitions (strong ref)
// [2] Number of transitions (can be zero after trimming)
// [3] First transition key (strong ref)
// [4] First transition target (weak ref)
// ...
// [3 + number of transitions * kTransitionSize]: start of slack
class TransitionArray : public WeakFixedArray {
 public:
  DECL_CAST(TransitionArray)

  inline WeakFixedArray GetPrototypeTransitions();
  inline bool HasPrototypeTransitions();

  // Accessors for fetching instance transition at transition number.
  inline void SetKey(int transition_number, Name value);
  inline Name GetKey(int transition_number);
  inline HeapObjectSlot GetKeySlot(int transition_number);

  inline Map GetTarget(int transition_number);
  inline void SetRawTarget(int transition_number, MaybeObject target);
  inline MaybeObject GetRawTarget(int transition_number);
  inline HeapObjectSlot GetTargetSlot(int transition_number);
  inline bool GetTargetIfExists(int transition_number, Isolate* isolate,
                                Map* target);

  // Required for templatized Search interface.
  static constexpr int kNotFound = -1;

  inline Name GetSortedKey(int transition_number);
  int GetSortedKeyIndex(int transition_number) { return transition_number; }
  inline int number_of_entries() const;
#ifdef DEBUG
  bool IsSortedNoDuplicates(int valid_entries = -1);
#endif

  void Sort();

  void PrintInternal(std::ostream& os);

  DECL_PRINTER(TransitionArray)
  DECL_VERIFIER(TransitionArray)

  // Layout for full transition arrays.
  static const int kPrototypeTransitionsIndex = 0;
  static const int kTransitionLengthIndex = 1;
  static const int kFirstIndex = 2;

  // Layout of map transition entries in full transition arrays.
  static const int kEntryKeyIndex = 0;
  static const int kEntryTargetIndex = 1;
  static const int kEntrySize = 2;

  // Conversion from transition number to array indices.
  static int ToKeyIndex(int transition_number) {
    return kFirstIndex + (transition_number * kEntrySize) + kEntryKeyIndex;
  }

  static int ToTargetIndex(int transition_number) {
    return kFirstIndex + (transition_number * kEntrySize) + kEntryTargetIndex;
  }

  inline int SearchNameForTesting(Name name,
                                  int* out_insertion_index = nullptr);

 private:
  friend class Factory;
  friend class MarkCompactCollector;
  friend class TransitionsAccessor;

  inline void SetNumberOfTransitions(int number_of_transitions);

  inline int Capacity();

  // ===== PROTOTYPE TRANSITIONS =====
  // Cache format:
  //    0: finger - index of the first free cell in the cache
  //    1 + i: target map
  static const int kProtoTransitionHeaderSize = 1;
  static const int kMaxCachedPrototypeTransitions = 256;

  inline void SetPrototypeTransitions(WeakFixedArray prototype_transitions);

  static inline int NumberOfPrototypeTransitions(
      WeakFixedArray proto_transitions);
  static void SetNumberOfPrototypeTransitions(WeakFixedArray proto_transitions,
                                              int value);

  static const int kProtoTransitionNumberOfEntriesOffset = 0;
  STATIC_ASSERT(kProtoTransitionHeaderSize == 1);

  // Returns the fixed array length required to hold number_of_transitions
  // transitions.
  static int LengthFor(int number_of_transitions) {
    return ToKeyIndex(number_of_transitions);
  }

  // Search a  transition for a given kind, property name and attributes.
  int Search(PropertyKind kind, Name name, PropertyAttributes attributes,
             int* out_insertion_index = nullptr);

  Map SearchAndGetTarget(PropertyKind kind, Name name,
                         PropertyAttributes attributes);

  // Search a non-property transition (like elements kind, observe or frozen
  // transitions).
  inline int SearchSpecial(Symbol symbol, int* out_insertion_index = nullptr);
  // Search a first transition for a given property name.
  inline int SearchName(Name name, int* out_insertion_index = nullptr);
  int SearchDetails(int transition, PropertyKind kind,
                    PropertyAttributes attributes, int* out_insertion_index);
  Map SearchDetailsAndGetTarget(int transition, PropertyKind kind,
                                PropertyAttributes attributes);

  inline int number_of_transitions() const;

  static bool CompactPrototypeTransitionArray(Isolate* isolate,
                                              WeakFixedArray array);

  static Handle<WeakFixedArray> GrowPrototypeTransitionArray(
      Handle<WeakFixedArray> array, int new_capacity, Isolate* isolate);

  // Compares two tuples <key, kind, attributes>, returns -1 if
  // tuple1 is "less" than tuple2, 0 if tuple1 equal to tuple2 and 1 otherwise.
  static inline int CompareKeys(Name key1, uint32_t hash1, PropertyKind kind1,
                                PropertyAttributes attributes1, Name key2,
                                uint32_t hash2, PropertyKind kind2,
                                PropertyAttributes attributes2);

  // Compares keys, returns -1 if key1 is "less" than key2,
  // 0 if key1 equal to key2 and 1 otherwise.
  static inline int CompareNames(Name key1, uint32_t hash1, Name key2,
                                 uint32_t hash2);

  // Compares two details, returns -1 if details1 is "less" than details2,
  // 0 if details1 equal to details2 and 1 otherwise.
  static inline int CompareDetails(PropertyKind kind1,
                                   PropertyAttributes attributes1,
                                   PropertyKind kind2,
                                   PropertyAttributes attributes2);

  inline void Set(int transition_number, Name key, MaybeObject target);

  void Zap(Isolate* isolate);

  OBJECT_CONSTRUCTORS(TransitionArray, WeakFixedArray);
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_TRANSITIONS_H_
