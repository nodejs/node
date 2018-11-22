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

// WeakFactory object from the JS Weak Refs spec proposal:
// https://github.com/tc39/proposal-weakrefs
class JSWeakFactory : public JSObject {
 public:
  DECL_PRINTER(JSWeakFactory)
  DECL_VERIFIER(JSWeakFactory)
  DECL_CAST(JSWeakFactory)

  DECL_ACCESSORS(native_context, Context)
  DECL_ACCESSORS(cleanup, Object)
  DECL_ACCESSORS(active_cells, Object)
  DECL_ACCESSORS(cleared_cells, Object)

  // For storing a list of JSWeakFactory objects in NativeContext.
  DECL_ACCESSORS(next, Object)

  DECL_INT_ACCESSORS(flags)

  // Adds a newly constructed JSWeakCell object into this JSWeakFactory.
  inline void AddWeakCell(JSWeakCell* weak_cell);

  // Returns true if the cleared_cells list is non-empty.
  inline bool NeedsCleanup() const;

  inline bool scheduled_for_cleanup() const;
  inline void set_scheduled_for_cleanup(bool scheduled_for_cleanup);

  // Get and remove the first cleared JSWeakCell from the cleared_cells
  // list. (Assumes there is one.)
  inline JSWeakCell* PopClearedCell(Isolate* isolate);

  // Constructs an iterator for the WeakCells in the cleared_cells list and
  // calls the user's cleanup function.
  static void Cleanup(Handle<JSWeakFactory> weak_factory, Isolate* isolate);

  static const int kNativeContextOffset = JSObject::kHeaderSize;
  static const int kCleanupOffset = kNativeContextOffset + kPointerSize;
  static const int kActiveCellsOffset = kCleanupOffset + kPointerSize;
  static const int kClearedCellsOffset = kActiveCellsOffset + kPointerSize;
  static const int kNextOffset = kClearedCellsOffset + kPointerSize;
  static const int kFlagsOffset = kNextOffset + kPointerSize;
  static const int kSize = kFlagsOffset + kPointerSize;

  // Bitfields in flags.
  class ScheduledForCleanupField : public BitField<bool, 0, 1> {};

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(JSWeakFactory);
};

// WeakCell object from the JS Weak Refs spec proposal.
class JSWeakCell : public JSObject {
 public:
  DECL_PRINTER(JSWeakCell)
  DECL_VERIFIER(JSWeakCell)
  DECL_CAST(JSWeakCell)

  DECL_ACCESSORS(factory, Object)
  DECL_ACCESSORS(target, Object)
  DECL_ACCESSORS(holdings, Object)

  // For storing doubly linked lists of JSWeakCells in JSWeakFactory.
  DECL_ACCESSORS(prev, Object)
  DECL_ACCESSORS(next, Object)

  static const int kFactoryOffset = JSObject::kHeaderSize;
  static const int kTargetOffset = kFactoryOffset + kPointerSize;
  static const int kHoldingsOffset = kTargetOffset + kPointerSize;
  static const int kPrevOffset = kHoldingsOffset + kPointerSize;
  static const int kNextOffset = kPrevOffset + kPointerSize;
  static const int kSize = kNextOffset + kPointerSize;

  class BodyDescriptor;

  // Nullify is called during GC and it modifies the pointers in JSWeakCell and
  // JSWeakFactory. Thus we need to tell the GC about the modified slots via the
  // gc_notify_updated_slot function. The normal write barrier is not enough,
  // since it's disabled before GC.
  inline void Nullify(
      Isolate* isolate,
      std::function<void(HeapObject* object, ObjectSlot slot, Object* target)>
          gc_notify_updated_slot);

  inline void Clear(Isolate* isolate);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(JSWeakCell);
};

class JSWeakRef : public JSWeakCell {
 public:
  DECL_CAST(JSWeakRef)
 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(JSWeakRef);
};

class WeakFactoryCleanupJobTask : public Microtask {
 public:
  DECL_ACCESSORS(factory, JSWeakFactory)

  DECL_CAST(WeakFactoryCleanupJobTask)
  DECL_VERIFIER(WeakFactoryCleanupJobTask)
  DECL_PRINTER(WeakFactoryCleanupJobTask)

  static const int kFactoryOffset = Microtask::kHeaderSize;
  static const int kSize = kFactoryOffset + kPointerSize;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(WeakFactoryCleanupJobTask);
};

class JSWeakFactoryCleanupIterator : public JSObject {
 public:
  DECL_PRINTER(JSWeakFactoryCleanupIterator)
  DECL_VERIFIER(JSWeakFactoryCleanupIterator)
  DECL_CAST(JSWeakFactoryCleanupIterator)

  DECL_ACCESSORS(factory, JSWeakFactory)

  static const int kFactoryOffset = JSObject::kHeaderSize;
  static const int kSize = kFactoryOffset + kPointerSize;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(JSWeakFactoryCleanupIterator);
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_JS_WEAK_REFS_H_
