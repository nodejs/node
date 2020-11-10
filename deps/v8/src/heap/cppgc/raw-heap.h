// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_CPPGC_RAW_HEAP_H_
#define V8_HEAP_CPPGC_RAW_HEAP_H_

#include <iterator>
#include <memory>
#include <vector>

#include "include/cppgc/heap.h"
#include "src/base/logging.h"
#include "src/base/macros.h"

namespace cppgc {
namespace internal {

class Heap;
class BaseSpace;

// RawHeap is responsible for space management.
class V8_EXPORT_PRIVATE RawHeap final {
 public:
  // Normal spaces are used to store objects of different size classes:
  // - kNormal1:  < 32 bytes
  // - kNormal2:  < 64 bytes
  // - kNormal3:  < 128 bytes
  // - kNormal4: >= 128 bytes
  //
  // Objects of size greater than 2^16 get stored in the large space.
  //
  // Users can override where objects are allocated via cppgc::CustomSpace to
  // force allocation in a custom space.
  enum class RegularSpaceType : uint8_t {
    kNormal1,
    kNormal2,
    kNormal3,
    kNormal4,
    kLarge,
  };

  static constexpr size_t kNumberOfRegularSpaces =
      static_cast<size_t>(RegularSpaceType::kLarge) + 1;

  using Spaces = std::vector<std::unique_ptr<BaseSpace>>;
  using iterator = Spaces::iterator;
  using const_iterator = Spaces::const_iterator;

  explicit RawHeap(Heap* heap, size_t custom_spaces);
  ~RawHeap();

  // Space iteration support.
  iterator begin() { return spaces_.begin(); }
  const_iterator begin() const { return spaces_.begin(); }
  iterator end() { return spaces_.end(); }
  const_iterator end() const { return spaces_.end(); }

  iterator custom_begin() { return std::next(begin(), kNumberOfRegularSpaces); }
  iterator custom_end() { return end(); }

  size_t size() const { return spaces_.size(); }

  BaseSpace* Space(RegularSpaceType type) {
    const size_t index = static_cast<size_t>(type);
    DCHECK_GT(kNumberOfRegularSpaces, index);
    return Space(index);
  }
  const BaseSpace* Space(RegularSpaceType space) const {
    return const_cast<RawHeap&>(*this).Space(space);
  }

  BaseSpace* CustomSpace(CustomSpaceIndex space_index) {
    return Space(SpaceIndexForCustomSpace(space_index));
  }
  const BaseSpace* CustomSpace(CustomSpaceIndex space_index) const {
    return const_cast<RawHeap&>(*this).CustomSpace(space_index);
  }

  Heap* heap() { return main_heap_; }
  const Heap* heap() const { return main_heap_; }

 private:
  size_t SpaceIndexForCustomSpace(CustomSpaceIndex space_index) const {
    DCHECK_LT(space_index.value, spaces_.size() - kNumberOfRegularSpaces);
    return kNumberOfRegularSpaces + space_index.value;
  }

  BaseSpace* Space(size_t space_index) {
    DCHECK_GT(spaces_.size(), space_index);
    BaseSpace* space = spaces_[space_index].get();
    DCHECK(space);
    return space;
  }
  const BaseSpace* Space(size_t space_index) const {
    return const_cast<RawHeap&>(*this).Space(space_index);
  }

  Heap* main_heap_;
  Spaces spaces_;
};

}  // namespace internal
}  // namespace cppgc

#endif  // V8_HEAP_CPPGC_RAW_HEAP_H_
