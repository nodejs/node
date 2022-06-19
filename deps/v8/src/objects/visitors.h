// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_VISITORS_H_
#define V8_OBJECTS_VISITORS_H_

#include "src/common/globals.h"
#include "src/objects/code.h"
#include "src/objects/compressed-slots.h"
#include "src/objects/foreign.h"
#include "src/objects/slots.h"

namespace v8 {
namespace internal {

class CodeDataContainer;

#define ROOT_ID_LIST(V)                                 \
  V(kStringTable, "(Internalized strings)")             \
  V(kExternalStringsTable, "(External strings)")        \
  V(kReadOnlyRootList, "(Read-only roots)")             \
  V(kStrongRootList, "(Strong roots)")                  \
  V(kSmiRootList, "(Smi roots)")                        \
  V(kBootstrapper, "(Bootstrapper)")                    \
  V(kStackRoots, "(Stack roots)")                       \
  V(kRelocatable, "(Relocatable)")                      \
  V(kDebug, "(Debugger)")                               \
  V(kCompilationCache, "(Compilation cache)")           \
  V(kHandleScope, "(Handle scope)")                     \
  V(kBuiltins, "(Builtins)")                            \
  V(kGlobalHandles, "(Global handles)")                 \
  V(kEternalHandles, "(Eternal handles)")               \
  V(kThreadManager, "(Thread manager)")                 \
  V(kStrongRoots, "(Strong roots)")                     \
  V(kExtensions, "(Extensions)")                        \
  V(kCodeFlusher, "(Code flusher)")                     \
  V(kStartupObjectCache, "(Startup object cache)")      \
  V(kReadOnlyObjectCache, "(Read-only object cache)")   \
  V(kSharedHeapObjectCache, "(Shareable object cache)") \
  V(kWeakCollections, "(Weak collections)")             \
  V(kWrapperTracing, "(Wrapper tracing)")               \
  V(kWriteBarrier, "(Write barrier)")                   \
  V(kRetainMaps, "(Retain maps)")                       \
  V(kClientHeap, "(Client heap)")                       \
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

  // Visits a contiguous arrays of off-heap pointers in the half-open range
  // [start, end). Any or all of the values may be modified on return.
  virtual void VisitRootPointers(Root root, const char* description,
                                 OffHeapObjectSlot start,
                                 OffHeapObjectSlot end) {
    // This should be implemented for any visitor that visits the string table.
    // If we ever add new off-heap data-structures that we want to walk as roots
    // using this function, we should make it generic, by
    //
    //   1) Making this function pure virtual, and
    //   2) Implementing it for all visitors.
    UNREACHABLE();
  }

  // Visits a single pointer which is Code from the execution stack.
  virtual void VisitRunningCode(FullObjectSlot p) {
    // For most visitors, currently running Code is no different than any other
    // on-stack pointer.
    VisitRootPointer(Root::kStackRoots, nullptr, p);
  }

  // Intended for serialization/deserialization checking: insert, or
  // check for the presence of, a tag at this position in the stream.
  // Also used for marking up GC roots in heap snapshots.
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
  // When V8_EXTERNAL_CODE_SPACE is enabled, visits a Code pointer slot.
  // The values may be modified on return.
  // Not used when V8_EXTERNAL_CODE_SPACE is not enabled (the Code pointer
  // slots are visited as a part of on-heap slot visitation - via
  // VisitPointers()).
  virtual void VisitCodePointer(HeapObject host, CodeObjectSlot slot) = 0;

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
  void VisitRelocInfo(RelocIterator* it);

  // Visits the object's map pointer, decoding as necessary
  virtual void VisitMapPointer(HeapObject host) { UNREACHABLE(); }

  // Visits an external pointer. This is currently only guaranteed to be called
  // when the sandbox is enabled.
  virtual void VisitExternalPointer(HeapObject host, ExternalPointer_t ptr) {}
};

// Helper version of ObjectVisitor that also takes care of caching base values
// of the main pointer compression cage and for the code cage.
class ObjectVisitorWithCageBases : public ObjectVisitor {
 public:
  inline ObjectVisitorWithCageBases(PtrComprCageBase cage_base,
                                    PtrComprCageBase code_cage_base);
  inline explicit ObjectVisitorWithCageBases(Isolate* isolate);
  inline explicit ObjectVisitorWithCageBases(Heap* heap);

  // The pointer compression cage base value used for decompression of all
  // tagged values except references to Code objects.
  PtrComprCageBase cage_base() const {
#if V8_COMPRESS_POINTERS
    return cage_base_;
#else
    return PtrComprCageBase{};
#endif  // V8_COMPRESS_POINTERS
  }

  // The pointer compression cage base value used for decompression of
  // references to Code objects.
  PtrComprCageBase code_cage_base() const {
#ifdef V8_EXTERNAL_CODE_SPACE
    return code_cage_base_;
#else
    return cage_base();
#endif  // V8_EXTERNAL_CODE_SPACE
  }

 private:
#if V8_COMPRESS_POINTERS
  const PtrComprCageBase cage_base_;
#ifdef V8_EXTERNAL_CODE_SPACE
  const PtrComprCageBase code_cage_base_;
#endif  // V8_EXTERNAL_CODE_SPACE
#endif  // V8_COMPRESS_POINTERS
};

}  // namespace internal
}  // namespace v8

#endif  // V8_OBJECTS_VISITORS_H_
