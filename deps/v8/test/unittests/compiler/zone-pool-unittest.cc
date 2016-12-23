// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/base/utils/random-number-generator.h"
#include "src/compiler/zone-pool.h"
#include "test/unittests/test-utils.h"

namespace v8 {
namespace internal {
namespace compiler {

class ZonePoolTest : public TestWithIsolate {
 public:
  ZonePoolTest() : zone_pool_(&allocator_) {}

 protected:
  ZonePool* zone_pool() { return &zone_pool_; }

  void ExpectForPool(size_t current, size_t max, size_t total) {
    ASSERT_EQ(current, zone_pool()->GetCurrentAllocatedBytes());
    ASSERT_EQ(max, zone_pool()->GetMaxAllocatedBytes());
    ASSERT_EQ(total, zone_pool()->GetTotalAllocatedBytes());
  }

  void Expect(ZonePool::StatsScope* stats, size_t current, size_t max,
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
  ZonePool zone_pool_;
  base::RandomNumberGenerator rng;
};


TEST_F(ZonePoolTest, Empty) {
  ExpectForPool(0, 0, 0);
  {
    ZonePool::StatsScope stats(zone_pool());
    Expect(&stats, 0, 0, 0);
  }
  ExpectForPool(0, 0, 0);
  {
    ZonePool::Scope scope(zone_pool());
    scope.zone();
  }
  ExpectForPool(0, 0, 0);
}


TEST_F(ZonePoolTest, MultipleZonesWithDeletion) {
  static const size_t kArraySize = 10;

  ZonePool::Scope* scopes[kArraySize];

  // Initialize.
  size_t before_stats = 0;
  for (size_t i = 0; i < kArraySize; ++i) {
    scopes[i] = new ZonePool::Scope(zone_pool());
    before_stats += Allocate(scopes[i]->zone());  // Add some stuff.
  }

  ExpectForPool(before_stats, before_stats, before_stats);

  ZonePool::StatsScope stats(zone_pool());

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
    scopes[i] = new ZonePool::Scope(zone_pool());
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


TEST_F(ZonePoolTest, SimpleAllocationLoop) {
  int runs = 20;
  size_t total_allocated = 0;
  size_t max_loop_allocation = 0;
  ZonePool::StatsScope outer_stats(zone_pool());
  {
    ZonePool::Scope outer_scope(zone_pool());
    size_t outer_allocated = 0;
    for (int i = 0; i < runs; ++i) {
      {
        size_t bytes = Allocate(outer_scope.zone());
        outer_allocated += bytes;
        total_allocated += bytes;
      }
      ZonePool::StatsScope inner_stats(zone_pool());
      size_t allocated = 0;
      {
        ZonePool::Scope inner_scope(zone_pool());
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
