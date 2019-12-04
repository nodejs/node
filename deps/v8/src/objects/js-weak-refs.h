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
  inline static bool Unregister(Handle<JSFinalizationGroup> finalization_group,
                                Handle<JSReceiver> unregister_token,
                                Isolate* isolate);

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
  //
  // Returns Nothing<bool> if exception occurs, otherwise returns Just(true).
  static V8_WARN_UNUSED_RESULT Maybe<bool> Cleanup(
      Isolate* isolate, Handle<JSFinalizationGroup> finalization_group,
      Handle<Object> callback);

  // Layout description.
  DEFINE_FIELD_OFFSET_CONSTANTS(JSObject::kHeaderSize,
                                TORQUE_GENERATED_JSFINALIZATION_GROUP_FIELDS)

  // Bitfields in flags.
  using ScheduledForCleanupField = BitField<bool, 0, 1>;

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
  DEFINE_FIELD_OFFSET_CONSTANTS(HeapObject::kHeaderSize,
                                TORQUE_GENERATED_WEAK_CELL_FIELDS)

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
  DEFINE_FIELD_OFFSET_CONSTANTS(JSObject::kHeaderSize,
                                TORQUE_GENERATED_JSWEAK_REF_FIELDS)

  class BodyDescriptor;

  OBJECT_CONSTRUCTORS(JSWeakRef, JSObject);
};

class JSFinalizationGroupCleanupIterator : public JSObject {
 public:
  DECL_PRINTER(JSFinalizationGroupCleanupIterator)
  DECL_VERIFIER(JSFinalizationGroupCleanupIterator)
  DECL_CAST(JSFinalizationGroupCleanupIterator)

  DECL_ACCESSORS(finalization_group, JSFinalizationGroup)

  // Layout description.
  DEFINE_FIELD_OFFSET_CONSTANTS(
    JSObject::kHeaderSize,
    TORQUE_GENERATED_JSFINALIZATION_GROUP_CLEANUP_ITERATOR_FIELDS)

  OBJECT_CONSTRUCTORS(JSFinalizationGroupCleanupIterator, JSObject);
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_JS_WEAK_REFS_H_
