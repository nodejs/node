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

class JSWeakCell;
class NativeContext;

// WeakFactory object from the JS Weak Refs spec proposal:
// https://github.com/tc39/proposal-weakrefs
class JSWeakFactory : public JSObject {
 public:
  DECL_PRINTER(JSWeakFactory)
  DECL_VERIFIER(JSWeakFactory)
  DECL_CAST2(JSWeakFactory)

  DECL_ACCESSORS2(native_context, NativeContext)
  DECL_ACCESSORS(cleanup, Object)
  DECL_ACCESSORS(active_cells, Object)
  DECL_ACCESSORS(cleared_cells, Object)

  // For storing a list of JSWeakFactory objects in NativeContext.
  DECL_ACCESSORS(next, Object)

  DECL_INT_ACCESSORS(flags)

  // Adds a newly constructed JSWeakCell object into this JSWeakFactory.
  inline void AddWeakCell(JSWeakCell weak_cell);

  // Returns true if the cleared_cells list is non-empty.
  inline bool NeedsCleanup() const;

  inline bool scheduled_for_cleanup() const;
  inline void set_scheduled_for_cleanup(bool scheduled_for_cleanup);

  // Get and remove the first cleared JSWeakCell from the cleared_cells
  // list. (Assumes there is one.)
  inline JSWeakCell PopClearedCell(Isolate* isolate);

  // Constructs an iterator for the WeakCells in the cleared_cells list and
  // calls the user's cleanup function.
  static void Cleanup(Handle<JSWeakFactory> weak_factory, Isolate* isolate);

// Layout description.
#define JS_WEAK_FACTORY_FIELDS(V)      \
  V(kNativeContextOffset, kTaggedSize) \
  V(kCleanupOffset, kTaggedSize)       \
  V(kActiveCellsOffset, kTaggedSize)   \
  V(kClearedCellsOffset, kTaggedSize)  \
  V(kNextOffset, kTaggedSize)          \
  V(kFlagsOffset, kTaggedSize)         \
  /* Header size. */                   \
  V(kSize, 0)

  DEFINE_FIELD_OFFSET_CONSTANTS(JSObject::kHeaderSize, JS_WEAK_FACTORY_FIELDS)
#undef JS_WEAK_FACTORY_FIELDS

  // Bitfields in flags.
  class ScheduledForCleanupField : public BitField<bool, 0, 1> {};

  OBJECT_CONSTRUCTORS(JSWeakFactory, JSObject);
};

// WeakCell object from the JS Weak Refs spec proposal.
class JSWeakCell : public JSObject {
 public:
  DECL_PRINTER(JSWeakCell)
  DECL_VERIFIER(JSWeakCell)
  DECL_CAST2(JSWeakCell)

  DECL_ACCESSORS(factory, Object)
  DECL_ACCESSORS(target, Object)
  DECL_ACCESSORS(holdings, Object)

  // For storing doubly linked lists of JSWeakCells in JSWeakFactory.
  DECL_ACCESSORS(prev, Object)
  DECL_ACCESSORS(next, Object)

// Layout description.
#define JS_WEAK_CELL_FIELDS(V)    \
  V(kFactoryOffset, kTaggedSize)  \
  V(kTargetOffset, kTaggedSize)   \
  V(kHoldingsOffset, kTaggedSize) \
  V(kPrevOffset, kTaggedSize)     \
  V(kNextOffset, kTaggedSize)     \
  /* Header size. */              \
  V(kSize, 0)

  DEFINE_FIELD_OFFSET_CONSTANTS(JSObject::kHeaderSize, JS_WEAK_CELL_FIELDS)
#undef JS_WEAK_CELL_FIELDS

  class BodyDescriptor;

  // Nullify is called during GC and it modifies the pointers in JSWeakCell and
  // JSWeakFactory. Thus we need to tell the GC about the modified slots via the
  // gc_notify_updated_slot function. The normal write barrier is not enough,
  // since it's disabled before GC.
  inline void Nullify(
      Isolate* isolate,
      std::function<void(HeapObject object, ObjectSlot slot, Object target)>
          gc_notify_updated_slot);

  inline void Clear(Isolate* isolate);

  OBJECT_CONSTRUCTORS(JSWeakCell, JSObject);
};

class JSWeakRef : public JSObject {
 public:
  DECL_PRINTER(JSWeakRef)
  DECL_VERIFIER(JSWeakRef)
  DECL_CAST2(JSWeakRef)

  DECL_ACCESSORS(target, Object)

  static const int kTargetOffset = JSObject::kHeaderSize;
  static const int kSize = kTargetOffset + kPointerSize;

  class BodyDescriptor;

  OBJECT_CONSTRUCTORS(JSWeakRef, JSObject);
};

class WeakFactoryCleanupJobTask : public Microtask {
 public:
  DECL_ACCESSORS2(factory, JSWeakFactory)

  DECL_CAST2(WeakFactoryCleanupJobTask)
  DECL_VERIFIER(WeakFactoryCleanupJobTask)
  DECL_PRINTER(WeakFactoryCleanupJobTask)

// Layout description.
#define WEAK_FACTORY_CLEANUP_JOB_TASK_FIELDS(V) \
  V(kFactoryOffset, kTaggedSize)                \
  /* Total size. */                             \
  V(kSize, 0)

  DEFINE_FIELD_OFFSET_CONSTANTS(Microtask::kHeaderSize,
                                WEAK_FACTORY_CLEANUP_JOB_TASK_FIELDS)
#undef WEAK_FACTORY_CLEANUP_JOB_TASK_FIELDS

  OBJECT_CONSTRUCTORS(WeakFactoryCleanupJobTask, Microtask)
};

class JSWeakFactoryCleanupIterator : public JSObject {
 public:
  DECL_PRINTER(JSWeakFactoryCleanupIterator)
  DECL_VERIFIER(JSWeakFactoryCleanupIterator)
  DECL_CAST2(JSWeakFactoryCleanupIterator)

  DECL_ACCESSORS2(factory, JSWeakFactory)

// Layout description.
#define JS_WEAK_FACTORY_CLEANUP_ITERATOR_FIELDS(V) \
  V(kFactoryOffset, kTaggedSize)                   \
  /* Header size. */                               \
  V(kSize, 0)

  DEFINE_FIELD_OFFSET_CONSTANTS(JSObject::kHeaderSize,
                                JS_WEAK_FACTORY_CLEANUP_ITERATOR_FIELDS)
#undef JS_WEAK_FACTORY_CLEANUP_ITERATOR_FIELDS

  OBJECT_CONSTRUCTORS(JSWeakFactoryCleanupIterator, JSObject);
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_JS_WEAK_REFS_H_
