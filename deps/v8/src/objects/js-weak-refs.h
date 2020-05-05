// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_JS_WEAK_REFS_H_
#define V8_OBJECTS_JS_WEAK_REFS_H_

#include "src/objects/js-objects.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

class NativeContext;
class WeakCell;

// FinalizationRegistry object from the JS Weak Refs spec proposal:
// https://github.com/tc39/proposal-weakrefs
class JSFinalizationRegistry : public JSObject {
 public:
  DECL_PRINTER(JSFinalizationRegistry)
  EXPORT_DECL_VERIFIER(JSFinalizationRegistry)
  DECL_CAST(JSFinalizationRegistry)

  DECL_ACCESSORS(native_context, NativeContext)
  DECL_ACCESSORS(cleanup, Object)

  DECL_ACCESSORS(active_cells, HeapObject)
  DECL_ACCESSORS(cleared_cells, HeapObject)
  DECL_ACCESSORS(key_map, Object)

  DECL_ACCESSORS(next_dirty, Object)

  DECL_INT_ACCESSORS(flags)

  class BodyDescriptor;

  inline static void Register(
      Handle<JSFinalizationRegistry> finalization_registry,
      Handle<JSReceiver> target, Handle<Object> holdings, Handle<Object> key,
      Isolate* isolate);
  inline static bool Unregister(
      Handle<JSFinalizationRegistry> finalization_registry,
      Handle<JSReceiver> unregister_token, Isolate* isolate);

  // RemoveUnregisterToken is called from both Unregister and during GC. Since
  // it modifies slots in key_map and WeakCells and the normal write barrier is
  // disabled during GC, we need to tell the GC about the modified slots via the
  // gc_notify_updated_slot function.
  template <typename MatchCallback, typename GCNotifyUpdatedSlotCallback>
  inline bool RemoveUnregisterToken(
      JSReceiver unregister_token, Isolate* isolate,
      MatchCallback match_callback,
      GCNotifyUpdatedSlotCallback gc_notify_updated_slot);

  // Returns true if the cleared_cells list is non-empty.
  inline bool NeedsCleanup() const;

  inline bool scheduled_for_cleanup() const;
  inline void set_scheduled_for_cleanup(bool scheduled_for_cleanup);

  // Remove the first cleared WeakCell from the cleared_cells
  // list (assumes there is one) and return its holdings.
  inline static Object PopClearedCellHoldings(
      Handle<JSFinalizationRegistry> finalization_registry, Isolate* isolate);

  // Constructs an iterator for the WeakCells in the cleared_cells list and
  // calls the user's cleanup function.
  //
  // Returns Nothing<bool> if exception occurs, otherwise returns Just(true).
  static V8_WARN_UNUSED_RESULT Maybe<bool> Cleanup(
      Isolate* isolate, Handle<JSFinalizationRegistry> finalization_registry,
      Handle<Object> callback);

  // Layout description.
  DEFINE_FIELD_OFFSET_CONSTANTS(
      JSObject::kHeaderSize, TORQUE_GENERATED_JS_FINALIZATION_REGISTRY_FIELDS)

  // Bitfields in flags.
  using ScheduledForCleanupField = base::BitField<bool, 0, 1>;

  OBJECT_CONSTRUCTORS(JSFinalizationRegistry, JSObject);
};

// Internal object for storing weak references in JSFinalizationRegistry.
class WeakCell : public TorqueGeneratedWeakCell<WeakCell, HeapObject> {
 public:
  DECL_PRINTER(WeakCell)
  EXPORT_DECL_VERIFIER(WeakCell)

  class BodyDescriptor;

  // Nullify is called during GC and it modifies the pointers in WeakCell and
  // JSFinalizationRegistry. Thus we need to tell the GC about the modified
  // slots via the gc_notify_updated_slot function. The normal write barrier is
  // not enough, since it's disabled before GC.
  template <typename GCNotifyUpdatedSlotCallback>
  inline void Nullify(Isolate* isolate,
                      GCNotifyUpdatedSlotCallback gc_notify_updated_slot);

  inline void RemoveFromFinalizationRegistryCells(Isolate* isolate);

  TQ_OBJECT_CONSTRUCTORS(WeakCell)
};

class JSWeakRef : public TorqueGeneratedJSWeakRef<JSWeakRef, JSObject> {
 public:
  DECL_PRINTER(JSWeakRef)
  EXPORT_DECL_VERIFIER(JSWeakRef)

  class BodyDescriptor;

  TQ_OBJECT_CONSTRUCTORS(JSWeakRef)
};

class JSFinalizationRegistryCleanupIterator
    : public TorqueGeneratedJSFinalizationRegistryCleanupIterator<
          JSFinalizationRegistryCleanupIterator, JSObject> {
 public:
  DECL_PRINTER(JSFinalizationRegistryCleanupIterator)
  DECL_VERIFIER(JSFinalizationRegistryCleanupIterator)

  TQ_OBJECT_CONSTRUCTORS(JSFinalizationRegistryCleanupIterator)
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_JS_WEAK_REFS_H_
