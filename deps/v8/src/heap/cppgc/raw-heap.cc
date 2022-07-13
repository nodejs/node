// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/cppgc/raw-heap.h"

#include "src/heap/cppgc/heap-space.h"

namespace cppgc {
namespace internal {

// static
constexpr size_t RawHeap::kNumberOfRegularSpaces;

RawHeap::RawHeap(
    HeapBase* heap,
    const std::vector<std::unique_ptr<CustomSpaceBase>>& custom_spaces)
    : main_heap_(heap) {
  size_t i = 0;
  for (; i < static_cast<size_t>(RegularSpaceType::kLarge); ++i) {
    spaces_.push_back(std::make_unique<NormalPageSpace>(this, i, false));
  }
  spaces_.push_back(std::make_unique<LargePageSpace>(
      this, static_cast<size_t>(RegularSpaceType::kLarge)));
  DCHECK_EQ(kNumberOfRegularSpaces, spaces_.size());
  for (size_t j = 0; j < custom_spaces.size(); j++) {
    spaces_.push_back(std::make_unique<NormalPageSpace>(
        this, kNumberOfRegularSpaces + j, custom_spaces[j]->IsCompactable()));
  }
}

RawHeap::~RawHeap() = default;

}  // namespace internal
}  // namespace cppgc
