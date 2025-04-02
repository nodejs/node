// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_VISITORS_INL_H_
#define V8_OBJECTS_VISITORS_INL_H_

#include "src/codegen/reloc-info.h"
#include "src/common/globals.h"
#include "src/execution/isolate.h"
#include "src/heap/heap-layout-inl.h"
#include "src/objects/map.h"
#include "src/objects/visitors.h"

namespace v8 {
namespace internal {

ObjectVisitorWithCageBases::ObjectVisitorWithCageBases(
    PtrComprCageBase cage_base, PtrComprCageBase code_cage_base)
#if V8_COMPRESS_POINTERS
    : cage_base_(cage_base)
#ifdef V8_EXTERNAL_CODE_SPACE
      ,
      code_cage_base_(code_cage_base)
#endif  // V8_EXTERNAL_CODE_SPACE
#endif  // V8_COMPRESS_POINTERS
{
}

ObjectVisitorWithCageBases::ObjectVisitorWithCageBases(Isolate* isolate)
#if V8_COMPRESS_POINTERS
    : ObjectVisitorWithCageBases(PtrComprCageBase(isolate->cage_base()),
                                 PtrComprCageBase(isolate->code_cage_base()))
#else
    : ObjectVisitorWithCageBases(PtrComprCageBase(), PtrComprCageBase())
#endif  // V8_COMPRESS_POINTERS
{
}

ObjectVisitorWithCageBases::ObjectVisitorWithCageBases(Heap* heap)
    : ObjectVisitorWithCageBases(Isolate::FromHeap(heap)) {}

template <typename Visitor>
inline void ClientRootVisitor<Visitor>::VisitRunningCode(
    FullObjectSlot code_slot, FullObjectSlot maybe_istream_slot) {
#if DEBUG
  DCHECK(!HeapLayout::InWritableSharedSpace(Cast<HeapObject>(*code_slot)));
  Tagged<Object> maybe_istream = *maybe_istream_slot;
  DCHECK(maybe_istream == Smi::zero() ||
         !HeapLayout::InWritableSharedSpace(Cast<HeapObject>(maybe_istream)));
#endif
}

// static
template <typename Visitor>
bool ClientRootVisitor<Visitor>::IsSharedHeapObject(Tagged<Object> object) {
  return IsHeapObject(object) &&
         HeapLayout::InWritableSharedSpace(Cast<HeapObject>(object));
}

template <typename Visitor>
inline void ClientObjectVisitor<Visitor>::VisitMapPointer(
    Tagged<HeapObject> host) {
  if (!IsSharedHeapObject(host->map(cage_base()))) return;
  actual_visitor_->VisitMapPointer(host);
}

template <typename Visitor>
void ClientObjectVisitor<Visitor>::VisitInstructionStreamPointer(
    Tagged<Code> host, InstructionStreamSlot slot) {
#if DEBUG
  Tagged<Object> istream_object = slot.load(code_cage_base());
  Tagged<InstructionStream> istream;
  if (istream_object.GetHeapObject(&istream)) {
    DCHECK(!HeapLayout::InWritableSharedSpace(istream));
  }
#endif
}

template <typename Visitor>
inline void ClientObjectVisitor<Visitor>::VisitCodeTarget(
    Tagged<InstructionStream> host, RelocInfo* rinfo) {
#if DEBUG
  Tagged<InstructionStream> target =
      InstructionStream::FromTargetAddress(rinfo->target_address());
  DCHECK(!HeapLayout::InWritableSharedSpace(target));
#endif
}

template <typename Visitor>
inline void ClientObjectVisitor<Visitor>::VisitEmbeddedPointer(
    Tagged<InstructionStream> host, RelocInfo* rinfo) {
  if (!IsSharedHeapObject(rinfo->target_object(cage_base()))) return;
  actual_visitor_->VisitEmbeddedPointer(host, rinfo);
}

// static
template <typename Visitor>
bool ClientObjectVisitor<Visitor>::IsSharedHeapObject(Tagged<Object> object) {
  return IsHeapObject(object) &&
         HeapLayout::InWritableSharedSpace(Cast<HeapObject>(object));
}

}  // namespace internal
}  // namespace v8

#endif  // V8_OBJECTS_VISITORS_INL_H_
