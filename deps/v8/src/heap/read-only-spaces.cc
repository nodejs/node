// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/read-only-spaces.h"

#include <memory>

#include "include/v8-internal.h"
#include "include/v8-platform.h"
#include "src/base/logging.h"
#include "src/common/globals.h"
#include "src/common/ptr-compr-inl.h"
#include "src/execution/isolate.h"
#include "src/heap/allocation-stats.h"
#include "src/heap/basic-memory-chunk.h"
#include "src/heap/combined-heap.h"
#include "src/heap/heap-inl.h"
#include "src/heap/memory-allocator.h"
#include "src/heap/memory-chunk.h"
#include "src/heap/read-only-heap.h"
#include "src/objects/objects-inl.h"
#include "src/objects/property-details.h"
#include "src/objects/string.h"
#include "src/snapshot/read-only-deserializer.h"

namespace v8 {
namespace internal {

void CopyAndRebaseRoots(Address* src, Address* dst, Address new_base) {
  Address src_base = GetIsolateRootAddress(src[0]);
  for (size_t i = 0; i < ReadOnlyHeap::kEntriesCount; ++i) {
    dst[i] = src[i] - src_base + new_base;
  }
}

void ReadOnlyArtifacts::set_read_only_heap(
    std::unique_ptr<ReadOnlyHeap> read_only_heap) {
  read_only_heap_ = std::move(read_only_heap);
}

void ReadOnlyArtifacts::InitializeChecksum(
    SnapshotData* read_only_snapshot_data) {
#ifdef DEBUG
  read_only_blob_checksum_ = Checksum(read_only_snapshot_data->Payload());
#endif  // DEBUG
}

void ReadOnlyArtifacts::VerifyChecksum(SnapshotData* read_only_snapshot_data,
                                       bool read_only_heap_created) {
#ifdef DEBUG
  if (read_only_blob_checksum_) {
    // The read-only heap was set up from a snapshot. Make sure it's the always
    // the same snapshot.
    uint32_t snapshot_checksum = Checksum(read_only_snapshot_data->Payload());
    CHECK_WITH_MSG(snapshot_checksum,
                   "Attempt to create the read-only heap after already "
                   "creating from a snapshot.");
    if (!FLAG_stress_snapshot) {
      // --stress-snapshot is only intended to check how well the
      // serializer/deserializer copes with unexpected objects, and is not
      // intended to test whether the newly deserialized Isolate would actually
      // work since it serializes a currently running Isolate, which is not
      // supported. As a result, it's possible that it will create a new
      // read-only snapshot that is not compatible with the original one (for
      // instance due to the string table being re-ordered). Since we won't
      // acutally use that new Isoalte, we're ok with any potential corruption.
      // See crbug.com/1043058.
      CHECK_EQ(read_only_blob_checksum_, snapshot_checksum);
    }
  } else {
    // If there's no checksum, then that means the read-only heap objects are
    // being created.
    CHECK(read_only_heap_created);
  }
#endif  // DEBUG
}

SingleCopyReadOnlyArtifacts::~SingleCopyReadOnlyArtifacts() {
  // This particular SharedReadOnlySpace should not destroy its own pages as
  // TearDown requires MemoryAllocator which itself is tied to an Isolate.
  shared_read_only_space_->pages_.resize(0);

  for (ReadOnlyPage* chunk : pages_) {
    void* chunk_address = reinterpret_cast<void*>(chunk->address());
    size_t size = RoundUp(chunk->size(), page_allocator_->AllocatePageSize());
    CHECK(page_allocator_->FreePages(chunk_address, size));
  }
}

ReadOnlyHeap* SingleCopyReadOnlyArtifacts::GetReadOnlyHeapForIsolate(
    Isolate* isolate) {
  return read_only_heap();
}

void SingleCopyReadOnlyArtifacts::Initialize(Isolate* isolate,
                                             std::vector<ReadOnlyPage*>&& pages,
                                             const AllocationStats& stats) {
  // Do not use the platform page allocator when sharing a pointer compression
  // cage, as the Isolate's page allocator is a BoundedPageAllocator tied to the
  // shared cage.
  page_allocator_ = COMPRESS_POINTERS_IN_SHARED_CAGE_BOOL
                        ? isolate->page_allocator()
                        : GetPlatformPageAllocator();
  pages_ = std::move(pages);
  set_accounting_stats(stats);
  set_shared_read_only_space(
      std::make_unique<SharedReadOnlySpace>(isolate->heap(), this));
}

void SingleCopyReadOnlyArtifacts::ReinstallReadOnlySpace(Isolate* isolate) {
  isolate->heap()->ReplaceReadOnlySpace(shared_read_only_space());
}

void SingleCopyReadOnlyArtifacts::VerifyHeapAndSpaceRelationships(
    Isolate* isolate) {
  DCHECK_EQ(read_only_heap()->read_only_space(), shared_read_only_space());

  // Confirm the Isolate is using the shared ReadOnlyHeap and ReadOnlySpace.
  DCHECK_EQ(read_only_heap(), isolate->read_only_heap());
  DCHECK_EQ(shared_read_only_space(), isolate->heap()->read_only_space());
}

void PointerCompressedReadOnlyArtifacts::InitializeRootsFrom(Isolate* isolate) {
  auto isolate_ro_roots =
      isolate->roots_table().read_only_roots_begin().location();
  CopyAndRebaseRoots(isolate_ro_roots, read_only_roots_, 0);
}

void PointerCompressedReadOnlyArtifacts::InitializeRootsIn(Isolate* isolate) {
  auto isolate_ro_roots =
      isolate->roots_table().read_only_roots_begin().location();
  CopyAndRebaseRoots(read_only_roots_, isolate_ro_roots,
                     isolate->isolate_root());
}

SharedReadOnlySpace* PointerCompressedReadOnlyArtifacts::CreateReadOnlySpace(
    Isolate* isolate) {
  AllocationStats new_stats;
  new_stats.IncreaseCapacity(accounting_stats().Capacity());

  std::vector<std::unique_ptr<v8::PageAllocator::SharedMemoryMapping>> mappings;
  std::vector<ReadOnlyPage*> pages;
  Address isolate_root = isolate->isolate_root();
  for (size_t i = 0; i < pages_.size(); ++i) {
    const ReadOnlyPage* page = pages_[i];
    const Tagged_t offset = OffsetForPage(i);
    Address new_address = isolate_root + offset;
    ReadOnlyPage* new_page = nullptr;
    bool success = isolate->heap()
                       ->memory_allocator()
                       ->data_page_allocator()
                       ->ReserveForSharedMemoryMapping(
                           reinterpret_cast<void*>(new_address), page->size());
    CHECK(success);
    auto shared_memory = RemapPageTo(i, new_address, new_page);
    // Later it's possible that this might fail, but for now on Linux this is
    // not possible. When we move onto windows, it's not possible to reserve
    // memory and then map into the middle of it at which point we will have to
    // reserve the memory free it and then attempt to remap to it which could
    // fail. At that point this will need to change.
    CHECK(shared_memory);
    CHECK_NOT_NULL(new_page);

    new_stats.IncreaseAllocatedBytes(page->allocated_bytes(), new_page);
    mappings.push_back(std::move(shared_memory));
    pages.push_back(new_page);
  }

  auto* shared_read_only_space =
      new SharedReadOnlySpace(isolate->heap(), std::move(pages),
                              std::move(mappings), std::move(new_stats));
  return shared_read_only_space;
}

ReadOnlyHeap* PointerCompressedReadOnlyArtifacts::GetReadOnlyHeapForIsolate(
    Isolate* isolate) {
  DCHECK(ReadOnlyHeap::IsReadOnlySpaceShared());
  InitializeRootsIn(isolate);

  SharedReadOnlySpace* shared_read_only_space = CreateReadOnlySpace(isolate);
  ReadOnlyHeap* read_only_heap = new ReadOnlyHeap(shared_read_only_space);

  // TODO(v8:10699): The cache should just live uncompressed in
  // ReadOnlyArtifacts and be decompressed on the fly.
  auto original_cache = read_only_heap_->read_only_object_cache_;
  auto& cache = read_only_heap->read_only_object_cache_;
  Address isolate_root = isolate->isolate_root();
  for (Object original_object : original_cache) {
    Address original_address = original_object.ptr();
    Address new_address = isolate_root + CompressTagged(original_address);
    Object new_object = Object(new_address);
    cache.push_back(new_object);
  }

  return read_only_heap;
}

std::unique_ptr<::v8::PageAllocator::SharedMemoryMapping>
PointerCompressedReadOnlyArtifacts::RemapPageTo(size_t i, Address new_address,
                                                ReadOnlyPage*& new_page) {
  std::unique_ptr<::v8::PageAllocator::SharedMemoryMapping> mapping =
      shared_memory_[i]->RemapTo(reinterpret_cast<void*>(new_address));
  if (mapping) {
    new_page = static_cast<ReadOnlyPage*>(reinterpret_cast<void*>(new_address));
    return mapping;
  } else {
    return {};
  }
}

void PointerCompressedReadOnlyArtifacts::Initialize(
    Isolate* isolate, std::vector<ReadOnlyPage*>&& pages,
    const AllocationStats& stats) {
  DCHECK(ReadOnlyHeap::IsReadOnlySpaceShared());
  DCHECK(pages_.empty());
  DCHECK(!pages.empty());

  // It's not possible to copy the AllocationStats directly as the new pages
  // will be mapped to different addresses.
  stats_.IncreaseCapacity(stats.Capacity());

  v8::PageAllocator* page_allocator = GetPlatformPageAllocator();
  DCHECK(page_allocator->CanAllocateSharedPages());

  for (const ReadOnlyPage* page : pages) {
    size_t size = RoundUp(page->size(), page_allocator->AllocatePageSize());
    // 1. Allocate some new memory for a shared copy of the page and copy the
    // original contents into it. Doesn't need to be V8 page aligned, since
    // we'll never use it directly.
    auto shared_memory = page_allocator->AllocateSharedPages(size, page);
    void* ptr = shared_memory->GetMemory();
    CHECK_NOT_NULL(ptr);

    // 2. Copy the contents of the original page into the shared page.
    ReadOnlyPage* new_page = reinterpret_cast<ReadOnlyPage*>(ptr);

    pages_.push_back(new_page);
    shared_memory_.push_back(std::move(shared_memory));
    // This is just CompressTagged but inlined so it will always compile.
    Tagged_t compressed_address = CompressTagged(page->address());
    page_offsets_.push_back(compressed_address);

    // 3. Update the accounting stats so the allocated bytes are for the new
    // shared page rather than the original.
    stats_.IncreaseAllocatedBytes(page->allocated_bytes(), new_page);
  }

  InitializeRootsFrom(isolate);
  set_shared_read_only_space(
      std::make_unique<SharedReadOnlySpace>(isolate->heap(), this));
}

void PointerCompressedReadOnlyArtifacts::ReinstallReadOnlySpace(
    Isolate* isolate) {
  // We need to build a new SharedReadOnlySpace that occupies the same memory as
  // the original one, so first the original space's pages must be freed.
  Heap* heap = isolate->heap();
  heap->read_only_space()->TearDown(heap->memory_allocator());

  heap->ReplaceReadOnlySpace(CreateReadOnlySpace(heap->isolate()));

  DCHECK_NE(heap->read_only_space(), shared_read_only_space());

  // Also recreate the ReadOnlyHeap using the this space.
  auto* ro_heap = new ReadOnlyHeap(isolate->read_only_heap(),
                                   isolate->heap()->read_only_space());
  isolate->set_read_only_heap(ro_heap);

  DCHECK_NE(*isolate->roots_table().read_only_roots_begin().location(), 0);
}

void PointerCompressedReadOnlyArtifacts::VerifyHeapAndSpaceRelationships(
    Isolate* isolate) {
  // Confirm the canonical versions of the ReadOnlySpace/ReadOnlyHeap from the
  // ReadOnlyArtifacts are not accidentally present in a real Isolate (which
  // might destroy them) and the ReadOnlyHeaps and Spaces are correctly
  // associated with each other.
  DCHECK_NE(shared_read_only_space(), isolate->heap()->read_only_space());
  DCHECK_NE(read_only_heap(), isolate->read_only_heap());
  DCHECK_EQ(read_only_heap()->read_only_space(), shared_read_only_space());
  DCHECK_EQ(isolate->read_only_heap()->read_only_space(),
            isolate->heap()->read_only_space());
}

// -----------------------------------------------------------------------------
// ReadOnlySpace implementation

ReadOnlySpace::ReadOnlySpace(Heap* heap)
    : BaseSpace(heap, RO_SPACE),
      top_(kNullAddress),
      limit_(kNullAddress),
      is_string_padding_cleared_(heap->isolate()->initialized_from_snapshot()),
      capacity_(0),
      area_size_(MemoryChunkLayout::AllocatableMemoryInMemoryChunk(RO_SPACE)) {}

// Needs to be defined in the cc file to force the vtable to be emitted in
// component builds.
ReadOnlySpace::~ReadOnlySpace() = default;

void SharedReadOnlySpace::TearDown(MemoryAllocator* memory_allocator) {
  // SharedReadOnlySpaces do not tear down their own pages since they are either
  // freed down by the ReadOnlyArtifacts that contains them or in the case of
  // pointer compression, they are freed when the SharedMemoryMappings are
  // freed.
  pages_.resize(0);
  accounting_stats_.Clear();
}

void ReadOnlySpace::TearDown(MemoryAllocator* memory_allocator) {
  for (ReadOnlyPage* chunk : pages_) {
    memory_allocator->FreeReadOnlyPage(chunk);
  }
  pages_.resize(0);
  accounting_stats_.Clear();
}

void ReadOnlySpace::DetachPagesAndAddToArtifacts(
    std::shared_ptr<ReadOnlyArtifacts> artifacts) {
  DCHECK(ReadOnlyHeap::IsReadOnlySpaceShared());

  Heap* heap = ReadOnlySpace::heap();
  // Without pointer compression in a per-Isolate cage, ReadOnlySpace pages are
  // directly shared between all heaps and so must be unregistered from their
  // originating allocator.
  Seal(COMPRESS_POINTERS_IN_ISOLATE_CAGE_BOOL
           ? SealMode::kDetachFromHeap
           : SealMode::kDetachFromHeapAndUnregisterMemory);
  artifacts->Initialize(heap->isolate(), std::move(pages_), accounting_stats_);
}

ReadOnlyPage::ReadOnlyPage(Heap* heap, BaseSpace* space, size_t chunk_size,
                           Address area_start, Address area_end,
                           VirtualMemory reservation)
    : BasicMemoryChunk(heap, space, chunk_size, area_start, area_end,
                       std::move(reservation)) {
  allocated_bytes_ = 0;
  SetFlags(Flag::NEVER_EVACUATE | Flag::READ_ONLY_HEAP);
  heap->incremental_marking()
      ->non_atomic_marking_state()
      ->bitmap(this)
      ->MarkAllBits();
}

void ReadOnlyPage::MakeHeaderRelocatable() {
  heap_ = nullptr;
  owner_ = nullptr;
  reservation_.Reset();
}

void ReadOnlySpace::SetPermissionsForPages(MemoryAllocator* memory_allocator,
                                           PageAllocator::Permission access) {
  for (BasicMemoryChunk* chunk : pages_) {
    // Read only pages don't have valid reservation object so we get proper
    // page allocator manually.
    v8::PageAllocator* page_allocator =
        memory_allocator->page_allocator(NOT_EXECUTABLE);
    CHECK(SetPermissions(page_allocator, chunk->address(), chunk->size(),
                         access));
  }
}

// After we have booted, we have created a map which represents free space
// on the heap.  If there was already a free list then the elements on it
// were created with the wrong FreeSpaceMap (normally nullptr), so we need to
// fix them.
void ReadOnlySpace::RepairFreeSpacesAfterDeserialization() {
  BasicMemoryChunk::UpdateHighWaterMark(top_);
  // Each page may have a small free space that is not tracked by a free list.
  // Those free spaces still contain null as their map pointer.
  // Overwrite them with new fillers.
  for (BasicMemoryChunk* chunk : pages_) {
    Address start = chunk->HighWaterMark();
    Address end = chunk->area_end();
    // Put a filler object in the gap between the end of the allocated objects
    // and the end of the allocatable area.
    if (start < end) {
      heap()->CreateFillerObjectAt(start, static_cast<int>(end - start),
                                   ClearRecordedSlots::kNo);
    }
  }
}

void ReadOnlySpace::ClearStringPaddingIfNeeded() {
  if (V8_ENABLE_THIRD_PARTY_HEAP_BOOL) {
    // TODO(v8:11641): Revisit this once third-party heap supports iteration.
    return;
  }
  if (is_string_padding_cleared_) return;

  ReadOnlyHeapObjectIterator iterator(this);
  for (HeapObject o = iterator.Next(); !o.is_null(); o = iterator.Next()) {
    if (o.IsSeqOneByteString()) {
      SeqOneByteString::cast(o).clear_padding();
    } else if (o.IsSeqTwoByteString()) {
      SeqTwoByteString::cast(o).clear_padding();
    }
  }
  is_string_padding_cleared_ = true;
}

void ReadOnlySpace::Seal(SealMode ro_mode) {
  DCHECK(!is_marked_read_only_);

  FreeLinearAllocationArea();
  is_marked_read_only_ = true;
  auto* memory_allocator = heap()->memory_allocator();

  if (ro_mode != SealMode::kDoNotDetachFromHeap) {
    DetachFromHeap();
    for (ReadOnlyPage* p : pages_) {
      if (ro_mode == SealMode::kDetachFromHeapAndUnregisterMemory) {
        memory_allocator->UnregisterReadOnlyPage(p);
      }
      if (ReadOnlyHeap::IsReadOnlySpaceShared()) {
        p->MakeHeaderRelocatable();
      }
    }
  }

  SetPermissionsForPages(memory_allocator, PageAllocator::kRead);
}

void ReadOnlySpace::Unseal() {
  DCHECK(is_marked_read_only_);
  if (!pages_.empty()) {
    SetPermissionsForPages(heap()->memory_allocator(),
                           PageAllocator::kReadWrite);
  }
  is_marked_read_only_ = false;
}

bool ReadOnlySpace::ContainsSlow(Address addr) const {
  BasicMemoryChunk* c = BasicMemoryChunk::FromAddress(addr);
  for (BasicMemoryChunk* chunk : pages_) {
    if (chunk == c) return true;
  }
  return false;
}

namespace {
// Only iterates over a single chunk as the chunk iteration is done externally.
class ReadOnlySpaceObjectIterator : public ObjectIterator {
 public:
  ReadOnlySpaceObjectIterator(const Heap* heap, const ReadOnlySpace* space,
                              BasicMemoryChunk* chunk)
      : cur_addr_(kNullAddress), cur_end_(kNullAddress), space_(space) {}

