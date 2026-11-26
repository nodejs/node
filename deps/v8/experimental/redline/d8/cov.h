// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXPERIMENTAL_REDLINE_D8_COV_H_
#define V8_EXPERIMENTAL_REDLINE_D8_COV_H_

// The coverage interface of a d8 built with v8_redline=true. It has the layout
// of the shared memory between d8 and Redline, and the functions that V8 and
// d8 call.
// cov.cc writes the shared memory. Redline reads it (exec/coverage.cc).
// https://clang.llvm.org/docs/SanitizerCoverage.html

#include <cstddef>
#include <cstdint>
#include <set>
#include <vector>

// The layout of the shared memory. All modes of the compiler instrumentation
// use this layout:
//
//   0    Header  the fields of Header, then zeros up to kHeaderSize
//   256  data    one bit per edge (trace-pc-guard and inline-bool-flag), or
//                one byte per edge (inline-8bit-counters)
//
// Edge indices start at 1. Index 0 is not an edge. The parent creates the
// shared memory and gives its name in the environment variable SHM_ID. d8
// writes the header before it sends HELO. The parent clears the data before
// each execution and reads it after the execution.
namespace redline::cov {

// "RLCV" in memory.
inline constexpr uint32_t kMagic = 0x56434c52;
inline constexpr uint32_t kVersion = 1;
// The offset of the data. It is a multiple of 8, so a reader can read the
// data in 8-byte words.
inline constexpr uint32_t kHeaderSize = 256;
// The size of the shared memory that Redline creates. Without SHM_ID, d8 maps
// an anonymous region of this size.
inline constexpr uint32_t kShmSize = 0x400000;
inline constexpr uint32_t kMaxRegions = 8;
inline constexpr uint32_t kMaxBuildIdSize = 32;

// The instrumentation of d8 (sanitizer_coverage_flags).
enum class Mode : uint32_t {
  kNone = 0,                // no instrumentation
  kTracePcGuard = 1,        // trace-pc-guard: one bit per edge
  kInlineBoolFlag = 2,      // inline-bool-flag: one bit per edge
  kInline8bitCounters = 3,  // inline-8bit-counters: one byte per edge
};

// The kind of the edges of a region.
enum class RegionKind : uint32_t {
  // The edges of the C++ code.
  kCode = 1,
  // The basic blocks of the builtins (--fuzzilli-enable-builtins-coverage).
  kBuiltins = 2,
  // The features of optimized code
  // (--fuzzilli-enable-optimized-code-coverage).
  kOptimized = 3,
};

// The bits of Header::flags.
// d8 has a table of the program counters of the edges (pc-table).
inline constexpr uint32_t kFlagPcTable = 1u << 0;

struct Region {
  uint32_t kind;   // a RegionKind
  uint32_t first;  // the index of the first edge
  uint32_t count;  // the number of edges
};

struct Header {
  uint32_t magic;          // kMagic
  uint32_t version;        // kVersion
  uint32_t header_size;    // the offset of the data (kHeaderSize)
  uint32_t shm_size;       // the size of the shared memory of d8
  uint32_t mode;           // a Mode
  uint32_t bits_per_edge;  // BitsPerEdge(mode)
  uint32_t num_edges;      // the edges of all regions
  uint32_t max_edges;      // MaxEdges(shm_size, bits_per_edge)
  uint32_t flags;          // the kFlag* bits
  uint32_t num_regions;    // the entries of `regions` that d8 uses
  uint32_t build_id_size;  // the bytes of `build_id` that d8 uses
  uint32_t reserved;
  uint8_t build_id[kMaxBuildIdSize];  // the GNU build ID of d8
  Region regions[kMaxRegions];        // in the order of their edges
};
static_assert(sizeof(Header) <= kHeaderSize);
static_assert(kHeaderSize % 8 == 0);

constexpr uint32_t BitsPerEdge(Mode mode) {
  return mode == Mode::kInline8bitCounters ? 8 : 1;
}

// The largest edge index that fits into the data of a shared memory of
// `shm_size` bytes. A reader reads the data in 8-byte words, so one word at
// the end stays free.
constexpr uint32_t MaxEdges(uint64_t shm_size, uint32_t bits_per_edge) {
  if (bits_per_edge == 0 || shm_size < kHeaderSize + 16) {
    return 0;
  }
  const uint64_t edges = (shm_size - kHeaderSize - 8) * 8 / bits_per_edge - 1;
  return edges > 0x7fffffff ? 0x7fffffff : static_cast<uint32_t>(edges);
}

}  // namespace redline::cov

// The functions that V8 and d8 call.

void redline_cov_enable();
// Only does something when built with -fsanitize-coverage=trace-pc-guard:
// sets the guards of the edges again, so that the next execution reports
// them.
void sanitizer_cov_reset_edgeguards();
// Only does something when built with -fsanitize-coverage=inline-bool-flag or
// inline-8bit-counters. Transfers the edge flags (or counters) set since the
// last call into the shared memory and clears them. Must be called before the
// shared memory is read.
void sanitizer_cov_flush_bool_flags();
// Only does something when built with -fsanitize-coverage=inline-bool-flag or
// inline-8bit-counters. Clears the edge flags (or counters) set since the last
// call without reporting them. Thus the edges between two executions (e.g.
// during startup) do not count for the next execution.
void sanitizer_cov_discard_bool_flags();
// Only does something when built with -fsanitize-coverage=inline-bool-flag or
// inline-8bit-counters. Installs signal handlers that flush the edge flags (or
// counters) into the shared memory when the process crashes. The handlers
// then defer to the previously installed handler.
void sanitizer_cov_install_crash_flush_handlers();
uint32_t sanitizer_cov_count_discovered_edges();
void cov_init_builtins_edges(uint32_t num_edges);
bool cov_has_builtins_edges();
void cov_set_builtin_edge(uint32_t block_index);

constexpr uint32_t kRedlineNumOptimizedCodeEdges = 128 * 1024;

void cov_init_optimized_code_edges(uint32_t num_edges);
void cov_add_optimized_code_coverage_edges(const std::set<uint32_t>& hashes);
size_t cov_update_and_reset_optimized_code_coverage();

#endif  // V8_EXPERIMENTAL_REDLINE_D8_COV_H_
