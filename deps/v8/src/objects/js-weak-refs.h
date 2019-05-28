// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_JS_WEAK_REFS_H_
#define V8_OBJECTS_JS_WEAK_REFS_H_

#include "src/objects/js-objects.h"
#include "src/objects/microtask.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

class NativeContext;
class WeakCell;

// FinalizationGroup object from the JS Weak Refs spec proposal:
// https://github.com/tc39/proposal-weakrefs
class JSFinalizationGroup : public JSObject {
 public:
  DECL_PRINTER(JSFinalizationGroup)
  EXPORT_DECL_VERIFIER(JSFinalizationGroup)
  DECL_CAST(JSFinalizationGroup)

  DECL_ACCESSORS(native_context, NativeContext)
  DECL_ACCESSORS(cleanup, Object)

  DECL_ACCESSORS(active_cells, Object)
  DECL_ACCESSORS(cleared_cells, Object)
  DECL_ACCESSORS(key_map, Object)

  // For storing a list of JSFinalizationGroup objects in NativeContext.
  DECL_ACCESSORS(next, Object)

  DECL_INT_ACCESSORS(flags)

  inline static void Register(Handle<JSFinalizationGroup> finalization_group,
                              Handle<JSReceiver> target,
                              Handle<Object> holdings, Handle<Object> key,
                              Isolate* isolate);
  inline static void Unregister(Handle<JSFinalizationGroup> finalization_group,
                                Handle<Object> key, Isolate* isolate);

  // Returns true if the cleared_cells list is non-empty.
  inline bool NeedsCleanup() const;

  inline bool scheduled_for_cleanup() const;
  inline void set_scheduled_for_cleanup(bool scheduled_for_cleanup);

  // Remove the first cleared WeakCell from the cleared_cells
  // list (assumes there is one) and return its holdings.
  inline static Object PopClearedCellHoldings(
      Handle<JSFinalizationGroup> finalization_group, Isolate* isolate);

  // Constructs an iterator for the WeakCells in the cleared_cells list and
  // calls the user's cleanup function.
  static void Cleanup(Handle<JSFinalizationGroup> finalization_group,
                      Isolate* isolate);

// Layout description.
#define JS_FINALIZATION_GROUP_FIELDS(V) \
  V(kNativeContextOffset, kTaggedSize)  \
  V(kCleanupOffset, kTaggedSize)        \
  V(kActiveCellsOffset, kTaggedSize)    \
  V(kClearedCellsOffset, kTaggedSize)   \
  V(kKeyMapOffset, kTaggedSize)         \
  V(kNextOffset, kTaggedSize)           \
  V(kFlagsOffset, kTaggedSize)          \
  /* Header size. */                    \
  V(kSize, 0)

  DEFINE_FIELD_OFFSET_CONSTANTS(JSObject::kHeaderSize,
                                JS_FINALIZATION_GROUP_FIELDS)
#undef JS_FINALIZATION_GROUP_FIELDS

  // Bitfields in flags.
  class ScheduledForCleanupField : public BitField<bool, 0, 1> {};

  OBJECT_CONSTRUCTORS(JSFinalizationGroup, JSObject);
};

// Internal object for storing weak references in JSFinalizationGroup.
class WeakCell : public HeapObject {
 public:
  DECL_PRINTER(WeakCell)
  EXPORT_DECL_VERIFIER(WeakCell)
  DECL_CAST(WeakCell)

  DECL_ACCESSORS(finalization_group, Object)
  DECL_ACCESSORS(target, HeapObject)
  DECL_ACCESSORS(holdings, Object)

  // For storing doubly linked lists of WeakCells in JSFinalizationGroup's
  // "active_cells" and "cleared_cells" lists.
  DECL_ACCESSORS(prev, Object)
  DECL_ACCESSORS(next, Object)

