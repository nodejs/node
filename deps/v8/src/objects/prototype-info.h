// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_PROTOTYPE_INFO_H_
#define V8_OBJECTS_PROTOTYPE_INFO_H_

#include "src/objects/fixed-array.h"
#include "src/objects/objects.h"
#include "src/objects/struct.h"
#include "torque-generated/bit-fields.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

class PrototypeSharedClosureInfo;

#include "torque-generated/src/objects/prototype-info-tq.inc"

// Container for metadata stored on each prototype map.
V8_OBJECT class PrototypeInfo : public StructLayout {
 public:
  // Accessors
  inline Tagged<UnionOf<JSModuleNamespace, Undefined>> module_namespace() const;
  inline void set_module_namespace(
      Tagged<UnionOf<JSModuleNamespace, Undefined>> value,
      WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<UnionOf<WeakArrayList, Zero>> prototype_users() const;
  inline void set_prototype_users(Tagged<UnionOf<WeakArrayList, Zero>> value,
                                  WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<UnionOf<FixedArray, Zero, Undefined>>
  prototype_chain_enum_cache() const;
  inline void set_prototype_chain_enum_cache(
      Tagged<UnionOf<FixedArray, Zero, Undefined>> value,
      WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline int registry_slot() const;
  inline void set_registry_slot(int value);

  inline int bit_field() const;
  inline void set_bit_field(int value);

  // For caching derived maps for Object.create, Reflect.construct and proxies.
  inline Tagged<UnionOf<WeakArrayList, Undefined>> derived_maps() const;
  inline Tagged<UnionOf<WeakArrayList, Undefined>> derived_maps(
      AcquireLoadTag) const;
  inline void set_derived_maps(Tagged<UnionOf<WeakArrayList, Undefined>> value,
                               WriteBarrierMode mode = UPDATE_WRITE_BARRIER);
  inline void set_derived_maps(Tagged<UnionOf<WeakArrayList, Undefined>> value,
                               ReleaseStoreTag,
                               WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<UnionOf<PrototypeSharedClosureInfo, Undefined>>
  prototype_shared_closure_info() const;
  inline void set_prototype_shared_closure_info(
      Tagged<UnionOf<PrototypeSharedClosureInfo, Undefined>> value,
      WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<UnionOf<LoadHandler, Zero>> cached_handler(int index) const;
  inline void set_cached_handler(int index,
                                 Tagged<UnionOf<LoadHandler, Zero>> value,
                                 WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  // Cached most commonly used load-non-existent handlers.
  enum CachedHandlerIndex {
    // No access check, no lookup on receiver.
    kLoadNonExistentHandlerDefault = 0,
    kLoadNonExistentHandlerWithLookupOnReceiver,

    kCachedHandlerCount,
  };

  static const int UNREGISTERED = -1;

  static inline void SetObjectCreateMap(DirectHandle<PrototypeInfo> info,
                                        DirectHandle<Map> map,
                                        Isolate* isolate);
  inline Tagged<MaybeObject> ObjectCreateMap(AcquireLoadTag);
  inline Tagged<MaybeObject> ObjectCreateMap();

  static inline void AddDerivedMap(DirectHandle<PrototypeInfo> info,
                                   DirectHandle<Map> to, Isolate* isolate);
  inline Tagged<MaybeObject> GetDerivedMap(DirectHandle<Map> from);

  static inline bool IsPrototypeInfoFast(Tagged<Object> object);

  DECL_BOOLEAN_ACCESSORS(should_be_fast_map)

  // Dispatched behavior.
  DECL_PRINTER(PrototypeInfo)
  DECL_VERIFIER(PrototypeInfo)

  // Bit field usage.
  DEFINE_TORQUE_GENERATED_PROTOTYPE_INFO_FLAGS()

  class BodyDescriptor;

 public:
  TaggedMember<UnionOf<JSModuleNamespace, Undefined>> module_namespace_;
  TaggedMember<UnionOf<WeakArrayList, Zero>> prototype_users_;
  TaggedMember<UnionOf<FixedArray, Zero, Undefined>>
      prototype_chain_enum_cache_;
  TaggedMember<Smi> registry_slot_;
  TaggedMember<Smi> bit_field_;
  TaggedMember<UnionOf<WeakArrayList, Undefined>> derived_maps_;
  TaggedMember<UnionOf<PrototypeSharedClosureInfo, Undefined>>
      prototype_shared_closure_info_;
  TaggedMember<UnionOf<LoadHandler, Zero>> cached_handler_[kCachedHandlerCount];
} V8_OBJECT_END;

// A growing array with an additional API for marking slots "empty". When adding
// new elements, we reuse the empty slots instead of growing the array.
class V8_EXPORT_PRIVATE PrototypeUsers : public WeakArrayList {
 public:
  static Handle<WeakArrayList> Add(Isolate* isolate,
                                   Handle<WeakArrayList> array,
                                   DirectHandle<Map> value,
                                   int* assigned_index);

  static inline void MarkSlotEmpty(Tagged<WeakArrayList> array, int index);

  // The callback is called when a weak pointer to HeapObject "object" is moved
  // from index "from_index" to index "to_index" during compaction. The callback
  // must not cause GC.
  using CompactionCallback = void (*)(Tagged<HeapObject> object, int from_index,
                                      int to_index);
  static Tagged<WeakArrayList> Compact(
      DirectHandle<WeakArrayList> array, Heap* heap,
      CompactionCallback callback,
      AllocationType allocation = AllocationType::kYoung);

#ifdef VERIFY_HEAP
  static void Verify(Tagged<WeakArrayList> array);
#endif  // VERIFY_HEAP

  static const int kEmptySlotIndex = 0;
  static const int kFirstIndex = 1;

  static const int kNoEmptySlotsMarker = 0;

 private:
  static inline Tagged<Smi> empty_slot_index(Tagged<WeakArrayList> array);
  static inline void set_empty_slot_index(Tagged<WeakArrayList> array,
                                          int index);

  static void ScanForEmptySlots(Tagged<WeakArrayList> array);

  DISALLOW_IMPLICIT_CONSTRUCTORS(PrototypeUsers);
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_PROTOTYPE_INFO_H_
