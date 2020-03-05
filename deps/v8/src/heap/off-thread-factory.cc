// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/off-thread-factory.h"

#include "src/ast/ast-value-factory.h"
#include "src/ast/ast.h"
#include "src/base/logging.h"
#include "src/common/globals.h"
#include "src/execution/isolate.h"
#include "src/handles/handles.h"
#include "src/heap/spaces-inl.h"
#include "src/heap/spaces.h"
#include "src/objects/fixed-array.h"
#include "src/objects/heap-object.h"
#include "src/objects/map-inl.h"
#include "src/objects/objects-body-descriptors-inl.h"
#include "src/objects/string.h"
#include "src/objects/visitors.h"
#include "src/roots/roots-inl.h"
#include "src/roots/roots.h"

namespace v8 {
namespace internal {

OffThreadFactory::OffThreadFactory(Isolate* isolate)
    : roots_(isolate), space_(isolate->heap()), lo_space_(isolate->heap()) {}

namespace {

class StringSlotCollectingVisitor : public ObjectVisitor {
 public:
  explicit StringSlotCollectingVisitor(ReadOnlyRoots roots) : roots_(roots) {}

  void VisitPointers(HeapObject host, ObjectSlot start,
                     ObjectSlot end) override {
    for (ObjectSlot slot = start; slot != end; ++slot) {
      Object obj = *slot;
      if (obj.IsInternalizedString() &&
          !ReadOnlyHeap::Contains(HeapObject::cast(obj))) {
        string_slots.emplace_back(host.ptr(), slot.address() - host.ptr());
      }
    }
  }
  void VisitPointers(HeapObject host, MaybeObjectSlot start,
                     MaybeObjectSlot end) override {
    for (MaybeObjectSlot slot = start; slot != end; ++slot) {
      MaybeObject maybe_obj = *slot;
      HeapObject obj;
      if (maybe_obj.GetHeapObjectIfStrong(&obj)) {
        if (obj.IsInternalizedString() && !ReadOnlyHeap::Contains(obj)) {
          string_slots.emplace_back(host.ptr(), slot.address() - host.ptr());
        }
      }
    }
  }

  void VisitCodeTarget(Code host, RelocInfo* rinfo) override { UNREACHABLE(); }
  void VisitEmbeddedPointer(Code host, RelocInfo* rinfo) override {
    UNREACHABLE();
  }

  std::vector<RelativeSlot> string_slots;

