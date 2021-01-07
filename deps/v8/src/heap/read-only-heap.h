// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_READ_ONLY_HEAP_H_
#define V8_HEAP_READ_ONLY_HEAP_H_

#include <memory>
#include <utility>
#include <vector>

#include "src/base/macros.h"
#include "src/base/optional.h"
#include "src/objects/heap-object.h"
#include "src/objects/objects.h"
#include "src/roots/roots.h"

namespace v8 {

class SharedMemoryStatistics;

namespace internal {

class BasicMemoryChunk;
class Isolate;
class Page;
class ReadOnlyArtifacts;
class ReadOnlyPage;
class ReadOnlySpace;
class SharedReadOnlySpace;
class SnapshotData;

// This class transparently manages read-only space, roots and cache creation
// and destruction.
class ReadOnlyHeap {
 public:
  static constexpr size_t kEntriesCount =
      static_cast<size_t>(RootIndex::kReadOnlyRootsCount);

  virtual ~ReadOnlyHeap() = default;

  ReadOnlyHeap(const ReadOnlyHeap&) = delete;
  ReadOnlyHeap& operator=(const ReadOnlyHeap&) = delete;

  // If necessary creates read-only heap and initializes its artifacts (if the
  // deserializer is provided). Then attaches the read-only heap to the isolate.
  // If the deserializer is not provided, then the read-only heap will be only
  // finish initializing when initial heap object creation in the Isolate is
  // completed, which is signalled by calling OnCreateHeapObjectsComplete. When
  // V8_SHARED_RO_HEAP is enabled, a lock will be held until that method is
  // called.
  // TODO(v8:7464): Ideally we'd create this without needing a heap.
  static void SetUp(Isolate* isolate, SnapshotData* read_only_snapshot_data,
                    bool can_rehash);
  // Indicates that the isolate has been set up and all read-only space objects
  // have been created and will not be written to. This should only be called if
  // a deserializer was not previously provided to Setup. When V8_SHARED_RO_HEAP
  // is enabled, this releases the ReadOnlyHeap creation lock.
  void OnCreateHeapObjectsComplete(Isolate* isolate);
  // Indicates that the current isolate no longer requires the read-only heap
  // and it may be safely disposed of.
  virtual void OnHeapTearDown(Heap* heap);
  // If the read-only heap is shared, then populate |statistics| with its stats,
  // otherwise the read-only heap stats are set to 0.
  static void PopulateReadOnlySpaceStatistics(
      SharedMemoryStatistics* statistics);

  // Returns whether the address is within the read-only space.
  V8_EXPORT_PRIVATE static bool Contains(Address address);
  // Returns whether the object resides in the read-only space.
  V8_EXPORT_PRIVATE static bool Contains(HeapObject object);
  // Gets read-only roots from an appropriate root list: shared read-only root
  // list if the shared read-only heap has been initialized or the isolate
  // specific roots table.
  V8_EXPORT_PRIVATE inline static ReadOnlyRoots GetReadOnlyRoots(
      HeapObject object);

  // Extends the read-only object cache with new zero smi and returns a
  // reference to it.
  Object* ExtendReadOnlyObjectCache();
  // Returns a read-only cache entry at a particular index.
  Object cached_read_only_object(size_t i) const;
  bool read_only_object_cache_is_initialized() const;

  ReadOnlySpace* read_only_space() const { return read_only_space_; }

  // Returns whether the ReadOnlySpace will actually be shared taking into
  // account whether shared memory is available with pointer compression.
  static bool IsReadOnlySpaceShared() {
    return V8_SHARED_RO_HEAP_BOOL &&
           (!COMPRESS_POINTERS_BOOL || IsSharedMemoryAvailable());
  }

  virtual void InitializeIsolateRoots(Isolate* isolate) {}
  virtual void InitializeFromIsolateRoots(Isolate* isolate) {}
  virtual bool IsOwnedByIsolate() { return true; }

 protected:
  friend class ReadOnlyArtifacts;
  friend class PointerCompressedReadOnlyArtifacts;

  // Creates a new read-only heap and attaches it to the provided isolate. Only
  // used the first time when creating a ReadOnlyHeap for sharing.
  static ReadOnlyHeap* CreateInitalHeapForBootstrapping(
      Isolate* isolate, std::shared_ptr<ReadOnlyArtifacts> artifacts);
  // Runs the read-only deserializer and calls InitFromIsolate to complete
  // read-only heap initialization.
  void DeseralizeIntoIsolate(Isolate* isolate,
                             SnapshotData* read_only_snapshot_data,
                             bool can_rehash);
  // Initializes read-only heap from an already set-up isolate, copying
  // read-only roots from the isolate. This then seals the space off from
  // further writes, marks it as read-only and detaches it from the heap
  // (unless sharing is disabled).
  void InitFromIsolate(Isolate* isolate);

  bool init_complete_ = false;
  ReadOnlySpace* read_only_space_ = nullptr;
  std::vector<Object> read_only_object_cache_;

  // Returns whether shared memory can be allocated and then remapped to
  // additional addresses.
  static bool IsSharedMemoryAvailable();

  explicit ReadOnlyHeap(ReadOnlySpace* ro_space) : read_only_space_(ro_space) {}
  ReadOnlyHeap(ReadOnlyHeap* ro_heap, ReadOnlySpace* ro_space);
};

// This is used without pointer compression when there is just a single
// ReadOnlyHeap object shared between all Isolates.
class SoleReadOnlyHeap : public ReadOnlyHeap {
 public:
  void InitializeIsolateRoots(Isolate* isolate) override;
  void InitializeFromIsolateRoots(Isolate* isolate) override;
  void OnHeapTearDown(Heap* heap) override;
  bool IsOwnedByIsolate() override { return false; }

 private:
  friend class ReadOnlyHeap;

  explicit SoleReadOnlyHeap(ReadOnlySpace* ro_space) : ReadOnlyHeap(ro_space) {}
  Address read_only_roots_[kEntriesCount];
  V8_EXPORT_PRIVATE static SoleReadOnlyHeap* shared_ro_heap_;
};

// This class enables iterating over all read-only heap objects.
class V8_EXPORT_PRIVATE ReadOnlyHeapObjectIterator {
 public:
  explicit ReadOnlyHeapObjectIterator(ReadOnlyHeap* ro_heap);
  explicit ReadOnlyHeapObjectIterator(ReadOnlySpace* ro_space);

  HeapObject Next();

 private:
  ReadOnlySpace* const ro_space_;
  std::vector<ReadOnlyPage*>::const_iterator current_page_;
  Address current_addr_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_READ_ONLY_HEAP_H_
