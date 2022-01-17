// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/index-generator.h"

namespace v8 {
namespace internal {

IndexGenerator::IndexGenerator(size_t size) : size_(size) {
  if (size == 0) return;
  base::MutexGuard guard(&lock_);
  pending_indices_.push(0);
  ranges_to_split_.push({0, size_});
}

base::Optional<size_t> IndexGenerator::GetNext() {
  base::MutexGuard guard(&lock_);
  if (!pending_indices_.empty()) {
    // Return any pending index first.
    auto index = pending_indices_.top();
    pending_indices_.pop();
    return index;
  }
  if (ranges_to_split_.empty()) return base::nullopt;

  // Split the oldest running range in 2 and return the middle index as
  // starting point.
  auto range = ranges_to_split_.front();
  ranges_to_split_.pop();
  size_t size = range.second - range.first;
  size_t mid = range.first + size / 2;
  // Both sides of the range are added to |ranges_to_split_| so they may be
  // further split if possible.
  if (mid - range.first > 1) ranges_to_split_.push({range.first, mid});
  if (range.second - mid > 1) ranges_to_split_.push({mid, range.second});
  return mid;
}

void IndexGenerator::GiveBack(size_t index) {
  base::MutexGuard guard(&lock_);
  // Add |index| to pending indices so GetNext() may return it before anything
  // else.
  pending_indices_.push(index);
}

}  // namespace internal
}  // namespace v8
