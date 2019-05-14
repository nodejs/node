// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_READ_ONLY_HEAP_H_
#define V8_HEAP_READ_ONLY_HEAP_H_

#include "src/base/macros.h"
#include "src/heap/heap.h"
#include "src/roots.h"
#include "src/snapshot/read-only-deserializer.h"

namespace v8 {
namespace internal {

class ReadOnlySpace;

// This class transparently manages read-only space, roots and cache creation
// and destruction. Eventually this will allow sharing these artifacts between
// isolates.
class ReadOnlyHeap {
 public:
  static ReadOnlyHeap* GetOrCreateReadOnlyHeap(Heap* heap);
  // If necessary, deserialize read-only objects and set up read-only object
  // cache.
  void MaybeDeserialize(Isolate* isolate, ReadOnlyDeserializer* des);
  // Frees ReadOnlySpace and itself when sharing is disabled. No-op otherwise.
  // Read-only data should not be used within the current isolate after this is
  // called.
  void OnHeapTearDown();

  std::vector<Object>* read_only_object_cache() {
    return &read_only_object_cache_;
  }
  ReadOnlySpace* read_only_space() const { return read_only_space_; }

 private:
  ReadOnlySpace* read_only_space_ = nullptr;
  std::vector<Object> read_only_object_cache_;

  explicit ReadOnlyHeap(ReadOnlySpace* ro_space) : read_only_space_(ro_space) {}
  DISALLOW_COPY_AND_ASSIGN(ReadOnlyHeap);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_READ_ONLY_HEAP_H_
