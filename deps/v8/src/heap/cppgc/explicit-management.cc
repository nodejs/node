// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/cppgc/explicit-management.h"

#include <tuple>

#include "src/heap/cppgc/heap-base.h"
#include "src/heap/cppgc/heap-object-header.h"
#include "src/heap/cppgc/heap-page.h"
#include "src/heap/cppgc/sanitizers.h"

namespace cppgc {
namespace internal {

namespace {

std::pair<bool, BasePage*> CanModifyObject(void* object) {
  // object is guaranteed to be of type GarbageCollected, so getting the
  // BasePage is okay for regular and large objects.
  auto* base_page = BasePage::FromPayload(object);
  auto* heap = base_page->heap();
  // Whenever the GC is active, avoid modifying the object as it may mess with
  // state that the GC needs.
  const bool in_gc = heap->in_atomic_pause() || heap->marker() ||
                     heap->sweeper().IsSweepingInProgress();
  return {!in_gc, base_page};
}

}  // namespace

void FreeUnreferencedObject(void* object) {
  bool can_free;
  BasePage* base_page;
  std::tie(can_free, base_page) = CanModifyObject(object);
  if (!can_free) {
    return;
  }

  auto& header = HeapObjectHeader::FromPayload(object);
  header.Finalize();

  if (base_page->is_large()) {  // Large object.
    base_page->space()->RemovePage(base_page);
    base_page->heap()->stats_collector()->NotifyExplicitFree(
        LargePage::From(base_page)->PayloadSize());
    LargePage::Destroy(LargePage::From(base_page));
  } else {  // Regular object.
    const size_t header_size = header.GetSize();
    auto* normal_page = NormalPage::From(base_page);
    auto& normal_space = *static_cast<NormalPageSpace*>(base_page->space());
    auto& lab = normal_space.linear_allocation_buffer();
    ConstAddress payload_end = header.PayloadEnd();
    SET_MEMORY_INACCESSIBLE(&header, header_size);
    if (payload_end == lab.start()) {  // Returning to LAB.
      lab.Set(reinterpret_cast<Address>(&header), lab.size() + header_size);
      normal_page->object_start_bitmap().ClearBit(lab.start());
    } else {  // Returning to free list.
      base_page->heap()->stats_collector()->NotifyExplicitFree(header_size);
      normal_space.free_list().Add({&header, header_size});
      // No need to update the bitmap as the same bit is reused for the free
      // list entry.
    }
  }
}

namespace {

bool Grow(HeapObjectHeader& header, BasePage& base_page, size_t new_size,
          size_t size_delta) {
  DCHECK_GE(new_size, header.GetSize() + kAllocationGranularity);
  DCHECK_GE(size_delta, kAllocationGranularity);
  DCHECK(!base_page.is_large());

  auto& normal_space = *static_cast<NormalPageSpace*>(base_page.space());
  auto& lab = normal_space.linear_allocation_buffer();
  if (lab.start() == header.PayloadEnd() && lab.size() >= size_delta) {
    // LABs are considered used memory which means that no allocated size
    // adjustments are needed.
    Address delta_start = lab.Allocate(size_delta);
    SET_MEMORY_ACCESSIBLE(delta_start, size_delta);
    header.SetSize(new_size);
    return true;
  }
  return false;
}

bool Shrink(HeapObjectHeader& header, BasePage& base_page, size_t new_size,
            size_t size_delta) {
  DCHECK_GE(header.GetSize(), new_size + kAllocationGranularity);
  DCHECK_GE(size_delta, kAllocationGranularity);
  DCHECK(!base_page.is_large());

  auto& normal_space = *static_cast<NormalPageSpace*>(base_page.space());
  auto& lab = normal_space.linear_allocation_buffer();
  Address free_start = header.PayloadEnd() - size_delta;
  if (lab.start() == header.PayloadEnd()) {
    DCHECK_EQ(free_start, lab.start() - size_delta);
    // LABs are considered used memory which means that no allocated size
    // adjustments are needed.
    lab.Set(free_start, lab.size() + size_delta);
    SET_MEMORY_INACCESSIBLE(lab.start(), size_delta);
    header.SetSize(new_size);
    return true;
  }
  // Heuristic: Only return memory to the free list if the block is larger than
  // the smallest size class.
  if (size_delta >= ObjectAllocator::kSmallestSpaceSize) {
    SET_MEMORY_INACCESSIBLE(free_start, size_delta);
    base_page.heap()->stats_collector()->NotifyExplicitFree(size_delta);
    normal_space.free_list().Add({free_start, size_delta});
    NormalPage::From(&base_page)->object_start_bitmap().SetBit(free_start);
    header.SetSize(new_size);
  }
  // Return success in any case, as we want to avoid that embedders start
  // copying memory because of small deltas.
  return true;
}

}  // namespace

bool Resize(void* object, size_t new_object_size) {
  bool can_resize;
  BasePage* base_page;
  std::tie(can_resize, base_page) = CanModifyObject(object);
  if (!can_resize) {
    return false;
  }

  // TODO(chromium:1056170): Consider supporting large objects within certain
  // restrictions.
  if (base_page->is_large()) {
    return false;
  }

  const size_t new_size = RoundUp<kAllocationGranularity>(
      sizeof(HeapObjectHeader) + new_object_size);
  auto& header = HeapObjectHeader::FromPayload(object);
  const size_t old_size = header.GetSize();

  if (new_size > old_size) {
    return Grow(header, *base_page, new_size, new_size - old_size);
  } else if (old_size > new_size) {
    return Shrink(header, *base_page, new_size, old_size - new_size);
  }
  // Same size considering internal restrictions, e.g. alignment.
  return true;
}

}  // namespace internal
}  // namespace cppgc
