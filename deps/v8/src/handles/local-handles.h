// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HANDLES_LOCAL_HANDLES_H_
#define V8_HANDLES_LOCAL_HANDLES_H_

#include "include/v8-internal.h"
#include "src/base/functional.h"
#include "src/base/macros.h"
#include "src/handles/handles.h"
#include "src/heap/local-heap.h"

namespace v8 {
namespace internal {

class RootVisitor;

class LocalHandles {
 public:
  LocalHandles();

  void Iterate(RootVisitor* visitor);

 private:
  HandleScopeData scope_;
  std::vector<Address*> blocks_;

  V8_EXPORT_PRIVATE Address* AddBlock();
  V8_EXPORT_PRIVATE void RemoveBlocks();

  friend class LocalHandleScope;
};

class LocalHandleScope {
 public:
  explicit inline LocalHandleScope(LocalHeap* local_heap);
  inline ~LocalHandleScope();

  V8_INLINE static Address* GetHandle(LocalHeap* local_heap, Address value);

 private:
  // Prevent heap allocation or illegal handle scopes.
  void* operator new(size_t size);
  void operator delete(void* size_t);

  LocalHeap* local_heap_;
  Address* prev_limit_;
  Address* prev_next_;

  DISALLOW_COPY_AND_ASSIGN(LocalHandleScope);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_HANDLES_LOCAL_HANDLES_H_
