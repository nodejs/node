// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_VISITORS_H_
#define V8_VISITORS_H_

#include "src/globals.h"
#include "src/objects/code.h"
#include "src/objects/compressed-slots.h"
#include "src/objects/foreign.h"
#include "src/objects/slots.h"

namespace v8 {
namespace internal {

class CodeDataContainer;
class MaybeObject;
class Object;

#define ROOT_ID_LIST(V)                                \
  V(kStringTable, "(Internalized strings)")            \
  V(kExternalStringsTable, "(External strings)")       \
  V(kReadOnlyRootList, "(Read-only roots)")            \
  V(kStrongRootList, "(Strong roots)")                 \
  V(kSmiRootList, "(Smi roots)")                       \
  V(kBootstrapper, "(Bootstrapper)")                   \
  V(kTop, "(Isolate)")                                 \
  V(kRelocatable, "(Relocatable)")                     \
  V(kDebug, "(Debugger)")                              \
  V(kCompilationCache, "(Compilation cache)")          \
  V(kHandleScope, "(Handle scope)")                    \
  V(kDispatchTable, "(Dispatch table)")                \
  V(kBuiltins, "(Builtins)")                           \
  V(kGlobalHandles, "(Global handles)")                \
  V(kEternalHandles, "(Eternal handles)")              \
  V(kThreadManager, "(Thread manager)")                \
  V(kStrongRoots, "(Strong roots)")                    \
  V(kExtensions, "(Extensions)")                       \
  V(kCodeFlusher, "(Code flusher)")                    \
  V(kPartialSnapshotCache, "(Partial snapshot cache)") \
  V(kReadOnlyObjectCache, "(Read-only object cache)")  \
  V(kWeakCollections, "(Weak collections)")            \
  V(kWrapperTracing, "(Wrapper tracing)")              \
  V(kUnknown, "(Unknown)")

class VisitorSynchronization : public AllStatic {
 public:
#define DECLARE_ENUM(enum_item, ignore) enum_item,
  enum SyncTag { ROOT_ID_LIST(DECLARE_ENUM) kNumberOfSyncTags };
#undef DECLARE_ENUM
};

enum class Root {
#define DECLARE_ENUM(enum_item, ignore) enum_item,
  ROOT_ID_LIST(DECLARE_ENUM)
#undef DECLARE_ENUM
      kNumberOfRoots
};

// Abstract base class for visiting, and optionally modifying, the
// pointers contained in roots. Used in GC and serialization/deserialization.
class RootVisitor {
 public:
  virtual ~RootVisitor() = default;

  // Visits a contiguous arrays of pointers in the half-open range
  // [start, end). Any or all of the values may be modified on return.
  virtual void VisitRootPointers(Root root, const char* description,
                                 FullObjectSlot start, FullObjectSlot end) = 0;

  // Handy shorthand for visiting a single pointer.
  virtual void VisitRootPointer(Root root, const char* description,
                                FullObjectSlot p) {
    VisitRootPointers(root, description, p, p + 1);
  }

  // Intended for serialization/deserialization checking: insert, or
  // check for the presence of, a tag at this position in the stream.
  // Also used for marking up GC roots in heap snapshots.
  // TODO(ulan): Remove this.
  virtual void Synchronize(VisitorSynchronization::SyncTag tag) {}

  static const char* RootName(Root root);
};

class RelocIterator;

// Abstract base class for visiting, and optionally modifying, the
// pointers contained in Objects. Used in GC and serialization/deserialization.
class ObjectVisitor {
 public:
  virtual ~ObjectVisitor() = default;

  // Visits a contiguous arrays of pointers in the half-open range
  // [start, end). Any or all of the values may be modified on return.
  virtual void VisitPointers(HeapObject host, ObjectSlot start,
                             ObjectSlot end) = 0;
  virtual void VisitPointers(HeapObject host, MaybeObjectSlot start,
                             MaybeObjectSlot end) = 0;

  // Custom weak pointers must be ignored by the GC but not other
  // visitors. They're used for e.g., lists that are recreated after GC. The
  // default implementation treats them as strong pointers. Visitors who want to
  // ignore them must override this function with empty.
  virtual void VisitCustomWeakPointers(HeapObject host, ObjectSlot start,
                                       ObjectSlot end) {
    VisitPointers(host, start, end);
  }

  // Handy shorthand for visiting a single pointer.
  virtual void VisitPointer(HeapObject host, ObjectSlot p) {
    VisitPointers(host, p, p + 1);
  }
  virtual void VisitPointer(HeapObject host, MaybeObjectSlot p) {
    VisitPointers(host, p, p + 1);
  }
  virtual void VisitCustomWeakPointer(HeapObject host, ObjectSlot p) {
    VisitCustomWeakPointers(host, p, p + 1);
  }

  virtual void VisitEphemeron(HeapObject host, int index, ObjectSlot key,
                              ObjectSlot value) {
    VisitPointer(host, key);
    VisitPointer(host, value);
  }

  // To allow lazy clearing of inline caches the visitor has
  // a rich interface for iterating over Code objects ...

  // Visits a code target in the instruction stream.
  virtual void VisitCodeTarget(Code host, RelocInfo* rinfo) = 0;

  // Visit pointer embedded into a code object.
  virtual void VisitEmbeddedPointer(Code host, RelocInfo* rinfo) = 0;

  // Visits a runtime entry in the instruction stream.
  virtual void VisitRuntimeEntry(Code host, RelocInfo* rinfo) {}

  // Visits an external reference embedded into a code object.
  virtual void VisitExternalReference(Code host, RelocInfo* rinfo) {}

  // Visits an external reference.
  virtual void VisitExternalReference(Foreign host, Address* p) {}

  // Visits an (encoded) internal reference.
  virtual void VisitInternalReference(Code host, RelocInfo* rinfo) {}

  // Visits an off-heap target in the instruction stream.
  virtual void VisitOffHeapTarget(Code host, RelocInfo* rinfo) {}

  // Visits the relocation info using the given iterator.
  virtual void VisitRelocInfo(RelocIterator* it);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_VISITORS_H_
