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

class Code;

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
  V(kTracedHandles, "(Traced handles)")                 \
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

  // Visits a running Code object and potentially its associated
  // InstructionStream from the execution stack.
  virtual void VisitRunningCode(FullObjectSlot code_slot,
                                FullObjectSlot istream_or_smi_zero_slot) {
    // For most visitors, currently running code is no different than any other
    // on-stack pointer.
    VisitRootPointer(Root::kStackRoots, nullptr, istream_or_smi_zero_slot);
    VisitRootPointer(Root::kStackRoots, nullptr, code_slot);
  }

  // Intended for serialization/deserialization checking: insert, or
  // check for the presence of, a tag at this position in the stream.
  // Also used for marking up GC roots in heap snapshots.
  virtual void Synchronize(VisitorSynchronization::SyncTag tag) {}

  static const char* RootName(Root root);

  // The type of collector that invokes this visitor. This is used by the
  // ConservativeStackVisitor to determine which root pointers on the stack
  // to follow, during conservative stack scanning. For MARK_COMPACTOR (the
  // default) all pointers are followed, whereas for young generation
  // collectors only pointers to objects in the young generation are followed.
  virtual GarbageCollector collector() const {
    return GarbageCollector::MARK_COMPACTOR;
  }
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
  // When V8_EXTERNAL_CODE_SPACE is enabled, visits a InstructionStream pointer
  // slot. The values may be modified on return. Not used when
  // V8_EXTERNAL_CODE_SPACE is not enabled (the InstructionStream pointer slots
  // are visited as a part of on-heap slot visitation - via VisitPointers()).
  virtual void VisitCodePointer(Code host, CodeObjectSlot slot) = 0;

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

  virtual void VisitCodeTarget(RelocInfo* rinfo) = 0;

  virtual void VisitEmbeddedPointer(RelocInfo* rinfo) = 0;

  virtual void VisitExternalReference(RelocInfo* rinfo) {}

  virtual void VisitExternalPointer(HeapObject host, ExternalPointerSlot slot,
                                    ExternalPointerTag tag) {}

  virtual void VisitInternalReference(RelocInfo* rinfo) {}

  // TODO(ishell): rename to VisitBuiltinEntry.
  virtual void VisitOffHeapTarget(RelocInfo* rinfo) {}

  // Visits the relocation info using the given iterator.
  void VisitRelocInfo(RelocIterator* it);

  virtual void VisitMapPointer(HeapObject host) { UNREACHABLE(); }
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
  // tagged values except references to InstructionStream objects.
  PtrComprCageBase cage_base() const {
#if V8_COMPRESS_POINTERS
    return cage_base_;
#else
    return PtrComprCageBase{};
#endif  // V8_COMPRESS_POINTERS
  }

  // The pointer compression cage base value used for decompression of
  // references to InstructionStream objects.
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
