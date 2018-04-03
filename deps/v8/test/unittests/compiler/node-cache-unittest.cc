// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/node-cache.h"
#include "test/unittests/compiler/graph-unittest.h"
#include "test/unittests/test-utils.h"
#include "testing/gmock-support.h"

using testing::Contains;

namespace v8 {
namespace internal {
namespace compiler {
namespace node_cache_unittest {

typedef GraphTest NodeCacheTest;

TEST_F(NodeCacheTest, Int32Constant_back_to_back) {
  Int32NodeCache cache;

  for (int i = -2000000000; i < 2000000000; i += 3315177) {
    Node** pos = cache.Find(zone(), i);
    ASSERT_TRUE(pos != nullptr);
    for (int j = 0; j < 3; j++) {
      Node** npos = cache.Find(zone(), i);
      EXPECT_EQ(pos, npos);
    }
  }
}


TEST_F(NodeCacheTest, Int32Constant_five) {
  Int32NodeCache cache;
  int32_t constants[] = {static_cast<int32_t>(0x80000000), -77, 0, 1, -1};
  Node* nodes[arraysize(constants)];

  for (size_t i = 0; i < arraysize(constants); i++) {
    int32_t k = constants[i];
    Node* node = graph()->NewNode(common()->Int32Constant(k));
    *cache.Find(zone(), k) = nodes[i] = node;
  }

  for (size_t i = 0; i < arraysize(constants); i++) {
    int32_t k = constants[i];
    EXPECT_EQ(nodes[i], *cache.Find(zone(), k));
  }
}


TEST_F(NodeCacheTest, Int32Constant_hits) {
  Int32NodeCache cache;
  const int32_t kSize = 1500;
  Node** nodes = zone()->NewArray<Node*>(kSize);

  for (int i = 0; i < kSize; i++) {
    int32_t v = i * -55;
    nodes[i] = graph()->NewNode(common()->Int32Constant(v));
    *cache.Find(zone(), v) = nodes[i];
  }

  int hits = 0;
  for (int i = 0; i < kSize; i++) {
    int32_t v = i * -55;
    Node** pos = cache.Find(zone(), v);
    if (*pos != NULL) {
      EXPECT_EQ(nodes[i], *pos);
      hits++;
    }
  }
  EXPECT_LT(4, hits);
}


TEST_F(NodeCacheTest, Int64Constant_back_to_back) {
  Int64NodeCache cache;

  for (int64_t i = -2000000000; i < 2000000000; i += 3315177) {
    Node** pos = cache.Find(zone(), i);
    ASSERT_TRUE(pos != nullptr);
    for (int j = 0; j < 3; j++) {
      Node** npos = cache.Find(zone(), i);
      EXPECT_EQ(pos, npos);
    }
  }
}


TEST_F(NodeCacheTest, Int64Constant_hits) {
  Int64NodeCache cache;
  const int32_t kSize = 1500;
  Node** nodes = zone()->NewArray<Node*>(kSize);

  for (int i = 0; i < kSize; i++) {
    int64_t v = static_cast<int64_t>(i) * static_cast<int64_t>(5003001);
    nodes[i] = graph()->NewNode(common()->Int32Constant(i));
    *cache.Find(zone(), v) = nodes[i];
  }

  int hits = 0;
  for (int i = 0; i < kSize; i++) {
    int64_t v = static_cast<int64_t>(i) * static_cast<int64_t>(5003001);
    Node** pos = cache.Find(zone(), v);
    if (*pos != NULL) {
      EXPECT_EQ(nodes[i], *pos);
      hits++;
    }
  }
  EXPECT_LT(4, hits);
}


TEST_F(NodeCacheTest, GetCachedNodes_int32) {
  Int32NodeCache cache;
  int32_t constants[] = {0, 311, 12,  13,  14,  555, -555, -44, -33, -22, -11,
                         0, 311, 311, 412, 412, 11,  11,   -33, -33, -22, -11};

  for (size_t i = 0; i < arraysize(constants); i++) {
    int32_t k = constants[i];
    Node** pos = cache.Find(zone(), k);
    if (*pos != NULL) {
      ZoneVector<Node*> nodes(zone());
      cache.GetCachedNodes(&nodes);
      EXPECT_THAT(nodes, Contains(*pos));
    } else {
      ZoneVector<Node*> nodes(zone());
      Node* n = graph()->NewNode(common()->Int32Constant(k));
      *pos = n;
      cache.GetCachedNodes(&nodes);
      EXPECT_THAT(nodes, Contains(n));
    }
  }
}


TEST_F(NodeCacheTest, GetCachedNodes_int64) {
  Int64NodeCache cache;
  int64_t constants[] = {0, 311, 12,  13,  14,  555, -555, -44, -33, -22, -11,
                         0, 311, 311, 412, 412, 11,  11,   -33, -33, -22, -11};

  for (size_t i = 0; i < arraysize(constants); i++) {
    int64_t k = constants[i];
    Node** pos = cache.Find(zone(), k);
    if (*pos != NULL) {
      ZoneVector<Node*> nodes(zone());
      cache.GetCachedNodes(&nodes);
      EXPECT_THAT(nodes, Contains(*pos));
    } else {
      ZoneVector<Node*> nodes(zone());
      Node* n = graph()->NewNode(common()->Int64Constant(k));
      *pos = n;
      cache.GetCachedNodes(&nodes);
      EXPECT_THAT(nodes, Contains(n));
    }
  }
}

}  // namespace node_cache_unittest
}  // namespace compiler
}  // namespace internal
}  // namespace v8