  // Advance to the next object, skipping free spaces and other fillers and
  // skipping the special garbage section of which there is one per space.
  // Returns nullptr when the iteration has ended.
  HeapObject Next() override {
    HeapObject next_obj = FromCurrentPage();
    if (!next_obj.is_null()) return next_obj;
    return HeapObject();
  }

 private:
  HeapObject FromCurrentPage() {
    while (cur_addr_ != cur_end_) {
      if (cur_addr_ == space_->top() && cur_addr_ != space_->limit()) {
        cur_addr_ = space_->limit();
        continue;
      }
      HeapObject obj = HeapObject::FromAddress(cur_addr_);
      const int obj_size = obj.Size();
      cur_addr_ += obj_size;
      DCHECK_LE(cur_addr_, cur_end_);
      if (!obj.IsFreeSpaceOrFiller()) {
        if (obj.IsCode()) {
          DCHECK(Code::cast(obj).is_builtin());
          DCHECK_CODEOBJECT_SIZE(obj_size, space_);
        } else {
          DCHECK_OBJECT_SIZE(obj_size);
        }
        return obj;
      }
    }
    return HeapObject();
  }

  Address cur_addr_;  // Current iteration point.
  Address cur_end_;   // End iteration point.
  const ReadOnlySpace* const space_;
};
}  // namespace

#ifdef VERIFY_HEAP
namespace {
class VerifyReadOnlyPointersVisitor : public VerifyPointersVisitor {
 public:
  explicit VerifyReadOnlyPointersVisitor(Heap* heap)
      : VerifyPointersVisitor(heap) {}

