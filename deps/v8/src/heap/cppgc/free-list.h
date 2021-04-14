// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_CPPGC_FREE_LIST_H_
#define V8_HEAP_CPPGC_FREE_LIST_H_

#include <array>

#include "include/cppgc/heap-statistics.h"
#include "src/base/macros.h"
#include "src/heap/cppgc/globals.h"
#include "src/heap/cppgc/heap-object-header.h"

namespace cppgc {
namespace internal {

class V8_EXPORT_PRIVATE FreeList {
 public:
  struct Block {
    void* address;
    size_t size;
  };

  FreeList();

  FreeList(const FreeList&) = delete;
  FreeList& operator=(const FreeList&) = delete;

  FreeList(FreeList&& freelist) V8_NOEXCEPT;
  FreeList& operator=(FreeList&& freelist) V8_NOEXCEPT;

  // Allocates entries which are at least of the provided size.
  Block Allocate(size_t);

  // Adds block to the freelist. The minimal block size is two words.
  void Add(Block);

  // Append other freelist into this.
  void Append(FreeList&&);

  void Clear();

  size_t Size() const;
  bool IsEmpty() const;

  void CollectStatistics(HeapStatistics::FreeListStatistics&);

  bool ContainsForTesting(Block) const;

 private:
  class Entry;

  bool IsConsistent(size_t) const;

  // All |Entry|s in the nth list have size >= 2^n.
  std::array<Entry*, kPageSizeLog2> free_list_heads_;
  std::array<Entry*, kPageSizeLog2> free_list_tails_;
  size_t biggest_free_list_index_ = 0;
};

}  // namespace internal
}  // namespace cppgc

#endif  // V8_HEAP_CPPGC_FREE_LIST_H_