 private:
  ReadOnlyRoots roots_;
};

}  // namespace

void OffThreadFactory::FinishOffThread() {
  DCHECK(!is_finished);

  StringSlotCollectingVisitor string_slot_collector(read_only_roots());

  // First iterate all objects in the spaces to find string slots. At this point
  // all string slots have to point to off-thread strings or read-only strings.
  {
    PagedSpaceObjectIterator it(&space_);
    for (HeapObject obj = it.Next(); !obj.is_null(); obj = it.Next()) {
      obj.IterateBodyFast(&string_slot_collector);
    }
  }
  {
    LargeObjectSpaceObjectIterator it(&lo_space_);
    for (HeapObject obj = it.Next(); !obj.is_null(); obj = it.Next()) {
      obj.IterateBodyFast(&string_slot_collector);
    }
  }

  string_slots_ = std::move(string_slot_collector.string_slots);

  is_finished = true;
}

void OffThreadFactory::Publish(Isolate* isolate) {
  DCHECK(is_finished);

  HandleScope handle_scope(isolate);

  // First, handlify all the string slot holder objects, so that we can keep
  // track of them if they move.
  //
  // TODO(leszeks): We might be able to create a HandleScope-compatible
  // structure off-thread and merge it into the current handle scope all in one
  // go (DeferredHandles maybe?).
  std::vector<Handle<HeapObject>> heap_object_handles;
  heap_object_handles.reserve(string_slots_.size());
  for (RelativeSlot relative_slot : string_slots_) {
    // TODO(leszeks): Group slots in the same parent object to avoid creating
    // multiple duplicate handles.
    heap_object_handles.push_back(handle(
        HeapObject::cast(Object(relative_slot.object_address)), isolate));

    // De-internalize the string so that we can re-internalize it later.
    ObjectSlot slot(relative_slot.object_address + relative_slot.slot_offset);
    String string = String::cast(slot.Acquire_Load());
    bool one_byte = string.IsOneByteRepresentation();
    Map map = one_byte ? read_only_roots().one_byte_string_map()
                       : read_only_roots().string_map();
    string.set_map_no_write_barrier(map);
  }

  // Then merge the spaces. At this point, we are allowed to point between (no
  // longer) off-thread pages and main-thread heap pages, and objects in the
  // previously off-thread page can move.
  isolate->heap()->old_space()->MergeLocalSpace(&space_);
  isolate->heap()->lo_space()->MergeOffThreadSpace(&lo_space_);

  // Iterate the string slots, as an offset from the holders we have handles to.
  for (size_t i = 0; i < string_slots_.size(); ++i) {
    int slot_offset = string_slots_[i].slot_offset;

    // There's currently no cases where the holder object could have been
    // resized.
    DCHECK_LT(slot_offset, heap_object_handles[i]->Size());

    ObjectSlot slot(heap_object_handles[i]->ptr() + slot_offset);

    String string = String::cast(slot.Acquire_Load());
    if (string.IsThinString()) {
      // We may have already internalized this string via another slot.
      slot.Release_Store(ThinString::cast(string).GetUnderlying());
    } else {
      HandleScope handle_scope(isolate);

      Handle<String> string_handle = handle(string, isolate);
      Handle<String> internalized_string =
          isolate->factory()->InternalizeString(string_handle);

      // Recalculate the slot in case there was GC and the holder moved.
      ObjectSlot slot(heap_object_handles[i]->ptr() +
                      string_slots_[i].slot_offset);

      DCHECK(string_handle->IsThinString() ||
             string_handle->IsInternalizedString());
      if (*string_handle != *internalized_string) {
        slot.Release_Store(*internalized_string);
      }
    }
  }
}

OffThreadHandle<Object> OffThreadFactory::NewInvalidStringLengthError() {
  // TODO(leszeks): Implement.
  UNREACHABLE();
}

// Hacky method for creating a simple object with a slot pointing to a string.
// TODO(leszeks): Remove once we have full FixedArray support.
OffThreadHandle<FixedArray> OffThreadFactory::StringWrapperForTest(
    OffThreadHandle<String> string) {
  HeapObject wrapper =
      AllocateRaw(FixedArray::SizeFor(1), AllocationType::kOld);
  wrapper.set_map_after_allocation(read_only_roots().fixed_array_map());
  FixedArray array = FixedArray::cast(wrapper);
  array.set_length(1);
  array.data_start().Relaxed_Store(*string);
  return OffThreadHandle<FixedArray>(array);
}

OffThreadHandle<String> OffThreadFactory::MakeOrFindTwoCharacterString(
    uint16_t c1, uint16_t c2) {
  // TODO(leszeks): Do some real caching here. Also, these strings should be
  // internalized.
  if ((c1 | c2) <= unibrow::Latin1::kMaxChar) {
    OffThreadHandle<SeqOneByteString> ret =
        NewRawOneByteString(2, AllocationType::kOld);
    ret->SeqOneByteStringSet(0, c1);
    ret->SeqOneByteStringSet(1, c2);
    return ret;
  }
  OffThreadHandle<SeqTwoByteString> ret =
      NewRawTwoByteString(2, AllocationType::kOld);
  ret->SeqTwoByteStringSet(0, c1);
  ret->SeqTwoByteStringSet(1, c2);
  return ret;
}

HeapObject OffThreadFactory::AllocateRaw(int size, AllocationType allocation,
                                         AllocationAlignment alignment) {
  DCHECK(!is_finished);

  DCHECK_EQ(allocation, AllocationType::kOld);
  AllocationResult result;
  if (size > kMaxRegularHeapObjectSize) {
    result = lo_space_.AllocateRaw(size);
  } else {
    result = space_.AllocateRaw(size, alignment);
  }
  return result.ToObjectChecked();
}

void OffThreadFactory::FatalProcessOutOfHeapMemory(const char* location) {
  // TODO(leszeks): Do something reasonable.
  UNREACHABLE();
}

}  // namespace internal
}  // namespace v8
