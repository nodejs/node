// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_PROTOTYPE_INFO_H_
#define V8_OBJECTS_PROTOTYPE_INFO_H_

#include "src/objects/fixed-array.h"
#include "src/objects/objects.h"
#include "src/objects/struct.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

// Container for metadata stored on each prototype map.
class PrototypeInfo : public Struct {
 public:
  static const int UNREGISTERED = -1;

  // [module_namespace]: A backpointer to JSModuleNamespace from its
  // PrototypeInfo (or undefined). This field is only used for JSModuleNamespace
  // maps.  TODO(jkummerow): Figure out if there's a way to store the namespace
  // pointer elsewhere to save memory.
  DECL_ACCESSORS(module_namespace, Object)

  // [prototype_users]: WeakArrayList containing weak references to maps using
  // this prototype, or Smi(0) if uninitialized.
  DECL_ACCESSORS(prototype_users, Object)

  // [object_create_map]: A field caching the map for Object.create(prototype).
  static inline void SetObjectCreateMap(Handle<PrototypeInfo> info,
                                        Handle<Map> map);
  inline Map ObjectCreateMap();
  inline bool HasObjectCreateMap();

  // [registry_slot]: Slot in prototype's user registry where this user
  // is stored. Returns UNREGISTERED if this prototype has not been registered.
  inline int registry_slot() const;
  inline void set_registry_slot(int slot);

  // [bit_field]
  inline int bit_field() const;
  inline void set_bit_field(int bit_field);

  DECL_BOOLEAN_ACCESSORS(should_be_fast_map)

  DECL_CAST(PrototypeInfo)

  // Dispatched behavior.
  DECL_PRINTER(PrototypeInfo)
  DECL_VERIFIER(PrototypeInfo)

  DEFINE_FIELD_OFFSET_CONSTANTS(HeapObject::kHeaderSize,
                                TORQUE_GENERATED_PROTOTYPE_INFO_FIELDS)

  // Bit field usage.
  static const int kShouldBeFastBit = 0;

  class BodyDescriptor;

 private:
  DECL_ACCESSORS(object_create_map, MaybeObject)

  OBJECT_CONSTRUCTORS(PrototypeInfo, Struct);
};

// A growing array with an additional API for marking slots "empty". When adding
// new elements, we reuse the empty slots instead of growing the array.
class V8_EXPORT_PRIVATE PrototypeUsers : public WeakArrayList {
 public:
  static Handle<WeakArrayList> Add(Isolate* isolate,
                                   Handle<WeakArrayList> array,
                                   Handle<Map> value, int* assigned_index);

  static inline void MarkSlotEmpty(WeakArrayList array, int index);

  // The callback is called when a weak pointer to HeapObject "object" is moved
  // from index "from_index" to index "to_index" during compaction. The callback
  // must not cause GC.
  using CompactionCallback = void (*)(HeapObject object, int from_index,
                                      int to_index);
  static WeakArrayList Compact(
      Handle<WeakArrayList> array, Heap* heap, CompactionCallback callback,
      AllocationType allocation = AllocationType::kYoung);

#ifdef VERIFY_HEAP
  static void Verify(WeakArrayList array);
#endif  // VERIFY_HEAP

  static const int kEmptySlotIndex = 0;
  static const int kFirstIndex = 1;

  static const int kNoEmptySlotsMarker = 0;

 private:
  static inline Smi empty_slot_index(WeakArrayList array);
  static inline void set_empty_slot_index(WeakArrayList array, int index);

  static void ScanForEmptySlots(WeakArrayList array);

  DISALLOW_IMPLICIT_CONSTRUCTORS(PrototypeUsers);
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_PROTOTYPE_INFO_H_
