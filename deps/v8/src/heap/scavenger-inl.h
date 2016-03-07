// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_SCAVENGER_INL_H_
#define V8_HEAP_SCAVENGER_INL_H_

#include "src/heap/scavenger.h"

namespace v8 {
namespace internal {

void Scavenger::ScavengeObject(HeapObject** p, HeapObject* object) {
  DCHECK(object->GetIsolate()->heap()->InFromSpace(object));

  // We use the first word (where the map pointer usually is) of a heap
  // object to record the forwarding pointer.  A forwarding pointer can
  // point to an old space, the code space, or the to space of the new
  // generation.
  MapWord first_word = object->map_word();

  // If the first word is a forwarding address, the object has already been
  // copied.
  if (first_word.IsForwardingAddress()) {
    HeapObject* dest = first_word.ToForwardingAddress();
    DCHECK(object->GetIsolate()->heap()->InFromSpace(*p));
    *p = dest;
    return;
  }

  object->GetHeap()->UpdateAllocationSite<Heap::kGlobal>(
      object, object->GetHeap()->global_pretenuring_feedback_);

  // AllocationMementos are unrooted and shouldn't survive a scavenge
  DCHECK(object->map() != object->GetHeap()->allocation_memento_map());
  // Call the slow part of scavenge object.
  return ScavengeObjectSlow(p, object);
}


// static
void StaticScavengeVisitor::VisitPointer(Heap* heap, HeapObject* obj,
                                         Object** p) {
  Object* object = *p;
  if (!heap->InNewSpace(object)) return;
  Scavenger::ScavengeObject(reinterpret_cast<HeapObject**>(p),
                            reinterpret_cast<HeapObject*>(object));
}

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_SCAVENGER_INL_H_
