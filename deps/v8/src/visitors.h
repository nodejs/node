// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_VISITORS_H_
#define V8_VISITORS_H_

#include "src/globals.h"

namespace v8 {
namespace internal {

class CodeDataContainer;
class MaybeObject;
class Object;

#define ROOT_ID_LIST(V)                                \
  V(kStringTable, "(Internalized strings)")            \
  V(kExternalStringsTable, "(External strings)")       \
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
class RootVisitor BASE_EMBEDDED {
 public:
  virtual ~RootVisitor() {}

  // Visits a contiguous arrays of pointers in the half-open range
  // [start, end). Any or all of the values may be modified on return.
  virtual void VisitRootPointers(Root root, const char* description,
                                 Object** start, Object** end) = 0;

  // Handy shorthand for visiting a single pointer.
  virtual void VisitRootPointer(Root root, const char* description,
                                Object** p) {
    VisitRootPointers(root, description, p, p + 1);
  }

  // Intended for serialization/deserialization checking: insert, or
  // check for the presence of, a tag at this position in the stream.
  // Also used for marking up GC roots in heap snapshots.
  // TODO(ulan): Remove this.
  virtual void Synchronize(VisitorSynchronization::SyncTag tag) {}

  static const char* RootName(Root root);
};

// Abstract base class for visiting, and optionally modifying, the
// pointers contained in Objects. Used in GC and serialization/deserialization.
class ObjectVisitor BASE_EMBEDDED {
 public:
  virtual ~ObjectVisitor() {}

  // Visits a contiguous arrays of pointers in the half-open range
  // [start, end). Any or all of the values may be modified on return.
  virtual void VisitPointers(HeapObject* host, Object** start,
                             Object** end) = 0;
  virtual void VisitPointers(HeapObject* host, MaybeObject** start,
                             MaybeObject** end) = 0;

  // Handy shorthand for visiting a single pointer.
  virtual void VisitPointer(HeapObject* host, Object** p) {
    VisitPointers(host, p, p + 1);
  }
  virtual void VisitPointer(HeapObject* host, MaybeObject** p) {
    VisitPointers(host, p, p + 1);
  }

  // To allow lazy clearing of inline caches the visitor has
  // a rich interface for iterating over Code objects ...

  // Visits a code target in the instruction stream.
  virtual void VisitCodeTarget(Code* host, RelocInfo* rinfo);

  // Visits a runtime entry in the instruction stream.
  virtual void VisitRuntimeEntry(Code* host, RelocInfo* rinfo) {}

  // Visit pointer embedded into a code object.
  virtual void VisitEmbeddedPointer(Code* host, RelocInfo* rinfo);

  // Visits an external reference embedded into a code object.
  virtual void VisitExternalReference(Code* host, RelocInfo* rinfo) {}

  // Visits an external reference.
  virtual void VisitExternalReference(Foreign* host, Address* p) {}

  // Visits an (encoded) internal reference.
  virtual void VisitInternalReference(Code* host, RelocInfo* rinfo) {}

  // Visits an off-heap target in the instruction stream.
  virtual void VisitOffHeapTarget(Code* host, RelocInfo* rinfo) {}
};

}  // namespace internal
}  // namespace v8

#endif  // V8_VISITORS_H_