 protected:
  void VerifyPointers(HeapObject host, MaybeObjectSlot start,
                      MaybeObjectSlot end) override {
    if (!host.is_null()) {
      CHECK(ReadOnlyHeap::Contains(host.map()));
    }
    VerifyPointersVisitor::VerifyPointers(host, start, end);

    for (MaybeObjectSlot current = start; current < end; ++current) {
      HeapObject heap_object;
      if ((*current)->GetHeapObject(&heap_object)) {
        CHECK(ReadOnlyHeap::Contains(heap_object));
      }
    }
  }
};
}  // namespace

void ReadOnlySpace::Verify(Isolate* isolate) const {
  bool allocation_pointer_found_in_space = top_ == limit_;
  VerifyReadOnlyPointersVisitor visitor(isolate->heap());

  for (BasicMemoryChunk* page : pages_) {
    if (ReadOnlyHeap::IsReadOnlySpaceShared()) {
      CHECK_NULL(page->owner());
    } else {
      CHECK_EQ(page->owner(), this);
    }

    if (page == Page::FromAllocationAreaAddress(top_)) {
      allocation_pointer_found_in_space = true;
    }
    ReadOnlySpaceObjectIterator it(isolate->heap(), this, page);
    Address end_of_previous_object = page->area_start();
    Address top = page->area_end();

    for (HeapObject object = it.Next(); !object.is_null(); object = it.Next()) {
      CHECK(end_of_previous_object <= object.address());

      Map map = object.map();
      CHECK(map.IsMap());

      // The object itself should look OK.
      object.ObjectVerify(isolate);

      // All the interior pointers should be contained in the heap.
      int size = object.Size();
      object.IterateBody(map, size, &visitor);
      CHECK(object.address() + size <= top);
      end_of_previous_object = object.address() + size;

      CHECK(!object.IsExternalString());
      CHECK(!object.IsJSArrayBuffer());
    }

    CHECK(!page->IsFlagSet(Page::PAGE_NEW_OLD_PROMOTION));
    CHECK(!page->IsFlagSet(Page::PAGE_NEW_NEW_PROMOTION));
  }
  CHECK(allocation_pointer_found_in_space);

#ifdef DEBUG
  VerifyCounters(isolate->heap());
#endif
}

#ifdef DEBUG
void ReadOnlySpace::VerifyCounters(Heap* heap) const {
  size_t total_capacity = 0;
  size_t total_allocated = 0;
  for (BasicMemoryChunk* page : pages_) {
    total_capacity += page->area_size();
    ReadOnlySpaceObjectIterator it(heap, this, page);
    size_t real_allocated = 0;
    for (HeapObject object = it.Next(); !object.is_null(); object = it.Next()) {
      if (!object.IsFreeSpaceOrFiller()) {
        real_allocated += object.Size();
      }
    }
    total_allocated += page->allocated_bytes();
    // The real size can be smaller than the accounted size if array trimming,
    // object slack tracking happened after sweeping.
    DCHECK_LE(real_allocated, accounting_stats_.AllocatedOnPage(page));
    DCHECK_EQ(page->allocated_bytes(), accounting_stats_.AllocatedOnPage(page));
  }
  DCHECK_EQ(total_capacity, accounting_stats_.Capacity());
  DCHECK_EQ(total_allocated, accounting_stats_.Size());
}
#endif  // DEBUG
#endif  // VERIFY_HEAP

size_t ReadOnlySpace::CommittedPhysicalMemory() const {
  if (!base::OS::HasLazyCommits()) return CommittedMemory();
  BasicMemoryChunk::UpdateHighWaterMark(top_);
  size_t size = 0;
  for (auto* chunk : pages_) {
    size += chunk->size();
  }

  return size;
}

void ReadOnlySpace::FreeLinearAllocationArea() {
  // Mark the old linear allocation area with a free space map so it can be
  // skipped when scanning the heap.
  if (top_ == kNullAddress) {
    DCHECK_EQ(kNullAddress, limit_);
    return;
  }

  // Clear the bits in the unused black area.
  ReadOnlyPage* page = pages_.back();
  heap()->incremental_marking()->marking_state()->bitmap(page)->ClearRange(
      page->AddressToMarkbitIndex(top_), page->AddressToMarkbitIndex(limit_));

  heap()->CreateFillerObjectAt(top_, static_cast<int>(limit_ - top_),
                               ClearRecordedSlots::kNo);

  BasicMemoryChunk::UpdateHighWaterMark(top_);

  top_ = kNullAddress;
  limit_ = kNullAddress;
}

void ReadOnlySpace::EnsureSpaceForAllocation(int size_in_bytes) {
  if (top_ + size_in_bytes <= limit_) {
    return;
  }

  DCHECK_GE(size_in_bytes, 0);

  FreeLinearAllocationArea();

  BasicMemoryChunk* chunk =
      heap()->memory_allocator()->AllocateReadOnlyPage(this);
  capacity_ += AreaSize();

  accounting_stats_.IncreaseCapacity(chunk->area_size());
  AccountCommitted(chunk->size());
  CHECK_NOT_NULL(chunk);
  pages_.push_back(static_cast<ReadOnlyPage*>(chunk));

  heap()->CreateFillerObjectAt(chunk->area_start(),
                               static_cast<int>(chunk->area_size()),
                               ClearRecordedSlots::kNo);

  top_ = chunk->area_start();
  limit_ = chunk->area_end();
  return;
}

HeapObject ReadOnlySpace::TryAllocateLinearlyAligned(
    int size_in_bytes, AllocationAlignment alignment) {
  Address current_top = top_;
  int filler_size = Heap::GetFillToAlign(current_top, alignment);

  Address new_top = current_top + filler_size + size_in_bytes;
  if (new_top > limit_) return HeapObject();

  // Allocation always occurs in the last chunk for RO_SPACE.
  BasicMemoryChunk* chunk = pages_.back();
  int allocated_size = filler_size + size_in_bytes;
  accounting_stats_.IncreaseAllocatedBytes(allocated_size, chunk);
  chunk->IncreaseAllocatedBytes(allocated_size);

  top_ = new_top;
  if (filler_size > 0) {
    return heap()->PrecedeWithFiller(HeapObject::FromAddress(current_top),
                                     filler_size);
  }

  return HeapObject::FromAddress(current_top);
}

AllocationResult ReadOnlySpace::AllocateRawAligned(
    int size_in_bytes, AllocationAlignment alignment) {
  DCHECK(!FLAG_enable_third_party_heap);
  DCHECK(!IsDetached());
  int allocation_size = size_in_bytes;

  HeapObject object = TryAllocateLinearlyAligned(allocation_size, alignment);
  if (object.is_null()) {
    // We don't know exactly how much filler we need to align until space is
    // allocated, so assume the worst case.
    EnsureSpaceForAllocation(allocation_size +
                             Heap::GetMaximumFillToAlign(alignment));
    allocation_size = size_in_bytes;
    object = TryAllocateLinearlyAligned(size_in_bytes, alignment);
    CHECK(!object.is_null());
  }
  MSAN_ALLOCATED_UNINITIALIZED_MEMORY(object.address(), size_in_bytes);

  return AllocationResult::FromObject(object);
}

AllocationResult ReadOnlySpace::AllocateRawUnaligned(int size_in_bytes) {
  DCHECK(!IsDetached());
  EnsureSpaceForAllocation(size_in_bytes);
  Address current_top = top_;
  Address new_top = current_top + size_in_bytes;
  DCHECK_LE(new_top, limit_);
  top_ = new_top;
  HeapObject object = HeapObject::FromAddress(current_top);

  DCHECK(!object.is_null());
  MSAN_ALLOCATED_UNINITIALIZED_MEMORY(object.address(), size_in_bytes);

  // Allocation always occurs in the last chunk for RO_SPACE.
  BasicMemoryChunk* chunk = pages_.back();
  accounting_stats_.IncreaseAllocatedBytes(size_in_bytes, chunk);
  chunk->IncreaseAllocatedBytes(size_in_bytes);

  return AllocationResult::FromObject(object);
}

AllocationResult ReadOnlySpace::AllocateRaw(int size_in_bytes,
                                            AllocationAlignment alignment) {
  AllocationResult result =
      USE_ALLOCATION_ALIGNMENT_BOOL && alignment != kTaggedAligned
          ? AllocateRawAligned(size_in_bytes, alignment)
          : AllocateRawUnaligned(size_in_bytes);
  HeapObject heap_obj;
  if (result.To(&heap_obj)) {
    DCHECK(heap()->incremental_marking()->marking_state()->IsBlack(heap_obj));
  }
  return result;
}

size_t ReadOnlyPage::ShrinkToHighWaterMark() {
  // Shrink pages to high water mark. The water mark points either to a filler
  // or the area_end.
  HeapObject filler = HeapObject::FromAddress(HighWaterMark());
  if (filler.address() == area_end()) return 0;
  CHECK(filler.IsFreeSpaceOrFiller());
  DCHECK_EQ(filler.address() + filler.Size(), area_end());

  size_t unused = RoundDown(static_cast<size_t>(area_end() - filler.address()),
                            MemoryAllocator::GetCommitPageSize());
  if (unused > 0) {
    DCHECK_EQ(0u, unused % MemoryAllocator::GetCommitPageSize());
    if (FLAG_trace_gc_verbose) {
      PrintIsolate(heap()->isolate(), "Shrinking page %p: end %p -> %p\n",
                   reinterpret_cast<void*>(this),
                   reinterpret_cast<void*>(area_end()),
                   reinterpret_cast<void*>(area_end() - unused));
    }
    heap()->CreateFillerObjectAt(
        filler.address(),
        static_cast<int>(area_end() - filler.address() - unused),
        ClearRecordedSlots::kNo);
    heap()->memory_allocator()->PartialFreeMemory(
        this, address() + size() - unused, unused, area_end() - unused);
    if (filler.address() != area_end()) {
      CHECK(filler.IsFreeSpaceOrFiller());
      CHECK_EQ(filler.address() + filler.Size(), area_end());
    }
  }
  return unused;
}

void ReadOnlySpace::ShrinkPages() {
  if (V8_ENABLE_THIRD_PARTY_HEAP_BOOL) return;
  BasicMemoryChunk::UpdateHighWaterMark(top_);
  heap()->CreateFillerObjectAt(top_, static_cast<int>(limit_ - top_),
                               ClearRecordedSlots::kNo);

  for (ReadOnlyPage* chunk : pages_) {
    DCHECK(chunk->IsFlagSet(Page::NEVER_EVACUATE));
    size_t unused = chunk->ShrinkToHighWaterMark();
    capacity_ -= unused;
    accounting_stats_.DecreaseCapacity(static_cast<intptr_t>(unused));
    AccountUncommitted(unused);
  }
  limit_ = pages_.back()->area_end();
}

SharedReadOnlySpace::SharedReadOnlySpace(
    Heap* heap, PointerCompressedReadOnlyArtifacts* artifacts)
    : SharedReadOnlySpace(heap) {
  // This constructor should only be used when RO_SPACE is shared with pointer
  // compression in a per-Isolate cage.
  DCHECK(V8_SHARED_RO_HEAP_BOOL);
  DCHECK(COMPRESS_POINTERS_BOOL);
  DCHECK(COMPRESS_POINTERS_IN_ISOLATE_CAGE_BOOL);
  DCHECK(ReadOnlyHeap::IsReadOnlySpaceShared());
  DCHECK(!artifacts->pages().empty());

  accounting_stats_.IncreaseCapacity(artifacts->accounting_stats().Capacity());
  for (ReadOnlyPage* page : artifacts->pages()) {
    pages_.push_back(page);
    accounting_stats_.IncreaseAllocatedBytes(page->allocated_bytes(), page);
  }
}

SharedReadOnlySpace::SharedReadOnlySpace(
    Heap* heap, std::vector<ReadOnlyPage*>&& new_pages,
    std::vector<std::unique_ptr<::v8::PageAllocator::SharedMemoryMapping>>&&
        mappings,
    AllocationStats&& new_stats)
    : SharedReadOnlySpace(heap) {
  DCHECK(V8_SHARED_RO_HEAP_BOOL);
  DCHECK(COMPRESS_POINTERS_BOOL);
  DCHECK(COMPRESS_POINTERS_IN_ISOLATE_CAGE_BOOL);
  DCHECK(ReadOnlyHeap::IsReadOnlySpaceShared());

  accounting_stats_ = std::move(new_stats);
  pages_ = std::move(new_pages);
  shared_memory_mappings_ = std::move(mappings);
}

SharedReadOnlySpace::SharedReadOnlySpace(Heap* heap,
                                         SingleCopyReadOnlyArtifacts* artifacts)
    : SharedReadOnlySpace(heap) {
  // This constructor should only be used when RO_SPACE is shared without
  // pointer compression in a per-Isolate cage.
  DCHECK(V8_SHARED_RO_HEAP_BOOL);
  DCHECK(!COMPRESS_POINTERS_IN_ISOLATE_CAGE_BOOL);
  accounting_stats_ = artifacts->accounting_stats();
  pages_ = artifacts->pages();
}

}  // namespace internal
}  // namespace v8