  // For storing doubly linked lists of WeakCells per key in
  // JSFinalizationGroup's key-based hashmap. WeakCell also needs to know its
  // key, so that we can remove the key from the key_map when we remove the last
  // WeakCell associated with it.
  DECL_ACCESSORS(key, Object)
  DECL_ACCESSORS(key_list_prev, Object)
  DECL_ACCESSORS(key_list_next, Object)

// Layout description.
#define WEAK_CELL_FIELDS(V)                \
  V(kFinalizationGroupOffset, kTaggedSize) \
  V(kTargetOffset, kTaggedSize)            \
  V(kHoldingsOffset, kTaggedSize)          \
  V(kPrevOffset, kTaggedSize)              \
  V(kNextOffset, kTaggedSize)              \
  V(kKeyOffset, kTaggedSize)               \
  V(kKeyListPrevOffset, kTaggedSize)       \
  V(kKeyListNextOffset, kTaggedSize)       \
  /* Header size. */                       \
  V(kSize, 0)

  DEFINE_FIELD_OFFSET_CONSTANTS(HeapObject::kHeaderSize, WEAK_CELL_FIELDS)
#undef WEAK_CELL_FIELDS

  class BodyDescriptor;

  // Nullify is called during GC and it modifies the pointers in WeakCell and
  // JSFinalizationGroup. Thus we need to tell the GC about the modified slots
  // via the gc_notify_updated_slot function. The normal write barrier is not
  // enough, since it's disabled before GC.
  inline void Nullify(
      Isolate* isolate,
      std::function<void(HeapObject object, ObjectSlot slot, Object target)>
          gc_notify_updated_slot);

  inline void RemoveFromFinalizationGroupCells(Isolate* isolate);

  OBJECT_CONSTRUCTORS(WeakCell, HeapObject);
};

class JSWeakRef : public JSObject {
 public:
  DECL_PRINTER(JSWeakRef)
  EXPORT_DECL_VERIFIER(JSWeakRef)
  DECL_CAST(JSWeakRef)

  DECL_ACCESSORS(target, HeapObject)

// Layout description.
#define JS_WEAK_REF_FIELDS(V)   \
  V(kTargetOffset, kTaggedSize) \
  /* Header size. */            \
  V(kSize, 0)

  DEFINE_FIELD_OFFSET_CONSTANTS(JSObject::kHeaderSize, JS_WEAK_REF_FIELDS)
#undef JS_WEAK_REF_FIELDS

  class BodyDescriptor;

  OBJECT_CONSTRUCTORS(JSWeakRef, JSObject);
};

class FinalizationGroupCleanupJobTask : public Microtask {
 public:
  DECL_ACCESSORS(finalization_group, JSFinalizationGroup)

  DECL_CAST(FinalizationGroupCleanupJobTask)
  DECL_VERIFIER(FinalizationGroupCleanupJobTask)
  DECL_PRINTER(FinalizationGroupCleanupJobTask)

// Layout description.
#define FINALIZATION_GROUP_CLEANUP_JOB_TASK_FIELDS(V) \
  V(kFinalizationGroupOffset, kTaggedSize)            \
  /* Total size. */                                   \
  V(kSize, 0)

  DEFINE_FIELD_OFFSET_CONSTANTS(Microtask::kHeaderSize,
                                FINALIZATION_GROUP_CLEANUP_JOB_TASK_FIELDS)
#undef FINALIZATION_GROUP_CLEANUP_JOB_TASK_FIELDS

  OBJECT_CONSTRUCTORS(FinalizationGroupCleanupJobTask, Microtask);
};

class JSFinalizationGroupCleanupIterator : public JSObject {
 public:
  DECL_PRINTER(JSFinalizationGroupCleanupIterator)
  DECL_VERIFIER(JSFinalizationGroupCleanupIterator)
  DECL_CAST(JSFinalizationGroupCleanupIterator)

  DECL_ACCESSORS(finalization_group, JSFinalizationGroup)

// Layout description.
#define JS_FINALIZATION_GROUP_CLEANUP_ITERATOR_FIELDS(V) \
  V(kFinalizationGroupOffset, kTaggedSize)               \
  /* Header size. */                                     \
  V(kSize, 0)

  DEFINE_FIELD_OFFSET_CONSTANTS(JSObject::kHeaderSize,
                                JS_FINALIZATION_GROUP_CLEANUP_ITERATOR_FIELDS)
#undef JS_FINALIZATION_GROUP_CLEANUP_ITERATOR_FIELDS

  OBJECT_CONSTRUCTORS(JSFinalizationGroupCleanupIterator, JSObject);
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_JS_WEAK_REFS_H_
