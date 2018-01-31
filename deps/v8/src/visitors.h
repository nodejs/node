// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_VISITORS_H_
#define V8_VISITORS_H_

#include "src/globals.h"

namespace v8 {
namespace internal {

class CodeDataContainer;
class Object;

#define ROOT_ID_LIST(V)                                                    \
  V(kStringTable, "string_table", "(Internalized strings)")                \
  V(kExternalStringsTable, "external_strings_table", "(External strings)") \
  V(kStrongRootList, "strong_root_list", "(Strong roots)")                 \
  V(kSmiRootList, "smi_root_list", "(Smi roots)")                          \
  V(kBootstrapper, "bootstrapper", "(Bootstrapper)")                       \
  V(kTop, "top", "(Isolate)")                                              \
  V(kRelocatable, "relocatable", "(Relocatable)")                          \
  V(kDebug, "debug", "(Debugger)")                                         \
  V(kCompilationCache, "compilationcache", "(Compilation cache)")          \
  V(kHandleScope, "handlescope", "(Handle scope)")                         \
  V(kDispatchTable, "dispatchtable", "(Dispatch table)")                   \
  V(kBuiltins, "builtins", "(Builtins)")                                   \
  V(kGlobalHandles, "globalhandles", "(Global handles)")                   \
  V(kEternalHandles, "eternalhandles", "(Eternal handles)")                \
  V(kThreadManager, "threadmanager", "(Thread manager)")                   \
  V(kStrongRoots, "strong roots", "(Strong roots)")                        \
  V(kExtensions, "Extensions", "(Extensions)")

class VisitorSynchronization : public AllStatic {
 public:
#define DECLARE_ENUM(enum_item, ignore1, ignore2) enum_item,
  enum SyncTag { ROOT_ID_LIST(DECLARE_ENUM) kNumberOfSyncTags };
#undef DECLARE_ENUM

  static const char* const kTags[kNumberOfSyncTags];
  static const char* const kTagNames[kNumberOfSyncTags];
};

enum class Root {
#define DECLARE_ENUM(enum_item, ignore1, ignore2) enum_item,
  ROOT_ID_LIST(DECLARE_ENUM)
#undef DECLARE_ENUM
  // TODO(ulan): Merge with the ROOT_ID_LIST.
  kCodeFlusher,
  kPartialSnapshotCache,
  kWeakCollections,
  kWrapperTracing,
  kUnknown
};

// Abstract base class for visiting, and optionally modifying, the
// pointers contained in roots. Used in GC and serialization/deserialization.
class RootVisitor BASE_EMBEDDED {
 public:
  virtual ~RootVisitor() {}

  // Visits a contiguous arrays of pointers in the half-open range
  // [start, end). Any or all of the values may be modified on return.
  virtual void VisitRootPointers(Root root, Object** start, Object** end) = 0;

  // Handy shorthand for visiting a single pointer.
  virtual void VisitRootPointer(Root root, Object** p) {
    VisitRootPointers(root, p, p + 1);
  }

  // Intended for serialization/deserialization checking: insert, or
  // check for the presence of, a tag at this position in the stream.
  // Also used for marking up GC roots in heap snapshots.
  // TODO(ulan): Remove this.
  virtual void Synchronize(VisitorSynchronization::SyncTag tag) {}
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

  // Handy shorthand for visiting a single pointer.
  virtual void VisitPointer(HeapObject* host, Object** p) {
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
};

}  // namespace internal
}  // namespace v8

#endif  // V8_VISITORS_H_
