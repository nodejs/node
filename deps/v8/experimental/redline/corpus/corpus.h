// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXPERIMENTAL_REDLINE_CORPUS_CORPUS_H_
#define V8_EXPERIMENTAL_REDLINE_CORPUS_CORPUS_H_

#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

#include "experimental/redline/core/component.h"
#include "experimental/redline/core/program.h"
#include "experimental/redline/core/random.h"
#include "experimental/redline/core/selection.h"

namespace redline {

// The strategies that pick corpus programs are Selector<const Program>
// (core/selection.h): one picks the seed a worker mutates next (built-in:
// uniform, recency, tournament, productive), one picks the partner that
// Splice/Combine take material from (built-in: uniform). They are created
// from the configuration through SelectorRegistry<const Program> and are
// called with the corpus lock held, so they may keep plain state; they must
// be cheap because every mutation goes through them.
using ProgramSelector = Selector<const Program>;

class Corpus;

// RAII checkout guard for a leased corpus seed. While alive, the scheduler
// prefers unleased active seeds for other workers. Destruction releases the
// lease back to the `Corpus`. A `SeedLease` must not outlive its `Corpus`.
class SeedLease {
 public:
  SeedLease() = default;
  ~SeedLease();
  SeedLease(SeedLease&& other) noexcept;
  SeedLease& operator=(SeedLease&& other) noexcept;
  SeedLease(const SeedLease&) = delete;
  SeedLease& operator=(const SeedLease&) = delete;

  // Releases the lease early and clears the held program pointer.
  void Release();

  const ProgramPtr& program() const { return program_; }
  const Program* get() const { return program_.get(); }
  const Program* operator->() const { return program_.get(); }
  const Program& operator*() const { return *program_; }
  explicit operator bool() const { return program_ != nullptr; }

 private:
  friend class Corpus;
  SeedLease(Corpus* corpus, ProgramPtr program, size_t index);

  Corpus* corpus_ = nullptr;
  ProgramPtr program_;
  size_t index_ = ProgramSelector::kNoChoice;
};

// The set of programs known to reach new coverage. Append-only and shared
// between all workers; programs are immutable so readers only need the lock
// for the vector itself.
class Corpus {
 public:
  Corpus(std::unique_ptr<ProgramSelector> seeds,
         std::unique_ptr<ProgramSelector> partners);
  ~Corpus();

  // Assigns an id and publishes the program. Returns the published pointer.
  ProgramPtr Add(std::shared_ptr<Program> program);

  // Picks a parent for mutation (nullptr if the corpus is empty) and counts
  // the pick on the program.
  ProgramPtr Pick(Random& rng);
  // Inspects up to `tries` seeds under one lock without aging rejected
  // candidates. Returns the first seed where `prefer` is true, or else the
  // first candidate. Only the returned seed ages and increments `picks`.
  ProgramPtr PickPreferred(Random& rng, int tries,
                           const std::function<bool(const Program&)>& prefer);
  // Leases a parent for mutation and prefers unleased active seeds.
  SeedLease Lease(Random& rng);
  // Leases up to `tries` preferred candidates without aging rejected seeds.
  SeedLease LeasePreferred(Random& rng, int tries,
                           const std::function<bool(const Program&)>& prefer);
  // Picks a partner for `parent` (nullptr if there is no other program).
  ProgramPtr PickPartner(const Program& parent, Random& rng);
  // Reports the outcome of a mutation of `parent` back to the strategies.
  void OnResult(const Program& parent, bool interesting);
  // True if a strategy needs Program::hit_edges filled before Add().
  bool WantsHitEdges() const;
  // The edges of a crash that reproduces (see Selector::OnCrashEdges).
  void RecordCrashEdges(const std::vector<uint32_t>& edges);

  size_t size() const;
  // Number of active (positive-energy) seeds, or `size()` for non-rarity
  // strategies.
  size_t active_size() const;
  // Number of distinct seeds currently held by `SeedLease` guards.
  size_t leased_size() const;
  // Number of corpus programs that currently keep a parsed AST tree in memory.
  size_t resident_trees() const;
  // Verifies internal scheduler invariants. Returns false with `error` on
  // mismatch.
  bool CheckInvariantsForTest(std::string* error) const;
  ProgramPtr Get(uint32_t id) const;
  std::vector<ProgramPtr> Snapshot() const;
  // Sum of node_count over the corpus and total number of edges attributed
  // to corpus programs (informational).
  uint64_t total_nodes() const {
    return total_nodes_.load(std::memory_order_relaxed);
  }
  uint64_t total_edges() const {
    return total_edges_.load(std::memory_order_relaxed);
  }

  const ProgramSelector& seeds() const { return *seeds_; }
  const ProgramSelector& partners() const { return *partners_; }
  // Statistics of the seed picks, named after the strategy, for the stats
  // tables (invocations = picks, interesting = picks whose mutation
  // produced a new corpus program).
  const Component& seed_stats() const { return seed_stats_; }

 private:
  friend class SeedLease;

  SeedLease LeasePreferredInternal(
      Random& rng, int tries, const std::function<bool(const Program&)>& prefer,
      bool acquire_lease);
  void ReleaseLease(size_t index);
  void NoteTreeAccessedLocked(size_t index) const;
  void EvictExcessTreesLocked();

  mutable std::mutex mutex_;
  std::vector<ProgramPtr> programs_;
  std::vector<const Program*>
      candidates_;  // programs_ as the selectors see them
  std::unique_ptr<ProgramSelector> seeds_;
  std::unique_ptr<ProgramSelector> partners_;
  Component seed_stats_;
  mutable std::vector<uint8_t> tree_evicted_;
  mutable size_t resident_trees_ = 0;
  std::atomic<uint64_t> total_nodes_{0};
  std::atomic<uint64_t> total_edges_{0};
};

}  // namespace redline

#endif  // V8_EXPERIMENTAL_REDLINE_CORPUS_CORPUS_H_
