// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/maglev/hamt.h"

#include "test/unittests/test-utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace internal {
namespace maglev {

class HAMTTest : public TestWithZone {
 public:
  using IntHAMT = HAMT<int, int>;

  // Helper to collect all items into a std::map for easy verification
  std::map<int, int> ToMap(const IntHAMT& hamt) {
    std::map<int, int> result;
    for (auto it = hamt.begin(); it != hamt.end(); ++it) {
      auto pair = *it;
      result[pair.first] = pair.second;
    }
    return result;
  }

  IntHAMT InsertWithHash(IntHAMT& hamt, int key, int value, size_t hash) {
    return IntHAMT(hamt.InsertRec(zone(), hamt.root_, key, value, hash, 0));
  }

  const int* FindWithHash(IntHAMT& hamt, int key, size_t hash) const {
    return hamt.FindHelper(hamt.root_, key, hash, 0);
  }
};

TEST_F(HAMTTest, EmptyState) {
  IntHAMT hamt;

  EXPECT_EQ(hamt.begin(), hamt.end());
  EXPECT_EQ(hamt.find(100), nullptr);
}

TEST_F(HAMTTest, BasicInsertAndFind) {
  IntHAMT h0;

  // Insert first element
  IntHAMT h1 = h0.insert(zone(), 1, 100);

  // Verify h1 has it
  const int* val = h1.find(1);
  ASSERT_NE(val, nullptr);
  EXPECT_EQ(*val, 100);

  // Verify h0 is still empty (Persistence/Immutability)
  EXPECT_EQ(h0.find(1), nullptr);

  // Insert second element
  IntHAMT h2 = h1.insert(zone(), 2, 200);

  EXPECT_EQ(*h2.find(1), 100);
  EXPECT_EQ(*h2.find(2), 200);

  // Verify h1 didn't change
  EXPECT_EQ(h1.find(2), nullptr);
}

TEST_F(HAMTTest, OverwriteValues) {
  IntHAMT h0;
  h0 = h0.insert(zone(), 42, 100);

  // Overwrite key 42 with 999
  IntHAMT h1 = h0.insert(zone(), 42, 999);

  EXPECT_EQ(*h0.find(42), 100);  // Old tree unchanged
  EXPECT_EQ(*h1.find(42), 999);  // New tree updated
}

TEST_F(HAMTTest, LargeInsertionToTriggerBranching) {
  // Insert enough items to force creation of Branch nodes and multiple levels.
  // The 5-bit window means 32 items could potentially fill one node,
  // so we go higher to force depth.
  IntHAMT hamt;
  std::map<int, int> expected;

  for (int i = 0; i < 1000; ++i) {
    hamt = hamt.insert(zone(), i, i * 2);
    expected[i] = i * 2;
  }

  // Verify all items are present
  for (const auto& kv : expected) {
    const int* val = hamt.find(kv.first);
    ASSERT_NE(val, nullptr) << "Failed to find key " << kv.first;
    EXPECT_EQ(*val, kv.second);
  }

  // Verify Iterator covers everything
  std::map<int, int> actual = ToMap(hamt);
  EXPECT_EQ(expected, actual);
}

TEST_F(HAMTTest, ForceHashCollision_Chaining) {
  // Scenario: Two different keys have the EXACT same hash.
  // Expectation: They should live in the same Leaf node via the 'next' pointer.
  // We don't have access to internal Node pointers easily, but we verify
  // that both keys are retrievable despite the collision.

  IntHAMT hamt;
  size_t kFixedHash = 0xDEADBEEF;

  // Insert Key 1
  hamt = InsertWithHash(hamt, 1, 10, kFixedHash);

  // Insert Key 2 with SAME hash
  hamt = InsertWithHash(hamt, 2, 20, kFixedHash);

  // Verify both exist
  const int* v1 = FindWithHash(hamt, 1, kFixedHash);
  const int* v2 = FindWithHash(hamt, 2, kFixedHash);

  ASSERT_NE(v1, nullptr);
  ASSERT_NE(v2, nullptr);
  EXPECT_EQ(*v1, 10);
  EXPECT_EQ(*v2, 20);

  // Verify non-existent key with same hash returns null
  EXPECT_EQ(FindWithHash(hamt, 3, kFixedHash), nullptr);
}

TEST_F(HAMTTest, ForceHashCollision_BranchSplit) {
  // Scenario: Two keys collide on the first 5 bits (Level 0),
  // but differ on the next 5 bits (Level 1).
  // Expectation: The HAMT should grow a new Branch node at Level 1.

  IntHAMT hamt;

  // Hash A: ...00000 00000 (0)
  size_t hashA = 0;

  // Hash B: ...00001 00000 (32)
  // Level 0 (bits 0-4): 0 (Same as hashA -> Collision)
  // Level 1 (bits 5-9): 1 (Different -> Split)
  size_t hashB = 1 << 5;

  hamt = InsertWithHash(hamt, 1, 10, hashA);
  hamt = InsertWithHash(hamt, 2, 20, hashB);

  // Both should be found
  EXPECT_EQ(*FindWithHash(hamt, 1, hashA), 10);
  EXPECT_EQ(*FindWithHash(hamt, 2, hashB), 20);
}

TEST_F(HAMTTest, ForceHashCollision_DeepRecursion) {
  // Scenario: Force a collision that persists for several levels
  // to ensure the recursive InsertRec logic works.

  IntHAMT h0;

  // Both hashes are 0 for the first 10 bits (Level 0 and Level 1).
  // They diverge at Level 2 (bits 10-14).
  size_t hashA = 0;
  size_t hashB = 1 << 10;  // 1024

  IntHAMT h1 = InsertWithHash(h0, 1, 100, hashA);
  IntHAMT h2 = InsertWithHash(h1, 2, 200, hashB);

  EXPECT_EQ(FindWithHash(h0, 1, hashA), nullptr);
  EXPECT_EQ(FindWithHash(h0, 2, hashB), nullptr);

  EXPECT_EQ(*FindWithHash(h1, 1, hashA), 100);
  EXPECT_EQ(FindWithHash(h1, 2, hashB), nullptr);

  EXPECT_EQ(*FindWithHash(h2, 1, hashA), 100);
  EXPECT_EQ(*FindWithHash(h2, 2, hashB), 200);
}

TEST_F(HAMTTest, UpdateInCollisionChain) {
  // Scenario: We have a linked list of collisions. We update one of them.
  // This verifies that `InsertInCollisionListRec` handles immutable updates
  // correctly (copying the path down the linked list).

  IntHAMT h0;
  size_t kHash = 0x555;

  // Create chain: [Key1] -> [Key2] -> [Key3]
  h0 = InsertWithHash(h0, 1, 10, kHash);
  h0 = InsertWithHash(h0, 2, 20, kHash);
  h0 = InsertWithHash(h0, 3, 30, kHash);

  // Update Key 2 (middle of chain)
  IntHAMT h1 = InsertWithHash(h0, 2, 222, kHash);

  // Verify h1 (New Tree)
  EXPECT_EQ(*FindWithHash(h1, 1, kHash), 10);   // Should still be found
  EXPECT_EQ(*FindWithHash(h1, 2, kHash), 222);  // Updated
  EXPECT_EQ(*FindWithHash(h1, 3, kHash), 30);   // Should still be found

  // Verify h0 (Old Tree) - Persistence Check
  const int* old_val = FindWithHash(h0, 2, kHash);
  ASSERT_NE(old_val, nullptr);
  EXPECT_EQ(*old_val, 20);  // Should be original value
}

TEST_F(HAMTTest, MergeIntoIsLeftJoin) {
  // Left Join (Keep all from A, update from B).

  IntHAMT hA;
  hA = hA.insert(zone(), 1, 10);  // In A, not B
  hA = hA.insert(zone(), 2, 20);  // In A and B (Conflict)

  IntHAMT hB;
  hB = hB.insert(zone(), 2, 200);  // In A and B
  hB = hB.insert(zone(), 3, 300);  // In B, not A

  auto summer = [](int a, int b) { return a + b; };

  // Perform merge: Result = A merge B
  IntHAMT result = hA.merge_into(zone(), hB, summer);

  // 1 was in A, not B. Should remain 10.
  ASSERT_NE(result.find(1), nullptr);
  EXPECT_EQ(*result.find(1), 10);

  // 2 was in both. Should be 20 + 200 = 220.
  ASSERT_NE(result.find(2), nullptr);
  EXPECT_EQ(*result.find(2), 220);

  // 3 was in B only. Since implementation iterates mapA (LHS),
  // it should NOT be in the result.
  EXPECT_EQ(result.find(3), nullptr);
}

TEST_F(HAMTTest, IteratorTraversal) {
  IntHAMT hamt;
  hamt = hamt.insert(zone(), 10, 1);
  hamt = hamt.insert(zone(), 20, 2);
  hamt = hamt.insert(zone(), 30, 3);

  int count = 0;
  int sum_keys = 0;
  for (auto it = hamt.begin(); it != hamt.end(); ++it) {
    count++;
    sum_keys += (*it).first;
  }

  EXPECT_EQ(count, 3);
  EXPECT_EQ(sum_keys, 10 + 20 + 30);
}

TEST_F(HAMTTest, IteratorDeepStructure) {
  // Force a deep structure and ensure iterator doesn't crash or miss items
  IntHAMT hamt;
  const int kNumItems = 2000;
  for (int i = 0; i < kNumItems; i++) {
    hamt = hamt.insert(zone(), i, i);
  }

  int items_seen = 0;
  for (auto it = hamt.begin(); it != hamt.end(); ++it) {
    items_seen++;
    // Verify key integrity
    EXPECT_EQ((*it).first, (*it).second);
  }
  EXPECT_EQ(items_seen, kNumItems);
}

}  // namespace maglev
}  // namespace internal
}  // namespace v8
