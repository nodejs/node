// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef V8_ENABLE_MAGLEV

#include "src/maglev/maglev-kna-map.h"

#include <algorithm>
#include <map>
#include <random>
#include <vector>

#include "test/unittests/test-utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace internal {
namespace maglev {

namespace {

struct MockNodeInfo {
  int type = 0;
  bool no_info = false;
  bool unstable_map = false;
  bool stale_map = false;

  bool no_info_available() const { return no_info; }
  bool any_map_is_unstable() const { return unstable_map; }
  bool maps_are_stale() const { return stale_map; }

  bool operator==(const MockNodeInfo& other) const {
    return type == other.type && no_info == other.no_info &&
           unstable_map == other.unstable_map && stale_map == other.stale_map;
  }
};

struct MockAvailableExpression {
  int node_id = 0;
  uint32_t effect_epoch = 0;

  bool operator==(const MockAvailableExpression& other) const {
    return node_id == other.node_id && effect_epoch == other.effect_epoch;
  }
};

}  // namespace

class KnaMapTest : public TestWithZone {
 public:
  template <typename MapT>
  static std::map<typename MapT::key_type, typename MapT::mapped_type> ToStdMap(
      const MapT& map) {
    std::map<typename MapT::key_type, typename MapT::mapped_type> res;
    for (const auto& [k, v] : map) {
      res.emplace(k, v);
    }
    EXPECT_EQ(res.size(), map.size());
    EXPECT_EQ(res.empty(), map.empty());
    return res;
  }

#ifdef V8_ENABLE_MAGLEV_KNA_BTREE
  template <typename Key, typename Value, size_t kMaxKeys>
  static bool SameRoot(const KnaMap<Key, Value, kMaxKeys>& a,
                       const KnaMap<Key, Value, kMaxKeys>& b) {
    return a.IsSharedWith(b);
  }

  template <typename Key, typename Value, size_t kMaxKeys>
  static size_t TreeDepth(const KnaMap<Key, Value, kMaxKeys>& map) {
    size_t depth = 0;
    const auto* curr = map.root_;
    while (curr) {
      ++depth;
      if (curr->is_leaf) break;
      curr =
          static_cast<
              const typename KnaMap<Key, Value, kMaxKeys>::InternalNode*>(curr)
              ->children[0];
    }
    return depth;
  }

  template <typename PoolT>
  static size_t FreeLeafCount(const PoolT& pool) {
    size_t count = 0;
    for (auto* p = pool.free_leaves; p != nullptr; p = p->next_free) {
      ++count;
    }
    return count;
  }

  template <typename PoolT>
  static size_t FreeInternalCount(const PoolT& pool) {
    size_t count = 0;
    for (auto* p = pool.free_internals; p != nullptr; p = p->next_free) {
      ++count;
    }
    return count;
  }
#endif

  template <typename Map>
  void RunMultiLevelGrowthAndEraseToEmpty(size_t expected_min_depth) {
    typename Map::Pool pool(zone());
    Map map(&pool);
    std::map<int, int> ref;

    std::vector<int> keys(300);
    for (int i = 0; i < 300; ++i) keys[i] = i * 3;
    std::mt19937 rng(42);
    std::shuffle(keys.begin(), keys.end(), rng);

    for (int k : keys) {
      auto [it, inserted] = map.try_emplace(k, k + 100);
      EXPECT_TRUE(inserted);
      EXPECT_EQ(it->first, k);
      EXPECT_EQ(it->second, k + 100);
      ref.emplace(k, k + 100);

      // Duplicate try_emplace does not overwrite.
      auto [dup_it, dup_inserted] = map.try_emplace(k, -1);
      EXPECT_FALSE(dup_inserted);
      EXPECT_EQ(dup_it->second, k + 100);
    }

    EXPECT_EQ(ToStdMap(map), ref);
#ifdef V8_ENABLE_MAGLEV_KNA_BTREE
    EXPECT_GE(TreeDepth(map), expected_min_depth);
#else
    USE(expected_min_depth);
#endif

    for (int i = 0; i < 300; ++i) {
      int k = i * 3;
      EXPECT_TRUE(map.contains(k));
      EXPECT_FALSE(map.contains(k + 1));
      auto it = std::as_const(map).find(k);
      ASSERT_NE(it, map.end());
      EXPECT_EQ(it->second, k + 100);
    }

    // Erase all keys in another random order, exercising both in-place leaf
    // erase and full-tree rebuild/coalesce erase paths.
    std::shuffle(keys.begin(), keys.end(), rng);
    for (size_t i = 0; i < keys.size(); ++i) {
      int k = keys[i];
      EXPECT_EQ(map.erase(k + 1), 0u);
      EXPECT_EQ(map.erase(k), 1u);
      ref.erase(k);
      if (i % 15 == 0 || ref.empty()) {
        EXPECT_EQ(ToStdMap(map), ref);
      }
    }
    EXPECT_TRUE(map.empty());
    EXPECT_EQ(map.begin(), map.end());
  }

