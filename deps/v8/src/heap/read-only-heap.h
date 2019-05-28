// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_READ_ONLY_HEAP_H_
#define V8_HEAP_READ_ONLY_HEAP_H_

#include "src/base/macros.h"
#include "src/heap/heap.h"
#include "src/objects.h"
#include "src/roots.h"

namespace v8 {
namespace internal {

class ReadOnlySpace;
class ReadOnlyDeserializer;

// This class transparently manages read-only space, roots and cache creation
// and destruction.
class ReadOnlyHeap final {
 public:
  static constexpr size_t kEntriesCount =
      static_cast<size_t>(RootIndex::kReadOnlyRootsCount);

  // If necessary create read-only heap and initialize its artifacts (if the
  // deserializer is provided).
  // TODO(goszczycki): Ideally we'd create this without needing a heap.
  static void SetUp(Isolate* isolate, ReadOnlyDeserializer* des);
  // Indicate that all read-only space objects have been created and will not
  // be written to. This is not thread safe, and should really only be used as
  // part of mksnapshot or when read-only heap sharing is disabled.
  void OnCreateHeapObjectsComplete();
  // Indicate that the current isolate no longer requires the read-only heap and
  // it may be safely disposed of.
  void OnHeapTearDown();

  // Returns whether the object resides in the read-only space.
  V8_EXPORT_PRIVATE static bool Contains(HeapObject object);

  std::vector<Object>* read_only_object_cache() {
    return &read_only_object_cache_;
  }
  ReadOnlySpace* read_only_space() const { return read_only_space_; }

 private:
  static ReadOnlyHeap* Init(Isolate* isolate, ReadOnlyDeserializer* des);

  bool deserializing_ = false;
  ReadOnlySpace* read_only_space_ = nullptr;
  std::vector<Object> read_only_object_cache_;

#ifdef V8_SHARED_RO_HEAP
  Address read_only_roots_[kEntriesCount];
#endif

  explicit ReadOnlyHeap(ReadOnlySpace* ro_space) : read_only_space_(ro_space) {}
  DISALLOW_COPY_AND_ASSIGN(ReadOnlyHeap);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_READ_ONLY_HEAP_H_
