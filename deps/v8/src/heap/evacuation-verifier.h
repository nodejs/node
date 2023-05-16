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

class EvacuationVerifier : public ObjectVisitorWithCageBases,
                           public RootVisitor {
 public:
  virtual void Run() = 0;

  void VisitPointers(HeapObject host, ObjectSlot start,
                     ObjectSlot end) override;

  void VisitPointers(HeapObject host, MaybeObjectSlot start,
                     MaybeObjectSlot end) override;

  void VisitCodePointer(Code host, CodeObjectSlot slot) override;

  void VisitRootPointers(Root root, const char* description,
                         FullObjectSlot start, FullObjectSlot end) override;

  void VisitMapPointer(HeapObject object) override;

 protected:
  explicit EvacuationVerifier(Heap* heap);

  inline Heap* heap() { return heap_; }

  virtual void VerifyMap(Map map) = 0;
  virtual void VerifyPointers(ObjectSlot start, ObjectSlot end) = 0;
  virtual void VerifyPointers(MaybeObjectSlot start, MaybeObjectSlot end) = 0;
  virtual void VerifyCodePointer(CodeObjectSlot slot) = 0;
  virtual void VerifyRootPointers(FullObjectSlot start, FullObjectSlot end) = 0;

  void VerifyRoots();
  void VerifyEvacuationOnPage(Address start, Address end);
  void VerifyEvacuation(NewSpace* new_space);
  void VerifyEvacuation(PagedSpaceBase* paged_space);

  Heap* heap_;
};

class FullEvacuationVerifier : public EvacuationVerifier {
 public:
  explicit FullEvacuationVerifier(Heap* heap);

  void Run() override;

 protected:
  V8_INLINE void VerifyHeapObjectImpl(HeapObject heap_object);

  V8_INLINE bool ShouldVerifyObject(HeapObject heap_object);

  template <typename TSlot>
  void VerifyPointersImpl(TSlot start, TSlot end);

  void VerifyMap(Map map) override;
  void VerifyPointers(ObjectSlot start, ObjectSlot end) override;
  void VerifyPointers(MaybeObjectSlot start, MaybeObjectSlot end) override;
  void VerifyCodePointer(CodeObjectSlot slot) override;
  void VisitCodeTarget(RelocInfo* rinfo) override;
  void VisitEmbeddedPointer(RelocInfo* rinfo) override;
  void VerifyRootPointers(FullObjectSlot start, FullObjectSlot end) override;
};

class YoungGenerationEvacuationVerifier : public EvacuationVerifier {
 public:
  explicit YoungGenerationEvacuationVerifier(Heap* heap);

  void Run() override;

 protected:
  V8_INLINE void VerifyHeapObjectImpl(HeapObject heap_object);

  template <typename TSlot>
  void VerifyPointersImpl(TSlot start, TSlot end);

  void VerifyMap(Map map) override;
  void VerifyPointers(ObjectSlot start, ObjectSlot end) override;
  void VerifyPointers(MaybeObjectSlot start, MaybeObjectSlot end) override;
  void VerifyCodePointer(CodeObjectSlot slot) override;
  void VisitCodeTarget(RelocInfo* rinfo) override;
  void VisitEmbeddedPointer(RelocInfo* rinfo) override;
  void VerifyRootPointers(FullObjectSlot start, FullObjectSlot end) override;
};

#endif  // VERIFY_HEAP

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_EVACUATION_VERIFIER_H_