  template <typename Map>
  void RunRandomizedDifferentialAgainstStdMap() {
    typename Map::Pool pool(zone());
    Map map(&pool);
    std::map<int, int> ref;

    Map snapshot(&pool);
    std::map<int, int> snapshot_ref;

    std::mt19937 rng(1337);
    std::uniform_int_distribution<int> key_dist(0, 120);
    std::uniform_int_distribution<int> val_dist(-500, 500);
    std::uniform_int_distribution<int> op_dist(0, 9);

    for (int step = 0; step < 1200; ++step) {
      int op = op_dist(rng);
      switch (op) {
        case 0:
        case 1: {
          int k = key_dist(rng);
          int v = val_dist(rng);
          auto [it1, ins1] = map.try_emplace(k, v);
          auto [it2, ins2] = ref.try_emplace(k, v);
          EXPECT_EQ(ins1, ins2);
          EXPECT_EQ(it1->second, it2->second);
          break;
        }
        case 2: {
          int k = key_dist(rng);
          int v = val_dist(rng);
          map[k] = v;
          ref[k] = v;
          break;
        }
        case 3: {
          int k = key_dist(rng);
          EXPECT_EQ(map.erase(k), ref.erase(k));
          break;
        }
        case 4: {
          snapshot = map;
          snapshot_ref = ref;
          break;
        }
        case 5: {
          int mod = (step % 5) + 2;
          size_t erased_map = map.EraseIf([mod](const int& k, int& v) {
            if (k % mod == 0) return true;
            v += 1;
            return false;
          });
          size_t erased_ref = std::erase_if(ref, [mod](auto& entry) {
            if (entry.first % mod == 0) return true;
            entry.second += 1;
            return false;
          });
          EXPECT_EQ(erased_map, erased_ref);
          break;
        }
        case 6: {
          map.MutateValues([](const int& k, int& v) {
            if (k % 2 == 0) v ^= 3;
          });
          for (auto& [k, v] : ref) {
            if (k % 2 == 0) v ^= 3;
          }
          break;
        }
        case 7: {
          auto remap = [](const int& k) { return k / 2; };
          auto combine = [](int& dst, const int& src) { dst += src; };
          map.RemapKeys(remap, combine);
          std::map<int, int> new_ref;
          for (const auto& [k, v] : ref) {
            auto [it, inserted] = new_ref.try_emplace(remap(k), v);
            if (!inserted) combine(it->second, v);
          }
          ref = std::move(new_ref);
          break;
        }
        case 8: {
          if (step % 2 == 0) {
            map.DestructivelyMergeWith(
                snapshot,
                [](const int&, int& lhs, const int& rhs) {
                  if ((lhs & 1) != (rhs & 1)) return false;
                  lhs = std::min(lhs, rhs);
                  return true;
                },
                NeverKeepLhsOnly{});
            for (auto it = ref.begin(); it != ref.end();) {
              auto sit = snapshot_ref.find(it->first);
              if (sit == snapshot_ref.end() ||
                  ((it->second & 1) != (sit->second & 1))) {
                it = ref.erase(it);
              } else {
                it->second = std::min(it->second, sit->second);
                ++it;
              }
            }
          } else {
            map.DestructivelyMergeWith(
                snapshot,
                [](const int&, int& lhs, const int& rhs) {
                  lhs += rhs;
                  return true;
                },
                [](const int& k, int& lhs) {
                  lhs -= 1;
                  return (k & 1) == 0;
                });
            for (auto it = ref.begin(); it != ref.end();) {
              auto sit = snapshot_ref.find(it->first);
              if (sit != snapshot_ref.end()) {
                it->second += sit->second;
                ++it;
              } else if ((it->first & 1) == 0) {
                it->second -= 1;
                ++it;
              } else {
                it = ref.erase(it);
              }
            }
          }
          break;
        }
        case 9: {
          auto cmp = [](int a, int b) { return a <= b; };
          auto is_empty = [](int a) { return (a & 7) == 0; };
          bool expected = true;
          for (const auto& [k, v] : ref) {
            if (is_empty(v)) continue;
            auto sit = snapshot_ref.find(k);
            if (sit == snapshot_ref.end() || !cmp(v, sit->second)) {
              expected = false;
              break;
            }
          }
          EXPECT_EQ(map.Includes(snapshot, cmp, is_empty), expected);
          EXPECT_TRUE(map.Includes(map, std::equal_to<int>{}));
          break;
        }
      }

      ASSERT_EQ(ToStdMap(map), ref) << "Mismatch at step " << step;
      ASSERT_EQ(ToStdMap(snapshot), snapshot_ref)
          << "Snapshot mutated at step " << step;
    }
  }
};

TEST_F(KnaMapTest, MultiLevelGrowthAndEraseToEmpty) {
#ifdef V8_ENABLE_MAGLEV_KNA_BTREE
  RunMultiLevelGrowthAndEraseToEmpty<KnaMap<int, int, 4>>(4u);
  RunMultiLevelGrowthAndEraseToEmpty<KnaMap<int, int, 8>>(3u);
#else
  RunMultiLevelGrowthAndEraseToEmpty<KnaMap<int, int>>(0u);
#endif
}

#ifdef V8_ENABLE_MAGLEV_KNA_BTREE
TEST_F(KnaMapTest, CopyOnWriteAndPoolFreelistReuse) {
  using Map = KnaMap<int, int, 4>;
  Map::Pool pool(zone());

  {
    Map m1(&pool);
    for (int i = 0; i < 100; ++i) {
      m1[i] = i * 10;
    }
    Map m2 = m1;
    EXPECT_TRUE(SameRoot(m1, m2));

    // Mutating m2 unshares only the modified path and leaves m1 intact.
    m2[50] = 9999;
    EXPECT_FALSE(SameRoot(m1, m2));
    EXPECT_EQ(std::as_const(m1).find(50)->second, 500);
    EXPECT_EQ(std::as_const(m2).find(50)->second, 9999);

    // No-op MutateValues preserves root sharing.
    Map m3 = m1;
    EXPECT_TRUE(SameRoot(m1, m3));
    m3.MutateValues([](const int&, int&) {});
    EXPECT_TRUE(SameRoot(m1, m3));

    // No-op EraseIf preserves root sharing.
    EXPECT_EQ(m3.EraseIf([](const int&, const int&) { return false; }), 0u);
    EXPECT_TRUE(SameRoot(m1, m3));
  }

  // After all maps in the scope are destroyed, every allocated leaf and
  // internal node must be on the pool's freelists, so rebuilding the same
  // 100-key tree allocates 0 new bytes from the Zone.
  EXPECT_GT(FreeLeafCount(pool), 0u);
  EXPECT_GT(FreeInternalCount(pool), 0u);
  size_t zone_bytes_before = zone()->allocation_size();
  {
    Map m_reused(&pool);
    for (int i = 0; i < 100; ++i) {
      m_reused[i] = i * 10;
    }
    EXPECT_EQ(m_reused.size(), 100u);
  }
  EXPECT_EQ(zone()->allocation_size(), zone_bytes_before);
}

TEST_F(KnaMapTest, FindConstVsMutableAndSharedInnerMapDestructiveMerge) {
  using InnerMap = KnaMap<int, int, 4>;
  using OuterMap = KnaMap<int, InnerMap, 4>;
  InnerMap::Pool inner_pool(zone());
  OuterMap::Pool outer_pool(zone());

  OuterMap outer1(&outer_pool);
  {
    auto& inner5 = outer1.try_emplace(5, &inner_pool).first->second;
    inner5[1] = 10;
    inner5[2] = 20;
    inner5[3] = 30;
    auto& inner10 = outer1.try_emplace(10, &inner_pool).first->second;
    inner10[3] = 30;
  }

  OuterMap outer2 = outer1;
  EXPECT_TRUE(SameRoot(outer1, outer2));

  // Const find() and missed mutable find() do not unshare outer1 and outer2.
  auto const_it10 = std::as_const(outer1).find(10);
  ASSERT_NE(const_it10, outer1.end());
  EXPECT_EQ(const_it10->second.size(), 1u);
  EXPECT_EQ(outer1.find(999), outer1.end());
  EXPECT_TRUE(SameRoot(outer1, outer2));

  // Mutable find() unshares the outer leaf while keeping unmodified inner maps
  // shared.
  auto it10 = outer1.find(10);
  ASSERT_NE(it10, outer1.end());
  EXPECT_FALSE(SameRoot(outer1, outer2));
  EXPECT_TRUE(SameRoot(std::as_const(outer1).find(5)->second,
                       std::as_const(outer2).find(5)->second));
  it10->second.clear();
  EXPECT_TRUE(std::as_const(outer1).find(10)->second.empty());
  EXPECT_FALSE(std::as_const(outer2).find(10)->second.empty());

  // When outer3 is cloned from outer1 while key 10 has an in-place cleared
  // (empty) inner map, DestructivelyMergeWith must prune the empty outer entry
  // rather than blindly skipping the shared root.
  OuterMap outer3 = outer1;
  EXPECT_TRUE(SameRoot(outer1, outer3));
  outer1.DestructivelyMergeWith(
      outer3,
      [](const int&, InnerMap& lhs, const InnerMap& rhs) {
        lhs.DestructivelyMergeWith(rhs, DefaultMergeFunc{}, NeverKeepLhsOnly{});
        return !lhs.empty();
      },
      NeverKeepLhsOnly{});
  EXPECT_FALSE(outer1.contains(10));
  EXPECT_TRUE(outer1.contains(5));
}

TEST_F(KnaMapTest,
       DestructivelyMergeWithNodeInfoAndAvailableExpressionFlavors) {
  // Test MockNodeInfo flavor (has no_info_available / any_map_is_unstable /
  // maps_are_stale).
  using NodeInfoMap = KnaMap<int, MockNodeInfo, 4>;
  NodeInfoMap::Pool info_pool(zone());
  NodeInfoMap infos1(&info_pool);
  for (int i = 0; i < 20; ++i) {
    infos1[i] = MockNodeInfo{.type = i,
                             .no_info = false,
                             .unstable_map = (i == 15),
                             .stale_map = false};
  }
  NodeInfoMap infos2 = infos1;
  EXPECT_TRUE(SameRoot(infos1, infos2));

  bool saw_unstable = false;
  infos1.DestructivelyMergeWith(
      infos2,
      [&](const int&, MockNodeInfo& lhs, const MockNodeInfo& rhs) {
        if (lhs.any_map_is_unstable() && !lhs.maps_are_stale()) {
          saw_unstable = true;
        }
        return !lhs.no_info_available() && !rhs.no_info_available();
      },
      NeverKeepLhsOnly{});
  // Shared-root fast path must still invoke the callback on the unstable entry
  // to preserve `side_effects_require_invalidation_` while keeping root shared.
  EXPECT_TRUE(saw_unstable);
  EXPECT_TRUE(SameRoot(infos1, infos2));

  // If an entry has `no_info_available() == true`, shared merge must prune it.
  infos1[7].no_info = true;
  NodeInfoMap infos3 = infos1;
  infos1.DestructivelyMergeWith(
      infos3,
      [](const int&, MockNodeInfo& lhs, const MockNodeInfo& rhs) {
        return !lhs.no_info_available() && !rhs.no_info_available();
      },
      NeverKeepLhsOnly{});
  EXPECT_FALSE(infos1.contains(7));
  EXPECT_EQ(infos1.size(), 19u);

  // Test MockAvailableExpression flavor (has `effect_epoch`).
  using ExprMap = KnaMap<uint32_t, MockAvailableExpression, 4>;
  ExprMap::Pool expr_pool(zone());
  ExprMap exprs1(&expr_pool);
  for (uint32_t i = 0; i < 20; ++i) {
    exprs1[i] = MockAvailableExpression{static_cast<int>(i),
                                        /*effect_epoch=*/10};
  }
  ExprMap exprs2 = exprs1;
  uint32_t current_epoch = 5;
  exprs1.DestructivelyMergeWith(
      exprs2,
      [&](const uint32_t&, MockAvailableExpression& lhs,
          const MockAvailableExpression& rhs) {
        return lhs == rhs && current_epoch <= lhs.effect_epoch;
      },
      NeverKeepLhsOnly{});
  EXPECT_TRUE(SameRoot(exprs1, exprs2));
  EXPECT_EQ(exprs1.size(), 20u);

  // Advancing `current_epoch` past some entries' `effect_epoch` prunes them
  // even when merging two maps that share the same root.
  exprs1[3].effect_epoch = 2;
  ExprMap exprs3 = exprs1;
  exprs1.DestructivelyMergeWith(
      exprs3,
      [&](const uint32_t&, MockAvailableExpression& lhs,
          const MockAvailableExpression& rhs) {
        return lhs == rhs && current_epoch <= lhs.effect_epoch;
      },
      NeverKeepLhsOnly{});
  EXPECT_FALSE(exprs1.contains(3u));
  EXPECT_EQ(exprs1.size(), 19u);
}

TEST_F(KnaMapTest, EraseAllInSharedLeafDoesNotAllocateNewLeaf) {
  using Map = KnaMap<int, int, 4>;
  Map::Pool pool(zone());
  Map m1(&pool);
  for (int i = 0; i < 4; ++i) m1[i] = i * 10;
  Map m2 = m1;
  EXPECT_TRUE(SameRoot(m1, m2));

  size_t zone_bytes_before = zone()->allocation_size();
  size_t erased = m1.EraseIf([](const int&, int&) { return true; });
  EXPECT_EQ(erased, 4u);
  EXPECT_TRUE(m1.empty());
  EXPECT_EQ(m2.size(), 4u);
  EXPECT_EQ(zone()->allocation_size(), zone_bytes_before);
  EXPECT_EQ(FreeLeafCount(pool), 0u);

  // Same for DestructivelyMergeWith when all entries in a shared leaf are
  // dropped.
  Map m3 = m2;
  Map m_disjoint(&pool);
  m_disjoint[100] = 1000;
  zone_bytes_before = zone()->allocation_size();
  m3.DestructivelyMergeWith(m_disjoint, DefaultMergeFunc{}, NeverKeepLhsOnly{});
  EXPECT_TRUE(m3.empty());
  EXPECT_EQ(m2.size(), 4u);
  EXPECT_EQ(zone()->allocation_size(), zone_bytes_before);
  EXPECT_EQ(FreeLeafCount(pool), 0u);
}
#endif  // V8_ENABLE_MAGLEV_KNA_BTREE

TEST_F(KnaMapTest, RandomizedDifferentialAgainstStdMap) {
#ifdef V8_ENABLE_MAGLEV_KNA_BTREE
  RunRandomizedDifferentialAgainstStdMap<KnaMap<int, int, 4>>();
  RunRandomizedDifferentialAgainstStdMap<KnaMap<int, int, 8>>();
#else
  RunRandomizedDifferentialAgainstStdMap<KnaMap<int, int>>();
#endif
}

}  // namespace maglev
}  // namespace internal
}  // namespace v8

#endif  // V8_ENABLE_MAGLEV
