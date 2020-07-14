// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HANDLES_PERSISTENT_HANDLES_H_
#define V8_HANDLES_PERSISTENT_HANDLES_H_

#include <vector>

#include "include/v8-internal.h"
#include "src/api/api.h"
#include "src/base/macros.h"
#include "src/objects/visitors.h"

namespace v8 {
namespace internal {

class Heap;

// PersistentHandles serves as a container for handles that can be passed back
// and forth between threads. Allocation and deallocation of this class is
// thread-safe and the isolate tracks all PersistentHandles containers.
class PersistentHandles {
 public:
  V8_EXPORT_PRIVATE explicit PersistentHandles(
      Isolate* isolate, size_t block_size = kHandleBlockSize);
  V8_EXPORT_PRIVATE ~PersistentHandles();

  PersistentHandles(const PersistentHandles&) = delete;
  PersistentHandles& operator=(const PersistentHandles&) = delete;

  void Iterate(RootVisitor* visitor);

  V8_EXPORT_PRIVATE Handle<Object> NewHandle(Address value);

 private:
  void AddBlock();
  Address* GetHandle(Address value);

#ifdef DEBUG
  void Attach(LocalHeap* local_heap);
  void Detach();

  LocalHeap* owner_ = nullptr;

#else
  void Attach(LocalHeap*) {}
  void Detach() {}
#endif

  Isolate* isolate_;
  std::vector<Address*> blocks_;
  size_t block_size_;

  Address* block_next_;
  Address* block_limit_;

  PersistentHandles* prev_;
  PersistentHandles* next_;

  friend class PersistentHandlesList;
  friend class LocalHeap;
};

class PersistentHandlesList {
 public:
  explicit PersistentHandlesList(Isolate* isolate)
      : isolate_(isolate), persistent_handles_head_(nullptr) {}

  // Iteration is only safe during a safepoint
  void Iterate(RootVisitor* visitor);

 private:
  void Add(PersistentHandles* persistent_handles);
  void Remove(PersistentHandles* persistent_handles);

  Isolate* isolate_;

  base::Mutex persistent_handles_mutex_;
  PersistentHandles* persistent_handles_head_ = nullptr;

  friend class PersistentHandles;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_HANDLES_PERSISTENT_HANDLES_H_
