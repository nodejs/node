// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_REMEMBERED_SET_INL_H_
#define V8_HEAP_REMEMBERED_SET_INL_H_

#include "src/common/ptr-compr-inl.h"
#include "src/heap/remembered-set.h"

namespace v8 {
namespace internal {

template <typename Callback>
SlotCallbackResult UpdateTypedSlotHelper::UpdateTypedSlot(Heap* heap,
                                                          SlotType slot_type,
                                                          Address addr,
                                                          Callback callback) {
  switch (slot_type) {
    case CODE_TARGET_SLOT: {
      RelocInfo rinfo(addr, RelocInfo::CODE_TARGET, 0, Code());
      return UpdateCodeTarget(&rinfo, callback);
    }
    case CODE_ENTRY_SLOT: {
      return UpdateCodeEntry(addr, callback);
    }
    case COMPRESSED_EMBEDDED_OBJECT_SLOT: {
      RelocInfo rinfo(addr, RelocInfo::COMPRESSED_EMBEDDED_OBJECT, 0, Code());
      return UpdateEmbeddedPointer(heap, &rinfo, callback);
    }
    case FULL_EMBEDDED_OBJECT_SLOT: {
      RelocInfo rinfo(addr, RelocInfo::FULL_EMBEDDED_OBJECT, 0, Code());
      return UpdateEmbeddedPointer(heap, &rinfo, callback);
    }
    case DATA_EMBEDDED_OBJECT_SLOT: {
      RelocInfo rinfo(addr, RelocInfo::DATA_EMBEDDED_OBJECT, 0, Code());
      return UpdateEmbeddedPointer(heap, &rinfo, callback);
    }
    case COMPRESSED_OBJECT_SLOT: {
      HeapObject old_target = HeapObject::cast(Object(
          DecompressTaggedAny(heap->isolate(), base::Memory<Tagged_t>(addr))));
      HeapObject new_target = old_target;
      SlotCallbackResult result = callback(FullMaybeObjectSlot(&new_target));
      DCHECK(!HasWeakHeapObjectTag(new_target));
      if (new_target != old_target) {
        base::Memory<Tagged_t>(addr) = CompressTagged(new_target.ptr());
      }
      return result;
    }
    case FULL_OBJECT_SLOT: {
      return callback(FullMaybeObjectSlot(addr));
    }
    case CLEARED_SLOT:
      break;
  }
  UNREACHABLE();
}

}  // namespace internal
}  // namespace v8
#endif  // V8_HEAP_REMEMBERED_SET_INL_H_
