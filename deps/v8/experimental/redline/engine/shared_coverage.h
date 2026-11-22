// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXPERIMENTAL_REDLINE_ENGINE_SHARED_COVERAGE_H_
#define V8_EXPERIMENTAL_REDLINE_ENGINE_SHARED_COVERAGE_H_

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace redline {

// Coverage bookkeeping shared by all workers of a fuzzing session: which
// edges some corpus program already reaches ("virgin bits", one bit per
// edge, set while unseen), which edges some crash already reached, and how
// often each edge failed a determinism check.
//
// Every worker owns a target process with its own bitmap (CoverageMap); the
// bitmaps are scanned against this shared state. Claiming is atomic per
// 64-bit word, so two workers that reach the same new edge at the same time
// never both consider it new.
class SharedCoverage {
 public:
  SharedCoverage() = default;
  SharedCoverage(const SharedCoverage&) = delete;
  SharedCoverage& operator=(const SharedCoverage&) = delete;

  // Called by every worker after its target started. The first call sizes
  // the state; later calls only check that the target is the same binary.
  bool Initialize(uint32_t num_edges, uint32_t bitmap_size, std::string* error);
  bool initialized() const { return words_ != 0; }

  uint32_t num_edges() const { return num_edges_; }
  uint32_t found_edges() const {
    return found_edges_.load(std::memory_order_relaxed);
  }
  // found_edges() / (num_edges() + 1), the ratio Fuzzilli reports.
  double coverage_fraction() const;

  // Claims every edge set in `bitmap` that nobody claimed before and
  // returns them (strictly increasing). The caller is now responsible for
  // these edges: it must either add a program for them or Release() them.
  std::vector<uint32_t> ClaimNewEdges(const uint8_t* bitmap);
  // Whether `bitmap` has unclaimed edges, without claiming them.
  bool HasNewEdges(const uint8_t* bitmap) const;
  // Gives claimed edges back (the program reaching them was not kept).
  void Release(const std::vector<uint32_t>& edges);
  // Marks edges as claimed without a program (state import, flaky edges).
  void MarkSeen(const std::vector<uint32_t>& edges);
  // Whether `edge` is currently claimed.
  bool IsSeen(uint32_t edge) const;

  // Like ClaimNewEdges for the crash bits; returns whether the crash reached
  // edges no earlier crash reached (i.e. it is probably a new crash).
  bool ClaimNewCrashEdges(const uint8_t* bitmap);

  // Records that `edge` was reached by a program but not reproducibly.
  // Returns true if the edge has now failed `threshold` times: the caller
  // should keep it claimed (so that it stops costing determinism re-runs)
  // instead of releasing it.
  bool RecordFlaky(uint32_t edge, int threshold);
  uint64_t suppressed_flaky_edges() const {
    return suppressed_.load(std::memory_order_relaxed);
  }

 private:
  uint32_t num_edges_ = 0;
  uint32_t words_ = 0;
  std::unique_ptr<std::atomic<uint64_t>[]> virgin_;
  std::unique_ptr<std::atomic<uint64_t>[]> crash_virgin_;
  std::atomic<uint32_t> found_edges_{0};
  std::mutex flaky_mutex_;
  std::unordered_map<uint32_t, int> flaky_counts_;
  std::atomic<uint64_t> suppressed_{0};
};

}  // namespace redline

#endif  // V8_EXPERIMENTAL_REDLINE_ENGINE_SHARED_COVERAGE_H_
