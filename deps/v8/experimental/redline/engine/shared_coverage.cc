// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "experimental/redline/engine/shared_coverage.h"

#include <cstring>

#include "experimental/redline/base/log.h"

namespace redline {

bool SharedCoverage::Initialize(uint32_t num_edges, uint32_t bitmap_size,
                                std::string* error) {
  std::scoped_lock<std::mutex> lock(flaky_mutex_);
  if (words_ != 0) {
    if (num_edges != num_edges_ || bitmap_size != words_ * 8) {
      *error = StringPrintf(
          "target reports %u edges but the session was started with %u; "
          "are all workers running the same d8?",
          num_edges, num_edges_);
      return false;
    }
    return true;
  }
  if (num_edges == 0 || bitmap_size == 0 || bitmap_size % 8 != 0) {
    *error = "invalid coverage bitmap geometry";
    return false;
  }
  num_edges_ = num_edges;
  uint32_t words = bitmap_size / 8;
  virgin_ = std::make_unique<std::atomic<uint64_t>[]>(words);
  crash_virgin_ = std::make_unique<std::atomic<uint64_t>[]>(words);
  for (uint32_t i = 0; i < words; i++) {
    virgin_[i].store(~uint64_t{0}, std::memory_order_relaxed);
    crash_virgin_[i].store(~uint64_t{0}, std::memory_order_relaxed);
  }
  // Edge 0 is never reported; bits beyond num_edges never get set.
  virgin_[0].fetch_and(~uint64_t{1}, std::memory_order_relaxed);
  crash_virgin_[0].fetch_and(~uint64_t{1}, std::memory_order_relaxed);
  uint32_t tail_bits = (num_edges + 1) % 64;
  if (tail_bits != 0) {
    uint64_t tail_mask = (uint64_t{1} << tail_bits) - 1;
    virgin_[words - 1].fetch_and(tail_mask, std::memory_order_relaxed);
    crash_virgin_[words - 1].fetch_and(tail_mask, std::memory_order_relaxed);
  }
  words_ = words;  // publishes the state (last)
  return true;
}

double SharedCoverage::coverage_fraction() const {
  if (num_edges_ == 0) {
    return 0.0;
  }
  // Suppressed flaky edges were claimed but never reached deterministically;
  // they are not coverage.
  uint64_t found = found_edges();
  uint64_t suppressed = suppressed_flaky_edges();
  found = found > suppressed ? found - suppressed : 0;
  return static_cast<double>(found) / static_cast<double>(num_edges_ + 1);
}

std::vector<uint32_t> SharedCoverage::ClaimNewEdges(const uint8_t* bitmap) {
  std::vector<uint32_t> claimed;
  uint32_t count = 0;
  for (uint32_t w = 0; w < words_; w++) {
    uint64_t current;
    memcpy(&current, bitmap + static_cast<size_t>(w) * 8, 8);
    if (current == 0) {
      continue;
    }
    uint64_t fresh = current & virgin_[w].load(std::memory_order_relaxed);
    if (fresh == 0) {
      continue;
    }
    // Clear the bits; whatever was still set in the old value is ours.
    uint64_t old = virgin_[w].fetch_and(~fresh, std::memory_order_acq_rel);
    uint64_t mine = old & fresh;
    while (mine != 0) {
      uint32_t bit = static_cast<uint32_t>(__builtin_ctzll(mine));
      claimed.push_back(w * 64 + bit);
      mine &= mine - 1;
      count++;
    }
  }
  if (count != 0) {
    found_edges_.fetch_add(count, std::memory_order_relaxed);
  }
  return claimed;
}

bool SharedCoverage::HasNewEdges(const uint8_t* bitmap) const {
  for (uint32_t w = 0; w < words_; w++) {
    uint64_t current;
    memcpy(&current, bitmap + static_cast<size_t>(w) * 8, 8);
    if (current == 0) {
      continue;
    }
    if ((current & virgin_[w].load(std::memory_order_relaxed)) != 0) {
      return true;
    }
  }
  return false;
}

void SharedCoverage::Release(const std::vector<uint32_t>& edges) {
  uint32_t count = 0;
  for (uint32_t edge : edges) {
    if (edge == 0 || edge > num_edges_) {
      continue;
    }
    uint64_t bit = uint64_t{1} << (edge % 64);
    uint64_t old = virgin_[edge / 64].fetch_or(bit, std::memory_order_acq_rel);
    if ((old & bit) == 0) {
      count++;
    }
  }
  if (count != 0) {
    found_edges_.fetch_sub(count, std::memory_order_relaxed);
  }
}

void SharedCoverage::MarkSeen(const std::vector<uint32_t>& edges) {
  uint32_t count = 0;
  for (uint32_t edge : edges) {
    if (edge == 0 || edge > num_edges_) {
      continue;
    }
    uint64_t bit = uint64_t{1} << (edge % 64);
    uint64_t old =
        virgin_[edge / 64].fetch_and(~bit, std::memory_order_acq_rel);
    if ((old & bit) != 0) {
      count++;
    }
  }
  if (count != 0) {
    found_edges_.fetch_add(count, std::memory_order_relaxed);
  }
}

bool SharedCoverage::IsSeen(uint32_t edge) const {
  if (edge == 0 || edge > num_edges_) {
    return true;
  }
  uint64_t bit = uint64_t{1} << (edge % 64);
  return (virgin_[edge / 64].load(std::memory_order_relaxed) & bit) == 0;
}

bool SharedCoverage::ClaimNewCrashEdges(const uint8_t* bitmap) {
  bool any = false;
  for (uint32_t w = 0; w < words_; w++) {
    uint64_t current;
    memcpy(&current, bitmap + static_cast<size_t>(w) * 8, 8);
    if (current == 0) {
      continue;
    }
    uint64_t fresh = current & crash_virgin_[w].load(std::memory_order_relaxed);
    if (fresh == 0) {
      continue;
    }
    uint64_t old =
        crash_virgin_[w].fetch_and(~fresh, std::memory_order_acq_rel);
    if ((old & fresh) != 0) {
      any = true;
    }
  }
  return any;
}

bool SharedCoverage::RecordFlaky(uint32_t edge, int threshold) {
  std::scoped_lock<std::mutex> lock(flaky_mutex_);
  int count = ++flaky_counts_[edge];
  if (count == threshold) {
    suppressed_.fetch_add(1, std::memory_order_relaxed);
  }
  return count >= threshold;
}

}  // namespace redline
