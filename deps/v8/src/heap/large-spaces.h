// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_LARGE_SPACES_H_
#define V8_HEAP_LARGE_SPACES_H_

#include <atomic>
#include <functional>
#include <memory>
#include <unordered_map>

#include "src/base/macros.h"
#include "src/base/platform/mutex.h"
#include "src/common/globals.h"
#include "src/heap/heap.h"
#include "src/heap/memory-chunk.h"
#include "src/heap/spaces.h"
#include "src/objects/heap-object.h"

namespace v8 {
namespace internal {

class Isolate;
class LocalHeap;

class LargePage : public MemoryChunk {
 public:
  // A limit to guarantee that we do not overflow typed slot offset in the old
  // to old remembered set. Note that this limit is higher than what assembler
  // already imposes on x64 and ia32 architectures.
  static const int kMaxCodePageSize = 512 * MB;

  static LargePage* FromHeapObject(HeapObject o) {
    return static_cast<LargePage*>(MemoryChunk::FromHeapObject(o));
  }

  HeapObject GetObject() { return HeapObject::FromAddress(area_start()); }

  LargePage* next_page() { return static_cast<LargePage*>(list_node_.next()); }

  // Uncommit memory that is not in use anymore by the object. If the object
  // cannot be shrunk 0 is returned.
  Address GetAddressToShrink(Address object_address, size_t object_size);

  void ClearOutOfLiveRangeSlots(Address free_start);

 private:
  static LargePage* Initialize(Heap* heap, MemoryChunk* chunk,
                               Executability executable);

  friend class MemoryAllocator;
};

STATIC_ASSERT(sizeof(LargePage) <= MemoryChunk::kHeaderSize);

// -----------------------------------------------------------------------------
// Large objects ( > kMaxRegularHeapObjectSize ) are allocated and managed by
// the large object space. Large objects do not move during garbage collections.

class V8_EXPORT_PRIVATE LargeObjectSpace : public Space {
 public:
  using iterator = LargePageIterator;

  ~LargeObjectSpace() override { TearDown(); }

  // Releases internal resources, frees objects in this space.
  void TearDown();

  // Available bytes for objects in this space.
  size_t Available() override;

  size_t Size() override { return size_; }
  size_t SizeOfObjects() override { return objects_size_; }

  // Approximate amount of physical memory committed for this space.
  size_t CommittedPhysicalMemory() override;

  int PageCount() { return page_count_; }

  // Frees unmarked objects.
  virtual void FreeUnmarkedObjects();

  // Checks whether a heap object is in this space; O(1).
  bool Contains(HeapObject obj);
  // Checks whether an address is in the object area in this space. Iterates all
  // objects in the space. May be slow.
  bool ContainsSlow(Address addr);

  // Checks whether the space is empty.
  bool IsEmpty() { return first_page() == nullptr; }

  virtual void AddPage(LargePage* page, size_t object_size);
  virtual void RemovePage(LargePage* page, size_t object_size);

  LargePage* first_page() {
    return reinterpret_cast<LargePage*>(Space::first_page());
  }

  iterator begin() { return iterator(first_page()); }
  iterator end() { return iterator(nullptr); }

  std::unique_ptr<ObjectIterator> GetObjectIterator(Heap* heap) override;

  virtual bool is_off_thread() const { return false; }

#ifdef VERIFY_HEAP
  virtual void Verify(Isolate* isolate);
#endif

#ifdef DEBUG
  void Print() override;
#endif

  // The last allocated object that is not guaranteed to be initialized when the
  // concurrent marker visits it.
  Address pending_object() {
    return pending_object_.load(std::memory_order_acquire);
  }

  void ResetPendingObject() {
    pending_object_.store(0, std::memory_order_release);
  }

 protected:
  LargeObjectSpace(Heap* heap, AllocationSpace id);

  void AdvanceAndInvokeAllocationObservers(Address soon_object, size_t size);

  LargePage* AllocateLargePage(int object_size, Executability executable);

  std::atomic<size_t> size_;  // allocated bytes
  int page_count_;       // number of chunks
  std::atomic<size_t> objects_size_;  // size of objects
  base::Mutex allocation_mutex_;
  std::atomic<Address> pending_object_;

 private:
  friend class LargeObjectSpaceObjectIterator;
};

class OldLargeObjectSpace : public LargeObjectSpace {
 public:
  explicit OldLargeObjectSpace(Heap* heap);

  V8_EXPORT_PRIVATE V8_WARN_UNUSED_RESULT AllocationResult
  AllocateRaw(int object_size);

  V8_EXPORT_PRIVATE V8_WARN_UNUSED_RESULT AllocationResult
  AllocateRawBackground(LocalHeap* local_heap, int object_size);

  // Clears the marking state of live objects.
  void ClearMarkingStateOfLiveObjects();

  void PromoteNewLargeObject(LargePage* page);

 protected:
  explicit OldLargeObjectSpace(Heap* heap, AllocationSpace id);
  V8_WARN_UNUSED_RESULT AllocationResult AllocateRaw(int object_size,
                                                     Executability executable);
};

class NewLargeObjectSpace : public LargeObjectSpace {
 public:
  NewLargeObjectSpace(Heap* heap, size_t capacity);

  V8_EXPORT_PRIVATE V8_WARN_UNUSED_RESULT AllocationResult
  AllocateRaw(int object_size);

  // Available bytes for objects in this space.
  size_t Available() override;

  void Flip();

  void FreeDeadObjects(const std::function<bool(HeapObject)>& is_dead);

  void SetCapacity(size_t capacity);

 private:
  size_t capacity_;
};

class CodeLargeObjectSpace : public OldLargeObjectSpace {
 public:
  explicit CodeLargeObjectSpace(Heap* heap);

  V8_EXPORT_PRIVATE V8_WARN_UNUSED_RESULT AllocationResult
  AllocateRaw(int object_size);

  // Finds a large object page containing the given address, returns nullptr if
  // such a page doesn't exist.
  LargePage* FindPage(Address a);

 protected:
  void AddPage(LargePage* page, size_t object_size) override;
  void RemovePage(LargePage* page, size_t object_size) override;

 private:
  static const size_t kInitialChunkMapCapacity = 1024;
  void InsertChunkMapEntries(LargePage* page);
  void RemoveChunkMapEntries(LargePage* page);

  // Page-aligned addresses to their corresponding LargePage.
  std::unordered_map<Address, LargePage*> chunk_map_;
};

class LargeObjectSpaceObjectIterator : public ObjectIterator {
 public:
  explicit LargeObjectSpaceObjectIterator(LargeObjectSpace* space);

  HeapObject Next() override;

 private:
  LargePage* current_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_LARGE_SPACES_H_
