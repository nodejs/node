// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/read-only-heap.h"

#include <cstring>

#include "include/v8.h"
#include "src/base/lazy-instance.h"
#include "src/base/lsan.h"
#include "src/base/platform/mutex.h"
#include "src/heap/heap-write-barrier-inl.h"
#include "src/heap/memory-chunk.h"
#include "src/heap/read-only-spaces.h"
#include "src/heap/third-party/heap-api.h"
#include "src/objects/heap-object-inl.h"
#include "src/objects/objects-inl.h"
#include "src/objects/smi.h"
#include "src/snapshot/read-only-deserializer.h"

namespace v8 {
namespace internal {

#ifdef V8_SHARED_RO_HEAP
namespace {
// Mutex used to ensure that ReadOnlyArtifacts creation is only done once.
base::LazyMutex read_only_heap_creation_mutex_ = LAZY_MUTEX_INITIALIZER;

// Weak pointer holding ReadOnlyArtifacts. ReadOnlyHeap::SetUp creates a
// std::shared_ptr from this when it attempts to reuse it. Since all Isolates
// hold a std::shared_ptr to this, the object is destroyed when no Isolates
// remain.
base::LazyInstance<std::weak_ptr<ReadOnlyArtifacts>>::type
    read_only_artifacts_ = LAZY_INSTANCE_INITIALIZER;

std::shared_ptr<ReadOnlyArtifacts> InitializeSharedReadOnlyArtifacts() {
  auto artifacts = std::make_shared<ReadOnlyArtifacts>();
  *read_only_artifacts_.Pointer() = artifacts;
  return artifacts;
}
}  // namespace

// This ReadOnlyHeap instance will only be accessed by Isolates that are already
// set up. As such it doesn't need to be guarded by a mutex or shared_ptrs,
// since an already set up Isolate will hold a shared_ptr to
// read_only_artifacts_.
ReadOnlyHeap* ReadOnlyHeap::shared_ro_heap_ = nullptr;
#endif

// static
void ReadOnlyHeap::SetUp(Isolate* isolate, ReadOnlyDeserializer* des) {
  DCHECK_NOT_NULL(isolate);
#ifdef V8_SHARED_RO_HEAP

  bool read_only_heap_created = false;

  if (des != nullptr) {
    base::MutexGuard guard(read_only_heap_creation_mutex_.Pointer());
    std::shared_ptr<ReadOnlyArtifacts> artifacts =
        read_only_artifacts_.Get().lock();
    if (!artifacts) {
      artifacts = InitializeSharedReadOnlyArtifacts();
      shared_ro_heap_ = CreateAndAttachToIsolate(isolate, artifacts);
#ifdef DEBUG
      shared_ro_heap_->read_only_blob_checksum_ = des->GetChecksum();
#endif  // DEBUG
      shared_ro_heap_->DeseralizeIntoIsolate(isolate, des);
      read_only_heap_created = true;
    } else {
      isolate->SetUpFromReadOnlyArtifacts(artifacts);
    }
  } else {
    // This path should only be taken in mksnapshot, should only be run once
    // before tearing down the Isolate that holds this ReadOnlyArtifacts and is
    // not thread-safe.
    std::shared_ptr<ReadOnlyArtifacts> artifacts =
        read_only_artifacts_.Get().lock();
    CHECK(!artifacts);
    artifacts = InitializeSharedReadOnlyArtifacts();
    shared_ro_heap_ = CreateAndAttachToIsolate(isolate, artifacts);
    read_only_heap_created = true;
  }

#ifdef DEBUG
  const base::Optional<uint32_t> last_checksum =
      shared_ro_heap_->read_only_blob_checksum_;
  if (last_checksum) {
    // The read-only heap was set up from a snapshot. Make sure it's the always
    // the same snapshot.
    CHECK_WITH_MSG(des->GetChecksum(),
                   "Attempt to create the read-only heap after already "
                   "creating from a snapshot.");
    CHECK_EQ(last_checksum, des->GetChecksum());
  } else {
    // The read-only heap objects were created. Make sure this happens only
    // once, during this call.
    CHECK(read_only_heap_created);
  }
#endif  // DEBUG
  USE(read_only_heap_created);

  if (des != nullptr) {
    void* const isolate_ro_roots = reinterpret_cast<void*>(
        isolate->roots_table().read_only_roots_begin().address());
    std::memcpy(isolate_ro_roots, shared_ro_heap_->read_only_roots_,
                kEntriesCount * sizeof(Address));
  }
#else
  auto artifacts = std::make_shared<ReadOnlyArtifacts>();
  auto* ro_heap = CreateAndAttachToIsolate(isolate, artifacts);
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
ReadOnlyHeap* ReadOnlyHeap::CreateAndAttachToIsolate(
    Isolate* isolate, std::shared_ptr<ReadOnlyArtifacts> artifacts) {
  std::unique_ptr<ReadOnlyHeap> ro_heap(
      new ReadOnlyHeap(new ReadOnlySpace(isolate->heap())));
  artifacts->set_read_only_heap(std::move(ro_heap));
  isolate->SetUpFromReadOnlyArtifacts(artifacts);
  return artifacts->read_only_heap();
}

void ReadOnlyHeap::InitFromIsolate(Isolate* isolate) {
  DCHECK(!init_complete_);
  read_only_space_->ShrinkImmortalImmovablePages();
#ifdef V8_SHARED_RO_HEAP
  std::shared_ptr<ReadOnlyArtifacts> artifacts(*read_only_artifacts_.Pointer());
  read_only_space()->DetachPagesAndAddToArtifacts(artifacts);
  read_only_space_ = artifacts->shared_read_only_space();

  void* const isolate_ro_roots = reinterpret_cast<void*>(
      isolate->roots_table().read_only_roots_begin().address());
  std::memcpy(read_only_roots_, isolate_ro_roots,
              kEntriesCount * sizeof(Address));
  // N.B. Since pages are manually allocated with mmap, Lsan doesn't track their
  // pointers. Seal explicitly ignores the necessary objects.
  LSAN_IGNORE_OBJECT(this);
#else
  read_only_space_->Seal(ReadOnlySpace::SealMode::kDoNotDetachFromHeap);
#endif
  init_complete_ = true;
}

void ReadOnlyHeap::OnHeapTearDown() {
#ifndef V8_SHARED_RO_HEAP
  delete read_only_space_;
#endif
}

// static
void ReadOnlyHeap::PopulateReadOnlySpaceStatistics(
    SharedMemoryStatistics* statistics) {
  statistics->read_only_space_size_ = 0;
  statistics->read_only_space_used_size_ = 0;
  statistics->read_only_space_physical_size_ = 0;
#ifdef V8_SHARED_RO_HEAP
  std::shared_ptr<ReadOnlyArtifacts> artifacts =
      read_only_artifacts_.Get().lock();
  if (artifacts) {
    auto ro_space = artifacts->shared_read_only_space();
    statistics->read_only_space_size_ = ro_space->CommittedMemory();
    statistics->read_only_space_used_size_ = ro_space->SizeOfObjects();
    statistics->read_only_space_physical_size_ =
        ro_space->CommittedPhysicalMemory();
  }
#endif  // V8_SHARED_RO_HEAP
}

// static
bool ReadOnlyHeap::Contains(Address address) {
  return MemoryChunk::FromAddress(address)->InReadOnlySpace();
}

// static
bool ReadOnlyHeap::Contains(HeapObject object) {
  if (V8_ENABLE_THIRD_PARTY_HEAP_BOOL) {
    return third_party_heap::Heap::InReadOnlySpace(object.address());
  } else {
    return MemoryChunk::FromHeapObject(object)->InReadOnlySpace();
  }
}

Object* ReadOnlyHeap::ExtendReadOnlyObjectCache() {
  read_only_object_cache_.push_back(Smi::zero());
  return &read_only_object_cache_.back();
}

Object ReadOnlyHeap::cached_read_only_object(size_t i) const {
  DCHECK_LE(i, read_only_object_cache_.size());
  return read_only_object_cache_[i];
}

bool ReadOnlyHeap::read_only_object_cache_is_initialized() const {
  return read_only_object_cache_.size() > 0;
}

ReadOnlyHeapObjectIterator::ReadOnlyHeapObjectIterator(ReadOnlyHeap* ro_heap)
    : ReadOnlyHeapObjectIterator(ro_heap->read_only_space()) {}

ReadOnlyHeapObjectIterator::ReadOnlyHeapObjectIterator(ReadOnlySpace* ro_space)
    : ro_space_(ro_space),
      current_page_(V8_ENABLE_THIRD_PARTY_HEAP_BOOL ? nullptr
                                                    : ro_space->first_page()),
      current_addr_(V8_ENABLE_THIRD_PARTY_HEAP_BOOL
                        ? Address()
                        : current_page_->area_start()) {}

HeapObject ReadOnlyHeapObjectIterator::Next() {
  if (V8_ENABLE_THIRD_PARTY_HEAP_BOOL) {
    return HeapObject();  // Unsupported
  }

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

    if (object.IsFreeSpaceOrFiller()) {
      continue;
    }

    DCHECK_OBJECT_SIZE(object_size);
    return object;
  }
}

}  // namespace internal
}  // namespace v8
