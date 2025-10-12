// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_JS_WEAK_REFS_H_
#define V8_OBJECTS_JS_WEAK_REFS_H_

#include "src/objects/js-objects.h"
#include "src/objects/tagged-field.h"
#include "src/objects/union.h"
#include "torque-generated/bit-fields.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

class NativeContext;
class WeakCell;

#include "torque-generated/src/objects/js-weak-refs-tq.inc"

// FinalizationRegistry object from the JS Weak Refs spec proposal:
// https://github.com/tc39/proposal-weakrefs
class JSFinalizationRegistry
    : public TorqueGeneratedJSFinalizationRegistry<JSFinalizationRegistry,
                                                   JSObject> {
 public:
  DECL_PRINTER(JSFinalizationRegistry)
  EXPORT_DECL_VERIFIER(JSFinalizationRegistry)

  DECL_BOOLEAN_ACCESSORS(scheduled_for_cleanup)

  class BodyDescriptor;

  inline static void RegisterWeakCellWithUnregisterToken(
      DirectHandle<JSFinalizationRegistry> finalization_registry,
      DirectHandle<WeakCell> weak_cell, Isolate* isolate);
  inline static bool Unregister(
      DirectHandle<JSFinalizationRegistry> finalization_registry,
      DirectHandle<HeapObject> unregister_token, Isolate* isolate);

  // RemoveUnregisterToken is called from both Unregister and during GC. Since
  // it modifies slots in key_map and WeakCells and the normal write barrier is
  // disabled during GC, we need to tell the GC about the modified slots via the
  // gc_notify_updated_slot function.
  enum RemoveUnregisterTokenMode {
    kRemoveMatchedCellsFromRegistry,
    kKeepMatchedCellsInRegistry
  };
  template <typename GCNotifyUpdatedSlotCallback>
  inline bool RemoveUnregisterToken(
      Tagged<HeapObject> unregister_token, Isolate* isolate,
      RemoveUnregisterTokenMode removal_mode,
      GCNotifyUpdatedSlotCallback gc_notify_updated_slot);

  // Returns true if the cleared_cells list is non-empty.
  inline bool NeedsCleanup() const;

  V8_EXPORT_PRIVATE Tagged<WeakCell> PopClearedCell(
      Isolate* isolate, bool* key_map_may_need_shrink);

  static void ShrinkKeyMap(
      Isolate* isolate,
      DirectHandle<JSFinalizationRegistry> finalization_registry);

  // Pop cleared cells and call their finalizers.
  static Maybe<bool> Cleanup(
      Isolate* isolate,
      DirectHandle<JSFinalizationRegistry> finalization_registry);

  // Remove the already-popped weak_cell from its unregister token linked list,
  // as well as removing the entry from the key map if it is the only WeakCell
  // with its unregister token. This method cannot GC and does not shrink the
  // key map. Asserts that weak_cell has a non-undefined unregister token.
  V8_EXPORT_PRIVATE void RemoveCellFromUnregisterTokenMap(
      Isolate* isolate, Tagged<WeakCell> weak_cell);

  // Bitfields in flags.
  DEFINE_TORQUE_GENERATED_FINALIZATION_REGISTRY_FLAGS()

  TQ_OBJECT_CONSTRUCTORS(JSFinalizationRegistry)
};

// Internal object for storing weak references in JSFinalizationRegistry.
V8_OBJECT class WeakCell : public HeapObjectLayout {
 public:
  inline Tagged<UnionOf<JSFinalizationRegistry, Undefined>>
  finalization_registry() const;
  inline void set_finalization_registry(
      Tagged<UnionOf<JSFinalizationRegistry, Undefined>> value,
      WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<UnionOf<Symbol, JSReceiver, Undefined>> target() const;
  inline void set_target(Tagged<UnionOf<Symbol, JSReceiver, Undefined>> value,
                         WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<UnionOf<Symbol, JSReceiver, Undefined>> unregister_token()
      const;
  inline void set_unregister_token(
      Tagged<UnionOf<Symbol, JSReceiver, Undefined>> value,
      WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<JSAny> holdings() const;
  inline void set_holdings(Tagged<JSAny> value,
                           WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<UnionOf<WeakCell, Undefined>> prev() const;
  inline void set_prev(Tagged<UnionOf<WeakCell, Undefined>> value,
                       WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<UnionOf<WeakCell, Undefined>> next() const;
  inline void set_next(Tagged<UnionOf<WeakCell, Undefined>> value,
                       WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<UnionOf<WeakCell, Undefined>> key_list_prev() const;
  inline void set_key_list_prev(Tagged<UnionOf<WeakCell, Undefined>> value,
                                WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<UnionOf<WeakCell, Undefined>> key_list_next() const;
  inline void set_key_list_next(Tagged<UnionOf<WeakCell, Undefined>> value,
                                WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  EXPORT_DECL_VERIFIER(WeakCell)

  DECL_PRINTER(WeakCell)

  class BodyDescriptor;

  // Provide relaxed load access to target field.
  inline Tagged<HeapObject> relaxed_target() const;

  // Provide relaxed load access to the unregister token field.
  inline Tagged<HeapObject> relaxed_unregister_token() const;

  // Nullify is called during GC and it modifies the pointers in WeakCell and
  // JSFinalizationRegistry. Thus we need to tell the GC about the modified
  // slots via the gc_notify_updated_slot function. The normal write barrier is
  // not enough, since it's disabled before GC.
  template <typename GCNotifyUpdatedSlotCallback>
  inline void Nullify(Isolate* isolate,
                      GCNotifyUpdatedSlotCallback gc_notify_updated_slot);

  inline void RemoveFromFinalizationRegistryCells(Isolate* isolate);

 private:
  friend class JSFinalizationRegistry;
  friend class MarkCompactCollector;
  template <typename ConcreteVisitor>
  friend class MarkingVisitorBase;
  friend class TorqueGeneratedWeakCellAsserts;
  friend class V8HeapExplorer;

  TaggedMember<UnionOf<JSFinalizationRegistry, Undefined>>
      finalization_registry_;
  TaggedMember<UnionOf<Symbol, JSReceiver, Undefined>> target_;
  TaggedMember<UnionOf<Symbol, JSReceiver, Undefined>> unregister_token_;
  TaggedMember<JSAny> holdings_;
  TaggedMember<UnionOf<WeakCell, Undefined>> prev_;
  TaggedMember<UnionOf<WeakCell, Undefined>> next_;
  TaggedMember<UnionOf<WeakCell, Undefined>> key_list_prev_;
  TaggedMember<UnionOf<WeakCell, Undefined>> key_list_next_;
} V8_OBJECT_END;

// TODO(42202654): Revise `JSWeakRef` to use `TaggedMember`s once `JSObject`
// inherits from `HeapObjectLayout`.
class JSWeakRef : public JSObject {
 public:
  constexpr JSWeakRef() : JSObject() {}

  static constexpr int kTargetOffset = JSObject::kHeaderSize;
  static constexpr int kTargetOffsetEnd = kTargetOffset + kTaggedSize - 1;
  static constexpr int kHeaderSize = kTargetOffsetEnd + 1;

  DECL_ACCESSORS(target, Tagged<Union<JSReceiver, Symbol, Undefined>>)
  DECL_PRINTER(JSWeakRef)
  EXPORT_DECL_VERIFIER(JSWeakRef)

  class BodyDescriptor;

 protected:
  constexpr V8_INLINE JSWeakRef(Address ptr, SkipTypeCheckTag)
      : JSObject(ptr, SkipTypeCheckTag()) {}

  friend class Tagged<JSWeakRef>;
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_JS_WEAK_REFS_H_
