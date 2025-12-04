// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_EVACUATION_VERIFIER_H_
#define V8_HEAP_EVACUATION_VERIFIER_H_

#include "src/heap/new-spaces.h"
#include "src/heap/paged-spaces.h"
#include "src/objects/map.h"
#include "src/objects/visitors.h"

namespace v8 {
namespace internal {

#ifdef VERIFY_HEAP

class EvacuationVerifier final : public ObjectVisitorWithCageBases,
                                 public RootVisitor {
 public:
  explicit EvacuationVerifier(Heap* heap);

  void Run();

  void VisitPointers(Tagged<HeapObject> host, ObjectSlot start,
                     ObjectSlot end) final;
  void VisitPointers(Tagged<HeapObject> host, MaybeObjectSlot start,
                     MaybeObjectSlot end) final;
  void VisitInstructionStreamPointer(Tagged<Code> host,
                                     InstructionStreamSlot slot) final;
  void VisitRootPointers(Root root, const char* description,
                         FullObjectSlot start, FullObjectSlot end) final;
  void VisitMapPointer(Tagged<HeapObject> object) final;
  void VisitCodeTarget(Tagged<InstructionStream> host, RelocInfo* rinfo) final;
  void VisitEmbeddedPointer(Tagged<InstructionStream> host,
                            RelocInfo* rinfo) final;

 private:
  V8_INLINE void VerifyHeapObjectImpl(Tagged<HeapObject> heap_object);
  V8_INLINE bool ShouldVerifyObject(Tagged<HeapObject> heap_object);

  template <typename TSlot>
  void VerifyPointersImpl(TSlot start, TSlot end);

  void VerifyRoots();
  void VerifyEvacuationOnPage(Address start, Address end);
  void VerifyEvacuation(NewSpace* new_space);
  void VerifyEvacuation(PagedSpaceBase* paged_space);

  Heap* heap_;
};

#endif  // VERIFY_HEAP

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_EVACUATION_VERIFIER_H_
