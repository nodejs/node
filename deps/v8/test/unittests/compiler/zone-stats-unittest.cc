// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/zone-stats.h"
#include "src/base/utils/random-number-generator.h"
#include "test/unittests/test-utils.h"

namespace v8 {
namespace internal {
namespace compiler {

class ZoneStatsTest : public TestWithIsolate {
 public:
  ZoneStatsTest() : zone_stats_(&allocator_) {}

 protected:
  ZoneStats* zone_stats() { return &zone_stats_; }

  void ExpectForPool(size_t current, size_t max, size_t total) {
    ASSERT_EQ(current, zone_stats()->GetCurrentAllocatedBytes());
    ASSERT_EQ(max, zone_stats()->GetMaxAllocatedBytes());
    ASSERT_EQ(total, zone_stats()->GetTotalAllocatedBytes());
  }

  void Expect(ZoneStats::StatsScope* stats, size_t current, size_t max,
              size_t total) {
    ASSERT_EQ(current, stats->GetCurrentAllocatedBytes());
    ASSERT_EQ(max, stats->GetMaxAllocatedBytes());
    ASSERT_EQ(total, stats->GetTotalAllocatedBytes());
  }

  size_t Allocate(Zone* zone) {
    size_t bytes = rng.NextInt(25) + 7;
    size_t size_before = zone->allocation_size();
    zone->New(bytes);
    return zone->allocation_size() - size_before;
  }

 private:
  v8::internal::AccountingAllocator allocator_;
  ZoneStats zone_stats_;
  base::RandomNumberGenerator rng;
};

TEST_F(ZoneStatsTest, Empty) {
  ExpectForPool(0, 0, 0);
  {
    ZoneStats::StatsScope stats(zone_stats());
    Expect(&stats, 0, 0, 0);
  }
  ExpectForPool(0, 0, 0);
  {
    ZoneStats::Scope scope(zone_stats(), ZONE_NAME);
    scope.zone();
  }
  ExpectForPool(0, 0, 0);
}

TEST_F(ZoneStatsTest, MultipleZonesWithDeletion) {
  static const size_t kArraySize = 10;

  ZoneStats::Scope* scopes[kArraySize];

  // Initialize.
  size_t before_stats = 0;
  for (size_t i = 0; i < kArraySize; ++i) {
    scopes[i] = new ZoneStats::Scope(zone_stats(), ZONE_NAME);
    before_stats += Allocate(scopes[i]->zone());  // Add some stuff.
  }

  ExpectForPool(before_stats, before_stats, before_stats);

  ZoneStats::StatsScope stats(zone_stats());

  size_t before_deletion = 0;
  for (size_t i = 0; i < kArraySize; ++i) {
    before_deletion += Allocate(scopes[i]->zone());  // Add some stuff.
  }

  Expect(&stats, before_deletion, before_deletion, before_deletion);
  ExpectForPool(before_stats + before_deletion, before_stats + before_deletion,
                before_stats + before_deletion);

  // Delete the scopes and create new ones.
  for (size_t i = 0; i < kArraySize; ++i) {
    delete scopes[i];
    scopes[i] = new ZoneStats::Scope(zone_stats(), ZONE_NAME);
  }

  Expect(&stats, 0, before_deletion, before_deletion);
  ExpectForPool(0, before_stats + before_deletion,
                before_stats + before_deletion);

  size_t after_deletion = 0;
  for (size_t i = 0; i < kArraySize; ++i) {
    after_deletion += Allocate(scopes[i]->zone());  // Add some stuff.
  }

  Expect(&stats, after_deletion, std::max(after_deletion, before_deletion),
         before_deletion + after_deletion);
  ExpectForPool(after_deletion,
                std::max(after_deletion, before_stats + before_deletion),
                before_stats + before_deletion + after_deletion);

  // Cleanup.
  for (size_t i = 0; i < kArraySize; ++i) {
    delete scopes[i];
  }

  Expect(&stats, 0, std::max(after_deletion, before_deletion),
         before_deletion + after_deletion);
  ExpectForPool(0, std::max(after_deletion, before_stats + before_deletion),
                before_stats + before_deletion + after_deletion);
}

TEST_F(ZoneStatsTest, SimpleAllocationLoop) {
  int runs = 20;
  size_t total_allocated = 0;
  size_t max_loop_allocation = 0;
  ZoneStats::StatsScope outer_stats(zone_stats());
  {
    ZoneStats::Scope outer_scope(zone_stats(), ZONE_NAME);
    size_t outer_allocated = 0;
    for (int i = 0; i < runs; ++i) {
      {
        size_t bytes = Allocate(outer_scope.zone());
        outer_allocated += bytes;
        total_allocated += bytes;
      }
      ZoneStats::StatsScope inner_stats(zone_stats());
      size_t allocated = 0;
      {
        ZoneStats::Scope inner_scope(zone_stats(), ZONE_NAME);
        for (int j = 0; j < 20; ++j) {
          size_t bytes = Allocate(inner_scope.zone());
          allocated += bytes;
          total_allocated += bytes;
          max_loop_allocation =
              std::max(max_loop_allocation, outer_allocated + allocated);
          Expect(&inner_stats, allocated, allocated, allocated);
          Expect(&outer_stats, outer_allocated + allocated, max_loop_allocation,
                 total_allocated);
          ExpectForPool(outer_allocated + allocated, max_loop_allocation,
                        total_allocated);
        }
      }
      Expect(&inner_stats, 0, allocated, allocated);
      Expect(&outer_stats, outer_allocated, max_loop_allocation,
             total_allocated);
      ExpectForPool(outer_allocated, max_loop_allocation, total_allocated);
    }
  }
  Expect(&outer_stats, 0, max_loop_allocation, total_allocated);
  ExpectForPool(0, max_loop_allocation, total_allocated);
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
