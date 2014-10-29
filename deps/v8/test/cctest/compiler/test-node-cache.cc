// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#include "graph-tester.h"
#include "src/compiler/common-operator.h"
#include "src/compiler/node-cache.h"

using namespace v8::internal;
using namespace v8::internal::compiler;

TEST(Int32Constant_back_to_back) {
  GraphTester graph;
  Int32NodeCache cache;

  for (int i = -2000000000; i < 2000000000; i += 3315177) {
    Node** pos = cache.Find(graph.zone(), i);
    CHECK_NE(NULL, pos);
    for (int j = 0; j < 3; j++) {
      Node** npos = cache.Find(graph.zone(), i);
      CHECK_EQ(pos, npos);
    }
  }
}


TEST(Int32Constant_five) {
  GraphTester graph;
  Int32NodeCache cache;
  CommonOperatorBuilder common(graph.zone());

  int32_t constants[] = {static_cast<int32_t>(0x80000000), -77, 0, 1, -1};

  Node* nodes[arraysize(constants)];

  for (size_t i = 0; i < arraysize(constants); i++) {
    int32_t k = constants[i];
    Node* node = graph.NewNode(common.Int32Constant(k));
    *cache.Find(graph.zone(), k) = nodes[i] = node;
  }

  for (size_t i = 0; i < arraysize(constants); i++) {
    int32_t k = constants[i];
    CHECK_EQ(nodes[i], *cache.Find(graph.zone(), k));
  }
}


TEST(Int32Constant_hits) {
  GraphTester graph;
  Int32NodeCache cache;
  const int32_t kSize = 1500;
  Node** nodes = graph.zone()->NewArray<Node*>(kSize);
  CommonOperatorBuilder common(graph.zone());

  for (int i = 0; i < kSize; i++) {
    int32_t v = i * -55;
    nodes[i] = graph.NewNode(common.Int32Constant(v));
    *cache.Find(graph.zone(), v) = nodes[i];
  }

  int hits = 0;
  for (int i = 0; i < kSize; i++) {
    int32_t v = i * -55;
    Node** pos = cache.Find(graph.zone(), v);
    if (*pos != NULL) {
      CHECK_EQ(nodes[i], *pos);
      hits++;
    }
  }
  CHECK_LT(4, hits);
}


TEST(Int64Constant_back_to_back) {
  GraphTester graph;
  Int64NodeCache cache;

  for (int64_t i = -2000000000; i < 2000000000; i += 3315177) {
    Node** pos = cache.Find(graph.zone(), i);
    CHECK_NE(NULL, pos);
    for (int j = 0; j < 3; j++) {
      Node** npos = cache.Find(graph.zone(), i);
      CHECK_EQ(pos, npos);
    }
  }
}


TEST(Int64Constant_hits) {
  GraphTester graph;
  Int64NodeCache cache;
  const int32_t kSize = 1500;
  Node** nodes = graph.zone()->NewArray<Node*>(kSize);
  CommonOperatorBuilder common(graph.zone());

  for (int i = 0; i < kSize; i++) {
    int64_t v = static_cast<int64_t>(i) * static_cast<int64_t>(5003001);
    nodes[i] = graph.NewNode(common.Int32Constant(i));
    *cache.Find(graph.zone(), v) = nodes[i];
  }

  int hits = 0;
  for (int i = 0; i < kSize; i++) {
    int64_t v = static_cast<int64_t>(i) * static_cast<int64_t>(5003001);
    Node** pos = cache.Find(graph.zone(), v);
    if (*pos != NULL) {
      CHECK_EQ(nodes[i], *pos);
      hits++;
    }
  }
  CHECK_LT(4, hits);
}


TEST(PtrConstant_back_to_back) {
  GraphTester graph;
  PtrNodeCache cache;
  int32_t buffer[50];

  for (int32_t* p = buffer;
       (p - buffer) < static_cast<ptrdiff_t>(arraysize(buffer)); p++) {
    Node** pos = cache.Find(graph.zone(), p);
    CHECK_NE(NULL, pos);
    for (int j = 0; j < 3; j++) {
      Node** npos = cache.Find(graph.zone(), p);
      CHECK_EQ(pos, npos);
    }
  }
}


TEST(PtrConstant_hits) {
  GraphTester graph;
  PtrNodeCache cache;
  const int32_t kSize = 50;
  int32_t buffer[kSize];
  Node* nodes[kSize];
  CommonOperatorBuilder common(graph.zone());

  for (size_t i = 0; i < arraysize(buffer); i++) {
    int k = static_cast<int>(i);
    int32_t* p = &buffer[i];
    nodes[i] = graph.NewNode(common.Int32Constant(k));
    *cache.Find(graph.zone(), p) = nodes[i];
  }

  int hits = 0;
  for (size_t i = 0; i < arraysize(buffer); i++) {
    int32_t* p = &buffer[i];
    Node** pos = cache.Find(graph.zone(), p);
    if (*pos != NULL) {
      CHECK_EQ(nodes[i], *pos);
      hits++;
    }
  }
  CHECK_LT(4, hits);
}
