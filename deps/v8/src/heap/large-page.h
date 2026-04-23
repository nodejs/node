// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_LARGE_PAGE_H_
#define V8_HEAP_LARGE_PAGE_H_

#include "src/heap/mutable-page.h"

namespace v8 {
namespace internal {

class LargePage final : public MutablePage {
 public:
  // A limit to guarantee that we do not overflow typed slot offset in the old
  // to old remembered set. Note that this limit is higher than what assembler
  // already imposes on x64 and ia32 architectures.
  static constexpr int kMaxCodePageSize = 512 * MB;

  V8_INLINE static LargePage* FromHeapObject(Isolate* i, Tagged<HeapObject> o);

  LargePage(Heap* heap, BaseSpace* space, size_t chunk_size, Address area_start,
            Address area_end, VirtualMemory reservation,
            Executability executable,
            MemoryChunk::MainThreadFlags* trusted_flags);

  Tagged<HeapObject> GetObject() const {
    return HeapObject::FromAddress(area_start());
  }

  LargePage* next_page() { return SbxCast<LargePage>(list_node_.next()); }
  const LargePage* next_page() const {
    return static_cast<const LargePage*>(list_node_.next());
  }

  void ClearOutOfLiveRangeSlots(Address free_start);

 private:
  friend class MemoryAllocator;
};

template <>
struct CastTraits<LargePage> {
  static inline bool AllowFrom(const BasePage& page) { return page.is_large(); }
};

}  // namespace internal

namespace base {
// Define special hash function for page pointers, to be used with std data
// structures, e.g. std::unordered_set<LargePage*, base::hash<LargePage*>>
template <>
struct hash<i::LargePage*> : hash<i::BasePage*> {};
template <>
struct hash<const i::LargePage*> : hash<const i::BasePage*> {};
}  // namespace base

}  // namespace v8

#endif  // V8_HEAP_LARGE_PAGE_H_
