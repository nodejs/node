// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/read-only-heap.h"

#include <cstring>

#include "src/base/once.h"
#include "src/heap/heap-inl.h"
#include "src/heap/heap-write-barrier-inl.h"
#include "src/heap/spaces.h"
#include "src/objects/heap-object-inl.h"
#include "src/objects/objects-inl.h"
#include "src/objects/smi.h"
#include "src/snapshot/read-only-deserializer.h"

namespace v8 {
namespace internal {

#ifdef V8_SHARED_RO_HEAP
V8_DECLARE_ONCE(setup_ro_heap_once);
ReadOnlyHeap* shared_ro_heap = nullptr;
#endif

// static
void ReadOnlyHeap::SetUp(Isolate* isolate, ReadOnlyDeserializer* des) {
  DCHECK_NOT_NULL(isolate);
#ifdef V8_SHARED_RO_HEAP
  // Make sure we are only sharing read-only space when deserializing. Otherwise
  // we would be trying to create heap objects inside an already initialized
  // read-only space. Use ClearSharedHeapForTest if you need a new read-only
  // space.
  DCHECK_IMPLIES(shared_ro_heap != nullptr, des != nullptr);

  base::CallOnce(&setup_ro_heap_once, [isolate, des]() {
    shared_ro_heap = CreateAndAttachToIsolate(isolate);
    if (des != nullptr) shared_ro_heap->DeseralizeIntoIsolate(isolate, des);
  });

  isolate->heap()->SetUpFromReadOnlyHeap(shared_ro_heap);
  if (des != nullptr) {
    void* const isolate_ro_roots = reinterpret_cast<void*>(
        isolate->roots_table().read_only_roots_begin().address());
    std::memcpy(isolate_ro_roots, shared_ro_heap->read_only_roots_,
                kEntriesCount * sizeof(Address));
  }
#else
  auto* ro_heap = CreateAndAttachToIsolate(isolate);
  if (des != nullptr) ro_heap->DeseralizeIntoIsolate(isolate, des);
#endif  // V8_SHARED_RO_HEAP
}

void ReadOnlyHeap::DeseralizeIntoIsolate(Isolate* isolate,
                                         ReadOnlyDeserializer* des) {
  DCHECK_NOT_NULL(des);
  des->DeserializeInto(isolate);
  InitFromIsolate(isolate);
}

void ReadOnlyHeap::OnCreateHeapObjectsComplete(Isolate* isolate) {
  DCHECK_NOT_NULL(isolate);
  InitFromIsolate(isolate);
}

// static
ReadOnlyHeap* ReadOnlyHeap::CreateAndAttachToIsolate(Isolate* isolate) {
  auto* ro_heap = new ReadOnlyHeap(new ReadOnlySpace(isolate->heap()));
  isolate->heap()->SetUpFromReadOnlyHeap(ro_heap);
  return ro_heap;
}

void ReadOnlyHeap::InitFromIsolate(Isolate* isolate) {
  DCHECK(!init_complete_);
#ifdef V8_SHARED_RO_HEAP
  void* const isolate_ro_roots = reinterpret_cast<void*>(
      isolate->roots_table().read_only_roots_begin().address());
  std::memcpy(read_only_roots_, isolate_ro_roots,
              kEntriesCount * sizeof(Address));
  read_only_space_->Seal(ReadOnlySpace::SealMode::kDetachFromHeapAndForget);
#else
  read_only_space_->Seal(ReadOnlySpace::SealMode::kDoNotDetachFromHeap);
#endif
  init_complete_ = true;
}

void ReadOnlyHeap::OnHeapTearDown() {
#ifndef V8_SHARED_RO_HEAP
  delete read_only_space_;
  delete this;
#endif
}

// static
void ReadOnlyHeap::ClearSharedHeapForTest() {
#ifdef V8_SHARED_RO_HEAP
  DCHECK_NOT_NULL(shared_ro_heap);
  // TODO(v8:7464): Just leak read-only space for now. The paged-space heap
  // is null so there isn't a nice way to do this.
  delete shared_ro_heap;
  shared_ro_heap = nullptr;
  setup_ro_heap_once = 0;
#endif
}

// static
bool ReadOnlyHeap::Contains(HeapObject object) {
  return Page::FromAddress(object.ptr())->owner()->identity() == RO_SPACE;
}

// static
ReadOnlyRoots ReadOnlyHeap::GetReadOnlyRoots(HeapObject object) {
#ifdef V8_SHARED_RO_HEAP
  // This fails if we are creating heap objects and the roots haven't yet been
  // copied into the read-only heap or it has been cleared for testing.
  if (shared_ro_heap != nullptr && shared_ro_heap->init_complete_) {
    return ReadOnlyRoots(shared_ro_heap->read_only_roots_);
  }
#endif
  return ReadOnlyRoots(GetHeapFromWritableObject(object));
}

Object* ReadOnlyHeap::ExtendReadOnlyObjectCache() {
  read_only_object_cache_.push_back(Smi::kZero);
  return &read_only_object_cache_.back();
}

Object ReadOnlyHeap::cached_read_only_object(size_t i) const {
  DCHECK_LE(i, read_only_object_cache_.size());
  return read_only_object_cache_[i];
}

bool ReadOnlyHeap::read_only_object_cache_is_initialized() const {
  return read_only_object_cache_.size() > 0;
}

ReadOnlyHeapIterator::ReadOnlyHeapIterator(ReadOnlyHeap* ro_heap)
    : ReadOnlyHeapIterator(ro_heap->read_only_space()) {}

ReadOnlyHeapIterator::ReadOnlyHeapIterator(ReadOnlySpace* ro_space)
    : ro_space_(ro_space),
      current_page_(ro_space->first_page()),
      current_addr_(current_page_->area_start()) {}

HeapObject ReadOnlyHeapIterator::Next() {
  if (current_page_ == nullptr) {
    return HeapObject();
  }

  for (;;) {
    DCHECK_LE(current_addr_, current_page_->area_end());
    if (current_addr_ == current_page_->area_end()) {
      // Progress to the next page.
      current_page_ = current_page_->next_page();
      if (current_page_ == nullptr) {
        return HeapObject();
      }
      current_addr_ = current_page_->area_start();
    }

    if (current_addr_ == ro_space_->top() &&
        current_addr_ != ro_space_->limit()) {
      current_addr_ = ro_space_->limit();
      continue;
    }
    HeapObject object = HeapObject::FromAddress(current_addr_);
    const int object_size = object.Size();
    current_addr_ += object_size;

    if (object.IsFiller()) {
      continue;
    }

    DCHECK_OBJECT_SIZE(object_size);
    return object;
  }
}

}  // namespace internal
}  // namespace v8
