// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <tuple>

#include "src/base/utils/random-number-generator.h"
#include "src/compiler/persistent-map.h"
#include "test/unittests/test-utils.h"

namespace v8 {
namespace internal {
namespace compiler {

// A random distribution that produces both small values and arbitrary numbers.
static int small_big_distr(base::RandomNumberGenerator* rand) {
  return rand->NextInt() / std::max(1, rand->NextInt() / 100);
}

class PersistentMapTest : public TestWithPlatform {};

TEST_F(PersistentMapTest, RefTest) {
  base::RandomNumberGenerator rand(92834738);
  AccountingAllocator allocator;
  Zone zone(&allocator, ZONE_NAME);
  std::vector<PersistentMap<int, int>> pers_maps;
  pers_maps.emplace_back(&zone);
  std::vector<std::map<int, int>> ref_maps(1);
  for (int i = 0; i < 100000; ++i) {
    if (rand.NextInt(2) == 0) {
      // Read value;
      int key = small_big_distr(&rand);
      if (ref_maps[0].count(key) > 0) {
        ASSERT_EQ(pers_maps[0].Get(key), ref_maps[0][key]);
      } else {
        ASSERT_EQ(pers_maps[0].Get(key), 0);
      }
    }
    if (rand.NextInt(2) == 0) {
      // Add value;
      int key = small_big_distr(&rand);
      int value = small_big_distr(&rand);
      pers_maps[0].Set(key, value);
      ref_maps[0][key] = value;
    }
    if (rand.NextInt(1000) == 0) {
      // Create empty map.
      pers_maps.emplace_back(&zone);
      ref_maps.emplace_back();
    }
    if (rand.NextInt(100) == 0) {
      // Copy and move around maps.
      int num_maps = static_cast<int>(pers_maps.size());
      int source = rand.NextInt(num_maps - 1) + 1;
      int target = rand.NextInt(num_maps - 1) + 1;
      pers_maps[target] = std::move(pers_maps[0]);
      ref_maps[target] = std::move(ref_maps[0]);
      pers_maps[0] = pers_maps[source];
      ref_maps[0] = ref_maps[source];
    }
  }
  for (size_t i = 0; i < pers_maps.size(); ++i) {
    std::set<int> keys;
    for (auto pair : pers_maps[i]) {
      ASSERT_EQ(keys.count(pair.first), 0u);
      keys.insert(pair.first);
      ASSERT_EQ(ref_maps[i][pair.first], pair.second);
    }
    for (auto pair : ref_maps[i]) {
      int value = pers_maps[i].Get(pair.first);
      ASSERT_EQ(pair.second, value);
      if (value != 0) {
        ASSERT_EQ(keys.count(pair.first), 1u);
        keys.erase(pair.first);
      }
    }
    ASSERT_TRUE(keys.empty());
  }
}

TEST_F(PersistentMapTest, Zip) {
  base::RandomNumberGenerator rand(92834738);
  AccountingAllocator allocator;
  Zone zone(&allocator, ZONE_NAME);

  // Provoke hash collisions to stress the iterator.
  struct bad_hash {
    size_t operator()(uint32_t key) {
      return base::hash_value(static_cast<size_t>(key) % 1000);
    }
  };
  PersistentMap<int, uint32_t, bad_hash> a(&zone);
  PersistentMap<int, uint32_t, bad_hash> b(&zone);

  uint32_t sum_a = 0;
  uint32_t sum_b = 0;

  for (int i = 0; i < 30000; ++i) {
    int key = small_big_distr(&rand);
    uint32_t value = small_big_distr(&rand);
    if (rand.NextBool()) {
      sum_a += value;
      a.Set(key, a.Get(key) + value);
    } else {
      sum_b += value;
      b.Set(key, b.Get(key) + value);
    }
  }

  uint32_t sum = sum_a + sum_b;

  for (auto pair : a) {
    sum_a -= pair.second;
  }
  ASSERT_EQ(0u, sum_a);

  for (auto pair : b) {
    sum_b -= pair.second;
  }
  ASSERT_EQ(0u, sum_b);

  for (auto triple : a.Zip(b)) {
    int key = std::get<0>(triple);
    uint32_t value_a = std::get<1>(triple);
    uint32_t value_b = std::get<2>(triple);
    ASSERT_EQ(value_a, a.Get(key));
    ASSERT_EQ(value_b, b.Get(key));
    sum -= value_a;
    sum -= value_b;
  }
  ASSERT_EQ(0u, sum);
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
