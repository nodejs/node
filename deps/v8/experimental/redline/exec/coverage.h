// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXPERIMENTAL_REDLINE_EXEC_COVERAGE_H_
#define V8_EXPERIMENTAL_REDLINE_EXEC_COVERAGE_H_

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "experimental/redline/d8/cov.h"

namespace redline {

// The coverage and extension interface of the target d8.
//   kAuto      detect from the shared memory after the REPRL handshake
//   kRedline   d8 built with v8_redline=true (cov::Header at offset 0)
//   kFuzzilli  d8 built with v8_fuzzilli=true (uint32_t num_edges at offset 0)
enum class TargetKind { kAuto, kRedline, kFuzzilli };

bool ParseTargetKind(std::string_view text, TargetKind* out);
const char* TargetKindName(TargetKind kind);

// The edge coverage shared with one target process, plus the bookkeeping of
// which features have been seen so far ("virgin bits").
//
// The shared memory is a POSIX object named `shm_id_<pid>_<id>`. It supports
// two target layouts:
// - Redline (`experimental/redline/d8/cov.h`, `v8_redline=true`): at offset 0,
// `d8` writes a `cov::Header` with the mode, the edge count, the regions and
// the build ID. The regions are the code edges, the builtins blocks and the
// optimized code features. One bit per edge follows (`trace-pc-guard` and
// `inline-bool-flag`), or one byte per edge (`inline-8bit-counters`, the
// counters layout). Each counter byte holds the hit count of the edge.
// - Fuzzilli (`src/fuzzilli/cov.cc`, `v8_fuzzilli=true`): at offset 0, `d8`
// writes a 4-byte edge count. One bit per edge follows from offset 4, with one
// code region (`1..num_edges`) and no build ID.
//
// Edge indices start at 1. Index 0 is never reported. The parent must clear
// the shared memory before each execution and read it afterwards.
//
// In the counters layout, this class builds a derived bitmap after each
// execution (Refresh()). It has one bit per edge (the edge ran) plus eight
// bucket bits for each watched edge (AFL's hit count buckets, see Bucket()).
// A bucket bit is a "feature" like an edge: it can be new, and the corpus
// can weigh it. All evaluation methods work on features, so the rest of
// Redline sees one bitmap in both layouts.
//
// Not thread-safe. One instance per ReprlChild.
class CoverageMap {
 public:
  // The size of the shared memory. The target maps the size that it gets.
  static constexpr size_t kShmSize = cov::kShmSize;
  // The shared memory constants of a d8 built with v8_fuzzilli=true
  // (src/fuzzilli/cov.cc).
  static constexpr size_t kFuzzilliShmSize = 0x200000;
  static constexpr uint32_t kFuzzilliHeaderSize = 4;
  static constexpr uint32_t kFuzzilliMaxEdges =
      static_cast<uint32_t>((kFuzzilliShmSize - kFuzzilliHeaderSize) * 8);
  // Buckets per watched edge.
  static constexpr uint32_t kBuckets = 8;

  using RegionKind = cov::RegionKind;
  struct Region {
    RegionKind kind;
    uint32_t first;  // index of the first edge
    uint32_t count;
  };
  // What a feature index stands for.
  enum class FeatureKind { kNone, kCode, kBuiltins, kOptimized, kBucket };

  // The edges to watch with hit count buckets (counters layout only).
  struct Watch {
    std::vector<uint32_t> edges;  // edge indices, any order
    bool optimized = true;        // all edges of the optimized code region
  };

  // AFL's bucket of a non-zero hit count: 1, 2, 3, 4-7, 8-15, 16-31, 32-127,
  // 128 or more.
  static uint32_t Bucket(uint8_t count);

  // Creates the shared memory object; `id` must be unique within this
  // process (one per target child).
  static std::unique_ptr<CoverageMap> Create(int id, std::string* error);
  ~CoverageMap();
  CoverageMap(const CoverageMap&) = delete;
  CoverageMap& operator=(const CoverageMap&) = delete;

  // Value for the SHM_ID environment variable of the target.
  const std::string& shm_name() const { return shm_name_; }
  int id() const { return id_; }

  // Sets the edges to watch. Call before FinishInitialization().
  void SetWatch(Watch watch) { watch_ = std::move(watch); }
  // Sets the expected target kind (kAuto detects it from the shared memory).
  void SetTargetKind(TargetKind kind) { requested_kind_ = kind; }
  TargetKind target_kind() const { return detected_kind_; }
  bool is_fuzzilli() const { return detected_kind_ == TargetKind::kFuzzilli; }

  // Reads the header written by the target. Must be called after the target
  // has started (i.e. after the REPRL handshake) and before any evaluation.
  // Returns false if the header is missing or invalid, for example because
  // the target is not instrumented.
  bool FinishInitialization(std::string* error);
  bool initialized() const { return num_edges_ != 0; }

  // Size of the feature index space: the edges, plus the bucket features in
  // the counters layout. Valid feature indices are 1..num_edges().
  uint32_t num_edges() const { return num_edges_; }
  // Number of instrumented edges reported by the target.
  uint32_t real_edges() const { return real_edges_; }
  // In Redline mode, the `cov::Header` that the target wrote. In Fuzzilli
  // mode, a synthetic `cov::Header` with `num_edges`, `bits_per_edge = 1` and
  // one `kCode` region (valid after `FinishInitialization()`).
  const cov::Header& header() const { return header_; }
  // The mode of the coverage instrumentation of the target, for example
  // "inline-bool-flag".
  const char* mode_name() const;
  // The GNU build ID of the target in hex, or an empty string.
  std::string build_id() const;
  // The edges that count for coverage_fraction(): all edges except the
  // optimized code region (indices 1..counted_edges()).
  uint32_t counted_edges() const { return counted_edges_; }
  // Number of distinct counted edges discovered so far (through
  // EvaluateNewEdges).
  uint32_t found_edges() const { return found_edges_; }
  // found_edges() / (counted_edges() + 1): the same ratio Fuzzilli reports.
  double coverage_fraction() const;

