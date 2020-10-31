/*
 * Copyright (C) 2019 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef SRC_TRACE_PROCESSOR_IMPORTERS_COMMON_CLOCK_TRACKER_H_
#define SRC_TRACE_PROCESSOR_IMPORTERS_COMMON_CLOCK_TRACKER_H_

#include <stdint.h>

#include <array>
#include <map>
#include <random>
#include <set>
#include <vector>

#include "perfetto/base/logging.h"
#include "perfetto/ext/base/optional.h"

namespace perfetto {
namespace trace_processor {

class TraceProcessorContext;

// This class handles synchronization of timestamps across different clock
// domains. This includes multi-hop conversions from two clocks A and D, e.g.
// A->B -> B->C -> C->D, even if we never saw a snapshot that contains A and D
// at the same time.
// The API is fairly simple (but the inner operation is not):
// - AddSnapshot(map<clock_id, timestamp>): pushes a set of clocks that have
//   been snapshotted at the same time (within technical limits).
// - Convert(src_clock_id, src_timestamp, target_clock_id):
//   converts a timestamp between two clock domains.
//
// Concepts:
// - Snapshot hash:
//   As new snapshots are pushed via AddSnapshot() we compute a snapshot hash.
//   Such hash is the hash(clock_ids) (only IDs, not their timestamps) and is
//   used to find other snapshots that involve the same clock domains.
//   Two clock snapshots have the same hash iff they snapshot the same set of
//   clocks (the order of clocks is irrelevant).
//   This hash is used to efficiently go from the clock graph pathfinder to the
//   time-series obtained by appending the various snapshots.
// - Snapshot id:
//   A simple monotonic counter that is incremented on each AddSnapshot() call.
//
// Data structures:
//  - For each clock domain:
//    - For each snapshot hash:
//      - A logic vector of (snapshot_id, timestamp) tuples (physically stored
//        as two vectors of the same length instead of a vector of pairs).
// This allows to efficiently binary search timestamps within a clock domain
// that were obtained through a particular snapshot.
//
// - A graph of edges (source_clock, target_clock) -> snapshot hash.
//
// Operation:
// Upon each AddSnapshot() call, we incrementally build an unweighted, directed
// graph, which has clock domains as nodes.
// The graph is timestamp-oblivious. As long as we see one snapshot that
// connects two clocks, we assume we'll always be able to convert between them.
// This graph is queried by the Convert() function to figure out the shortest
// path between clock domain, possibly involving hopping through snapshots of
// different type (i.e. different hash).
//
// Example:

// We see a snapshot, with hash S1, for clocks (A,B,C). We build the edges in
// the graph: A->B, B->C, A->C (and the symmetrical ones). In other words we
// keep track of the fact that we can convert between any of them using S1.
// Later we get another snapshot containing (C,E), this snapshot will have a
// different hash (S2, because Hash(C,E) != Hash(A,B,C)) and will add the edges
// C->E, E->C [via S2] to the graph.
// At this point when we are asked to convert a timestamp from A to E, or
// viceversa, we use a simple BFS to figure out a conversion path that is:
// A->C [via S1] + C->E [via S2].
//
// Visually:
// Assume we make the following calls:
//  - AddSnapshot(A:10, B:100)
//  - AddSnapshot(A:20, C:2000)
//  - AddSnapshot(B:400, C:5000)
//  - AddSnapshot(A:30, B:300)

// And assume Hash(A,B) = S1, H(A,C) = S2, H(B,C) = S3
// The vectors in the tracker will look as follows:
// Clock A:
//   S1        {t:10, id:1}                                      {t:30, id:4}
//   S2        |               {t:20, id:2}                      |
//             |               |                                 |
// Clock B:    |               |                                 |
//   S1        {t:100, id:1}   |                                 {t:300, id:4}
//   S3                        |                  {t:400, id:3}
//                             |                  |
// Clock C:                    |                  |
//   S2                        {t: 2000, id: 2}   |
//   S3                                           {t:5000, id:3}

class ClockTracker {
 public:
  using ClockId = uint64_t;

  // IDs in the range [64, 128) are reserved for sequence-scoped clock ids.
  // They can't be passed directly in ClockTracker calls and must be resolved
  // to 64-bit global clock ids by calling SeqScopedClockIdToGlobal().
  static bool IsReservedSeqScopedClockId(ClockId clock_id) {
    return clock_id >= 64 && clock_id < 128;
  }

  // Converts a sequence-scoped clock ids to a global clock id that can be
  // passed as argument to ClockTracker functions.
  static ClockId SeqScopedClockIdToGlobal(uint32_t seq_id, uint32_t clock_id) {
    PERFETTO_DCHECK(IsReservedSeqScopedClockId(clock_id));
    return (static_cast<uint64_t>(seq_id) << 32) | clock_id;
  }

  // Returns whether |global_clock_id| represents a sequence-scoped clock, i.e.
  // a ClockId returned by SeqScopedClockIdToGlobal().
  static bool ClockIsSeqScoped(ClockId global_clock_id) {
    // If the id is > 2**32, this is a sequence-scoped clock id translated into
    // the global namespace.
    return (global_clock_id >> 32) > 0;
  }

  explicit ClockTracker(TraceProcessorContext*);
  virtual ~ClockTracker();

  // Clock description and its value in a snapshot.
  struct ClockValue {
    ClockValue(ClockId id, int64_t ts) : clock_id(id), absolute_timestamp(ts) {}
    ClockValue(ClockId id, int64_t ts, int64_t unit, bool incremental)
        : clock_id(id),
          absolute_timestamp(ts),
          unit_multiplier_ns(unit),
          is_incremental(incremental) {}

    ClockId clock_id;
    int64_t absolute_timestamp;
    int64_t unit_multiplier_ns = 1;
    bool is_incremental = false;
  };

  // Appends a new snapshot for the given clock domains.
  // This is typically called by the code that reads the ClockSnapshot packet.
  void AddSnapshot(const std::vector<ClockValue>&);

  // Converts a timestamp between two clock domains. Tries to use the cache
  // first (only for single-path resolutions), then falls back on path finding
  // as described in the header.
  base::Optional<int64_t> Convert(ClockId src_clock_id,
                                  int64_t src_timestamp,
                                  ClockId target_clock_id) {
    if (PERFETTO_LIKELY(!cache_lookups_disabled_for_testing_)) {
      for (const auto& ce : cache_) {
        if (ce.src != src_clock_id || ce.target != target_clock_id)
          continue;
        int64_t ns = ce.src_domain->ToNs(src_timestamp);
        if (ns >= ce.min_ts_ns && ns < ce.max_ts_ns)
          return ns + ce.translation_ns;
      }
    }
    return ConvertSlowpath(src_clock_id, src_timestamp, target_clock_id);
  }

  base::Optional<int64_t> ConvertSlowpath(ClockId src_clock_id,
                                          int64_t src_timestamp,
                                          ClockId target_clock_id);

  base::Optional<int64_t> ToTraceTime(ClockId clock_id, int64_t timestamp) {
    if (clock_id == trace_time_clock_id_)
      return timestamp;
    return Convert(clock_id, timestamp, trace_time_clock_id_);
  }

  void SetTraceTimeClock(ClockId clock_id) {
    PERFETTO_DCHECK(!IsReservedSeqScopedClockId(clock_id));
    trace_time_clock_id_ = clock_id;
  }

  void set_cache_lookups_disabled_for_testing(bool v) {
    cache_lookups_disabled_for_testing_ = v;
  }

 private:
  using SnapshotHash = uint32_t;

  // 0th argument is the source clock, 1st argument is the target clock.
  using ClockGraphEdge = std::tuple<ClockId, ClockId, SnapshotHash>;

  // A value-type object that carries the information about the path between
  // two clock domains. It's used by the BFS algorithm.
  struct ClockPath {
    static constexpr size_t kMaxLen = 4;
    ClockPath() = default;
    ClockPath(const ClockPath&) = default;

    // Constructs an invalid path with just a source node.
    explicit ClockPath(ClockId clock_id) : last(clock_id) {}

    // Constructs a path by appending a node to |prefix|.
    // If |prefix| = [A,B] and clock_id = C, then |this| = [A,B,C].
    ClockPath(const ClockPath& prefix, ClockId clock_id, SnapshotHash hash) {
      PERFETTO_DCHECK(prefix.len < kMaxLen);
      len = prefix.len + 1;
      path = prefix.path;
      path[prefix.len] = ClockGraphEdge{prefix.last, clock_id, hash};
      last = clock_id;
    }

    bool valid() const { return len > 0; }
    const ClockGraphEdge& at(uint32_t i) const {
      PERFETTO_DCHECK(i < len);
      return path[i];
    }

    uint32_t len = 0;
    ClockId last = 0;
    std::array<ClockGraphEdge, kMaxLen> path;  // Deliberately uninitialized.
  };

  struct ClockSnapshots {
    // Invariant: both vectors have the same length.
    std::vector<uint32_t> snapshot_ids;
    std::vector<int64_t> timestamps_ns;
  };

  struct ClockDomain {
    // One time-series for each hash.
    std::map<SnapshotHash, ClockSnapshots> snapshots;

    // Multiplier for timestamps given in this domain.
    int64_t unit_multiplier_ns = 1;

    // Whether this clock domain encodes timestamps as deltas. This is only
    // supported on sequence-local domains.
    bool is_incremental = false;

    // If |is_incremental| is true, this stores the most recent absolute
    // timestamp in nanoseconds.
    int64_t last_timestamp_ns = 0;

    // Treats |timestamp| as delta timestamp if the clock uses incremental
    // encoding, and as absolute timestamp otherwise.
    int64_t ToNs(int64_t timestamp) {
      if (!is_incremental)
        return timestamp * unit_multiplier_ns;

      int64_t delta_ns = timestamp * unit_multiplier_ns;
      last_timestamp_ns += delta_ns;
      return last_timestamp_ns;
    }

    const ClockSnapshots& GetSnapshot(uint32_t hash) const {
      auto it = snapshots.find(hash);
      PERFETTO_DCHECK(it != snapshots.end());
      return it->second;
    }
  };

  // Holds data for cached entries. At the moment only single-path resolution
  // are cached.
  struct CachedClockPath {
    ClockId src;
    ClockId target;
    ClockDomain* src_domain;
    int64_t min_ts_ns;
    int64_t max_ts_ns;
    int64_t translation_ns;
  };

  ClockTracker(const ClockTracker&) = delete;
  ClockTracker& operator=(const ClockTracker&) = delete;

  ClockPath FindPath(ClockId src, ClockId target);

  ClockDomain* GetClock(ClockId clock_id) {
    auto it = clocks_.find(clock_id);
    PERFETTO_DCHECK(it != clocks_.end());
    return &it->second;
  }

  TraceProcessorContext* const context_;
  ClockId trace_time_clock_id_ = 0;
  std::map<ClockId, ClockDomain> clocks_;
  std::set<ClockGraphEdge> graph_;
  std::set<ClockId> non_monotonic_clocks_;
  std::array<CachedClockPath, 2> cache_{};
  bool cache_lookups_disabled_for_testing_ = false;
  std::minstd_rand rnd_;  // For cache eviction.
  uint32_t cur_snapshot_id_ = 0;
};

}  // namespace trace_processor
}  // namespace perfetto

#endif  // SRC_TRACE_PROCESSOR_IMPORTERS_COMMON_CLOCK_TRACKER_H_
