// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_READ_ONLY_HEAP_H_
#define V8_HEAP_READ_ONLY_HEAP_H_

#include <utility>

#include "src/base/macros.h"
#include "src/base/optional.h"
#include "src/objects/heap-object.h"
#include "src/objects/objects.h"
#include "src/roots/roots.h"

namespace v8 {
namespace internal {

class Isolate;
class Page;
class ReadOnlyDeserializer;
class ReadOnlySpace;

// This class transparently manages read-only space, roots and cache creation
// and destruction.
class ReadOnlyHeap final {
 public:
  static constexpr size_t kEntriesCount =
      static_cast<size_t>(RootIndex::kReadOnlyRootsCount);

  // If necessary creates read-only heap and initializes its artifacts (if
  // the deserializer is provided). Then attaches the read-only heap to the
  // isolate.
  // TODO(v8:7464): Ideally we'd create this without needing a heap.
  static void SetUp(Isolate* isolate, ReadOnlyDeserializer* des);
  // Indicates that the isolate has been set up and all read-only space objects
  // have been created and will not be written to. This is not thread safe, and
  // should really only be used during snapshot creation or when read-only heap
  // sharing is disabled.
  void OnCreateHeapObjectsComplete(Isolate* isolate);
  // Indicates that the current isolate no longer requires the read-only heap
  // and it may be safely disposed of.
  void OnHeapTearDown();

  // Returns whether the object resides in the read-only space.
  V8_EXPORT_PRIVATE static bool Contains(HeapObject object);
  // Gets read-only roots from an appropriate root list: shared read-only root
  // list if the shared read-only heap has been initialized or the isolate
  // specific roots table.
  V8_EXPORT_PRIVATE inline static ReadOnlyRoots GetReadOnlyRoots(
      HeapObject object);

  // Clears any shared read-only heap artifacts for testing, forcing read-only
  // heap to be re-created on next set up.
  V8_EXPORT_PRIVATE static void ClearSharedHeapForTest();

  // Extends the read-only object cache with new zero smi and returns a
  // reference to it.
  Object* ExtendReadOnlyObjectCache();
  // Returns a read-only cache entry at a particular index.
  Object cached_read_only_object(size_t i) const;
  bool read_only_object_cache_is_initialized() const;

  ReadOnlySpace* read_only_space() const { return read_only_space_; }

 private:
  using Checksum = std::pair<uint32_t, uint32_t>;

  // Creates a new read-only heap and attaches it to the provided isolate.
  static ReadOnlyHeap* CreateAndAttachToIsolate(Isolate* isolate);
  // Runs the read-only deserailizer and calls InitFromIsolate to complete
  // read-only heap initialization.
  void DeseralizeIntoIsolate(Isolate* isolate, ReadOnlyDeserializer* des);
  // Initializes read-only heap from an already set-up isolate, copying
  // read-only roots from the isolate. This then seals the space off from
  // further writes, marks it as read-only and detaches it from the heap (unless
  // sharing is disabled).
  void InitFromIsolate(Isolate* isolate);

  bool init_complete_ = false;
  ReadOnlySpace* read_only_space_ = nullptr;
  std::vector<Object> read_only_object_cache_;

#ifdef V8_SHARED_RO_HEAP
#ifdef DEBUG
  // The checksum of the blob the read-only heap was deserialized from, if any.
  base::Optional<Checksum> read_only_blob_checksum_;
#endif  // DEBUG

  Address read_only_roots_[kEntriesCount];

  V8_EXPORT_PRIVATE static ReadOnlyHeap* shared_ro_heap_;
#endif  // V8_SHARED_RO_HEAP

  explicit ReadOnlyHeap(ReadOnlySpace* ro_space) : read_only_space_(ro_space) {}
  DISALLOW_COPY_AND_ASSIGN(ReadOnlyHeap);
};

// This class enables iterating over all read-only heap objects.
class V8_EXPORT_PRIVATE ReadOnlyHeapObjectIterator {
 public:
  explicit ReadOnlyHeapObjectIterator(ReadOnlyHeap* ro_heap);
  explicit ReadOnlyHeapObjectIterator(ReadOnlySpace* ro_space);

  HeapObject Next();

 private:
  ReadOnlySpace* const ro_space_;
  Page* current_page_;
  Address current_addr_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_READ_ONLY_HEAP_H_