  bool counters_mode() const { return counters_mode_; }
  const std::vector<Region>& regions() const { return regions_; }
  // The watched edges, sorted. Watched edge j has the bucket features
  // bucket_base() + kBuckets * j + b, for b in [0, kBuckets).
  const std::vector<uint32_t>& watched() const { return watched_; }
  uint32_t bucket_base() const { return real_edges_ + 1; }
  FeatureKind KindOf(uint32_t feature) const;
  // The edge of a bucket feature, or the feature itself for an edge.
  uint32_t EdgeOf(uint32_t feature) const;

  // Zeroes the shared memory. Call before every execution.
  void ClearBitmap();
  // Builds the derived bitmap from the counters (counters layout only). Call
  // after every execution, before any evaluation.
  void Refresh();
  // The hit count of an edge in the last execution. In the bitmap layout, it
  // is 1 or 0.
  uint32_t HitCount(uint32_t edge) const;

  // Returns the features set in the bitmap that have never been seen before
  // and marks them as seen. Indices are strictly increasing.
  std::vector<uint32_t> EvaluateNewEdges();

  // Like EvaluateNewEdges but against a separate set of bits used for
  // crashing executions, so that crash triage does not pollute the regular
  // coverage state. Only edges count, not bucket features. Returns true if the
  // crash reached unseen crash edges.
  bool EvaluateCrash();

  // Whether every feature in `edges` is set in the current bitmap.
  bool AllEdgesSet(const std::vector<uint32_t>& edges) const;
  // The subset of `edges` that is not set in the current bitmap.
  std::vector<uint32_t> MissingEdges(const std::vector<uint32_t>& edges) const;

  // All features currently set in the bitmap (index 0 excluded).
  std::vector<uint32_t> CurrentEdges() const;
  // Number of counted edges currently set in the bitmap.
  uint32_t CountCurrentEdges() const;

  // Marks `edges` as unseen again (used when edges found by a program turn
  // out to be non-deterministic and the program is discarded).
  void ForgetEdges(const std::vector<uint32_t>& edges);

  // Imports a set of edges as already seen (e.g. when loading a state).
  void MarkSeen(const std::vector<uint32_t>& edges);

  // Resets all bookkeeping to the initial state.
  void ResetState();

  // Raw access to the feature bitmap (bitmap_size() bytes, a multiple of 8),
  // for coverage bookkeeping shared between several targets (see
  // engine/shared_coverage.h).
  const uint8_t* bitmap() const { return bits(); }
  uint32_t bitmap_size() const { return bitmap_size_; }
  // The bitmap bytes that hold the edges, rounded up to 8 bytes. In the
  // counters layout, the last 8 bytes can also hold the first bucket
  // features. Crash deduplication reads this prefix without these features.
  uint32_t edge_bitmap_size() const { return edge_bitmap_size_; }

 private:
  CoverageMap(int id, std::string shm_name, void* mapping);

  // The data (bits or counters) starts after the header.
  uint8_t* shm_edges() { return shmem_ + data_offset_; }
  const uint8_t* shm_edges() const { return shmem_ + data_offset_; }
  const uint8_t* shm_counters() const { return shmem_ + cov::kHeaderSize; }
  uint8_t* shm_counters() { return shmem_ + cov::kHeaderSize; }
  // The bitmap that the evaluation methods read.
  const uint8_t* bits() const {
    return counters_mode_ ? derived_.data() : shm_edges();
  }

  bool FinishFuzzilliInitialization(uint32_t num_edges, std::string* error);
  uint32_t ScanNewEdges(uint8_t* virgin_bits, uint32_t size_bytes,
                        std::vector<uint32_t>* out);

  int id_;
  std::string shm_name_;
  uint8_t* shmem_;
  TargetKind requested_kind_ = TargetKind::kAuto;
  TargetKind detected_kind_ = TargetKind::kRedline;
  uint32_t data_offset_ = cov::kHeaderSize;
  cov::Header header_ = {};
  Watch watch_;
  bool counters_mode_ = false;
  uint32_t real_edges_ = 0;
  uint32_t counted_edges_ = 0;
  uint32_t num_edges_ = 0;
  uint32_t bitmap_size_ = 0;       // bytes, multiple of 8
  uint32_t edge_bitmap_size_ = 0;  // bytes, multiple of 8
  uint32_t found_edges_ = 0;
  std::vector<Region> regions_;
  std::vector<uint32_t> watched_;
  // Runs of consecutive watched edges: the first edge, the index of this edge
  // in watched_, and the length.
  struct WatchRun {
    uint32_t edge;
    uint32_t index;
    uint32_t length;
  };
  std::vector<WatchRun> watch_runs_;
  std::vector<uint8_t> derived_;
  std::vector<uint8_t> virgin_bits_;
  std::vector<uint8_t> crash_bits_;
};

}  // namespace redline

#endif  // V8_EXPERIMENTAL_REDLINE_EXEC_COVERAGE_H_
