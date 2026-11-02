// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXPERIMENTAL_REDLINE_EXEC_COVERAGE_H_
#define V8_EXPERIMENTAL_REDLINE_EXEC_COVERAGE_H_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace redline {

// The edge coverage bitmap shared with one target process, plus the
// bookkeeping of which edges have been seen so far ("virgin bits").
//
// Layout and semantics are those of Fuzzilli's libcoverage / V8's
// src/fuzzilli/cov.cc: a POSIX shared memory object named
// "shm_id_<pid>_<id>" of kShmSize bytes starting with a uint32 edge count
// written by the target at startup, followed by one bit per edge. Edge
// indices start at 1; index 0 is never reported. The target sets a bit the
// first time an edge executes within a REPRL iteration and resets its
// guards at the end of every iteration, so the parent must clear the bitmap
// before each execution and read it afterwards.
//
// Not thread-safe; one instance per ReprlChild.
class CoverageMap {
 public:
  // Must match SHM_SIZE in src/fuzzilli/cov.cc: the target maps this many
  // bytes, so a smaller region would make it fault.
  static constexpr size_t kShmSize = 0x200000;
  // The 4-byte num_edges header leaves kShmSize - 4 bytes for the bitmap,
  // and scans read 8-byte words, so the usable bitmap is the largest multiple
  // of 8 bytes that fits after the header.
  static constexpr uint32_t kMaxEdges = static_cast<uint32_t>(
      ((kShmSize - sizeof(uint32_t)) & ~size_t{7}) * 8 - 1);

  // Creates the shared memory object; `id` must be unique within this
  // process (one per target child).
  static std::unique_ptr<CoverageMap> Create(int id, std::string* error);
  ~CoverageMap();
  CoverageMap(const CoverageMap&) = delete;
  CoverageMap& operator=(const CoverageMap&) = delete;

  // Value for the SHM_ID environment variable of the target.
  const std::string& shm_name() const { return shm_name_; }
  int id() const { return id_; }

  // Reads the edge count published by the target. Must be called after the
  // target has started (i.e. after the REPRL handshake) and before any
  // evaluation. Returns false if the target did not publish an edge count,
  // which means it is not instrumented.
  bool FinishInitialization(std::string* error);
  bool initialized() const { return num_edges_ != 0; }

  // Number of instrumented edges reported by the target.
  uint32_t num_edges() const { return num_edges_; }
  // Number of distinct edges discovered so far (through EvaluateNewEdges).
  uint32_t found_edges() const { return found_edges_; }
  // found_edges() / (num_edges() + 1): the same ratio Fuzzilli reports.
  double coverage_fraction() const;

  // Zeroes the bitmap. Call before every execution.
  void ClearBitmap();

  // Returns the edges set in the bitmap that have never been seen before
  // and marks them as seen. Indices are strictly increasing.
  std::vector<uint32_t> EvaluateNewEdges();

  // Like EvaluateNewEdges but against a separate set of bits used for
  // crashing executions, so that crash triage does not pollute the regular
  // coverage state. Returns true if the crash reached unseen crash edges.
  bool EvaluateCrash();

  // Whether every edge in `edges` is set in the current bitmap.
  bool AllEdgesSet(const std::vector<uint32_t>& edges) const;
  // The subset of `edges` that is not set in the current bitmap.
  std::vector<uint32_t> MissingEdges(const std::vector<uint32_t>& edges) const;

  // All edges currently set in the bitmap (index 0 excluded).
  std::vector<uint32_t> CurrentEdges() const;
  // Number of edges currently set in the bitmap.
  uint32_t CountCurrentEdges() const;

  // Marks `edges` as unseen again (used when edges found by a program turn
  // out to be non-deterministic and the program is discarded).
  void ForgetEdges(const std::vector<uint32_t>& edges);

  // Imports a set of edges as already seen (e.g. when loading a state).
  void MarkSeen(const std::vector<uint32_t>& edges);

  // Resets all bookkeeping to the initial state.
  void ResetState();

  // Raw access to the bitmap written by the target (bitmap_size() bytes, a
  // multiple of 8), for coverage bookkeeping shared between several targets
  // (see engine/shared_coverage.h).
  const uint8_t* bitmap() const { return shm_edges(); }
  uint32_t bitmap_size() const { return bitmap_size_; }

 private:
  CoverageMap(int id, std::string shm_name, void* mapping);

  // Layout of the shared memory region (libcoverage's shmem_data): the edge
  // count followed by the edge bitmap. The bitmap starts right after the
  // header, see shm_edges().
  struct ShmemData {
    uint32_t num_edges;
  };
  uint8_t* shm_edges() { return reinterpret_cast<uint8_t*>(shmem_ + 1); }
  const uint8_t* shm_edges() const {
    return reinterpret_cast<const uint8_t*>(shmem_ + 1);
  }

  uint32_t ScanNewEdges(uint8_t* virgin_bits, std::vector<uint32_t>* out);

  int id_;
  std::string shm_name_;
  ShmemData* shmem_;
  uint32_t num_edges_ = 0;
  uint32_t bitmap_size_ = 0;  // bytes, multiple of 8
  uint32_t found_edges_ = 0;
  std::vector<uint8_t> virgin_bits_;
  std::vector<uint8_t> crash_bits_;
};

}  // namespace redline

#endif  // V8_EXPERIMENTAL_REDLINE_EXEC_COVERAGE_H_
