// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iterator>

#include "src/v8.h"

#include "src/base/threaded-list.h"
#include "testing/gtest-support.h"

namespace v8 {
namespace base {

struct ThreadedListTestNode {
  ThreadedListTestNode() : next_(nullptr), other_next_(nullptr) {}

  ThreadedListTestNode** next() { return &next_; }

  ThreadedListTestNode* next_;

  struct OtherTraits {
    static ThreadedListTestNode** next(ThreadedListTestNode* t) {
      return t->other_next();
    }
  };

  ThreadedListTestNode** other_next() { return &other_next_; }

  ThreadedListTestNode* other_next_;
};

struct ThreadedListTest : public ::testing::Test {
  static const size_t INIT_NODES = 5;
  ThreadedListTest() {}

  void SetUp() override {
    for (size_t i = 0; i < INIT_NODES; i++) {
      nodes[i] = ThreadedListTestNode();
    }

    for (size_t i = 0; i < INIT_NODES; i++) {
      list.Add(&nodes[i]);
      normal_next_list.Add(&nodes[i]);
    }

    // Verify if setup worked
    CHECK(list.Verify());
    CHECK_EQ(list.LengthForTest(), INIT_NODES);
    CHECK(normal_next_list.Verify());
    CHECK_EQ(normal_next_list.LengthForTest(), INIT_NODES);

    extra_test_node_0 = ThreadedListTestNode();
    extra_test_node_1 = ThreadedListTestNode();
    extra_test_node_2 = ThreadedListTestNode();

    extra_test_list.Add(&extra_test_node_0);
    extra_test_list.Add(&extra_test_node_1);
    extra_test_list.Add(&extra_test_node_2);
    CHECK_EQ(extra_test_list.LengthForTest(), 3);
    CHECK(extra_test_list.Verify());

    normal_extra_test_list.Add(&extra_test_node_0);
    normal_extra_test_list.Add(&extra_test_node_1);
    normal_extra_test_list.Add(&extra_test_node_2);
    CHECK_EQ(normal_extra_test_list.LengthForTest(), 3);
    CHECK(normal_extra_test_list.Verify());
  }

  void TearDown() override {
    // Check if the normal list threaded through next is still untouched.
    CHECK(normal_next_list.Verify());
    CHECK_EQ(normal_next_list.LengthForTest(), INIT_NODES);
    CHECK_EQ(normal_next_list.AtForTest(0), &nodes[0]);
    CHECK_EQ(normal_next_list.AtForTest(4), &nodes[4]);
    CHECK(normal_extra_test_list.Verify());
    CHECK_EQ(normal_extra_test_list.LengthForTest(), 3);
    CHECK_EQ(normal_extra_test_list.AtForTest(0), &extra_test_node_0);
    CHECK_EQ(normal_extra_test_list.AtForTest(2), &extra_test_node_2);

    list.Clear();
    extra_test_list.Clear();
  }

  ThreadedListTestNode nodes[INIT_NODES];
  ThreadedList<ThreadedListTestNode, ThreadedListTestNode::OtherTraits> list;
  ThreadedList<ThreadedListTestNode> normal_next_list;

  ThreadedList<ThreadedListTestNode, ThreadedListTestNode::OtherTraits>
      extra_test_list;
  ThreadedList<ThreadedListTestNode> normal_extra_test_list;
  ThreadedListTestNode extra_test_node_0;
  ThreadedListTestNode extra_test_node_1;
  ThreadedListTestNode extra_test_node_2;
};

TEST_F(ThreadedListTest, Add) {
  CHECK_EQ(list.LengthForTest(), 5);
  ThreadedListTestNode new_node;
  // Add to existing list
  list.Add(&new_node);
  list.Verify();
  CHECK_EQ(list.LengthForTest(), 6);
  CHECK_EQ(list.AtForTest(5), &new_node);

  list.Clear();
  CHECK_EQ(list.LengthForTest(), 0);

  new_node = ThreadedListTestNode();
  // Add to empty list
  list.Add(&new_node);
  list.Verify();
  CHECK_EQ(list.LengthForTest(), 1);
  CHECK_EQ(list.AtForTest(0), &new_node);
}

TEST_F(ThreadedListTest, AddFront) {
  CHECK_EQ(list.LengthForTest(), 5);
  ThreadedListTestNode new_node;
  // AddFront to existing list
  list.AddFront(&new_node);
  list.Verify();
  CHECK_EQ(list.LengthForTest(), 6);
  CHECK_EQ(list.first(), &new_node);

  list.Clear();
  CHECK_EQ(list.LengthForTest(), 0);

  new_node = ThreadedListTestNode();
  // AddFront to empty list
  list.AddFront(&new_node);
  list.Verify();
  CHECK_EQ(list.LengthForTest(), 1);
  CHECK_EQ(list.first(), &new_node);
}

TEST_F(ThreadedListTest, ReinitializeHead) {
  CHECK_EQ(list.LengthForTest(), 5);
  CHECK_NE(extra_test_list.first(), list.first());
  list.ReinitializeHead(&extra_test_node_0);
  list.Verify();
  CHECK_EQ(extra_test_list.first(), list.first());
  CHECK_EQ(extra_test_list.end(), list.end());
  CHECK_EQ(extra_test_list.LengthForTest(), 3);
}

TEST_F(ThreadedListTest, DropHead) {
  CHECK_EQ(extra_test_list.LengthForTest(), 3);
  CHECK_EQ(extra_test_list.first(), &extra_test_node_0);
  extra_test_list.DropHead();
  extra_test_list.Verify();
  CHECK_EQ(extra_test_list.first(), &extra_test_node_1);
  CHECK_EQ(extra_test_list.LengthForTest(), 2);
}

TEST_F(ThreadedListTest, Append) {
  auto initial_extra_list_end = extra_test_list.end();
  CHECK_EQ(list.LengthForTest(), 5);
  list.Append(std::move(extra_test_list));
  list.Verify();
  extra_test_list.Verify();
  CHECK(extra_test_list.is_empty());
  CHECK_EQ(list.LengthForTest(), 8);
  CHECK_EQ(list.AtForTest(4), &nodes[4]);
  CHECK_EQ(list.AtForTest(5), &extra_test_node_0);
  CHECK_EQ(list.end(), initial_extra_list_end);
}

TEST_F(ThreadedListTest, Prepend) {
  CHECK_EQ(list.LengthForTest(), 5);
  list.Prepend(std::move(extra_test_list));
  list.Verify();
  extra_test_list.Verify();
  CHECK(extra_test_list.is_empty());
  CHECK_EQ(list.LengthForTest(), 8);
  CHECK_EQ(list.first(), &extra_test_node_0);
  CHECK_EQ(list.AtForTest(2), &extra_test_node_2);
  CHECK_EQ(list.AtForTest(3), &nodes[0]);
}

TEST_F(ThreadedListTest, Clear) {
  CHECK_NE(list.LengthForTest(), 0);
  list.Clear();
  CHECK_EQ(list.LengthForTest(), 0);
  CHECK_NULL(list.first());
}

TEST_F(ThreadedListTest, MoveAssign) {
  ThreadedList<ThreadedListTestNode, ThreadedListTestNode::OtherTraits> m_list;
  CHECK_EQ(extra_test_list.LengthForTest(), 3);
  m_list = std::move(extra_test_list);

  m_list.Verify();
  CHECK_EQ(m_list.first(), &extra_test_node_0);
  CHECK_EQ(m_list.LengthForTest(), 3);

  // move assign from empty list
  extra_test_list.Clear();
  CHECK_EQ(extra_test_list.LengthForTest(), 0);
  m_list = std::move(extra_test_list);
  CHECK_EQ(m_list.LengthForTest(), 0);

  m_list.Verify();
  CHECK_NULL(m_list.first());
}

TEST_F(ThreadedListTest, MoveCtor) {
  CHECK_EQ(extra_test_list.LengthForTest(), 3);
  ThreadedList<ThreadedListTestNode, ThreadedListTestNode::OtherTraits> m_list(
      std::move(extra_test_list));

  m_list.Verify();
  CHECK_EQ(m_list.LengthForTest(), 3);
  CHECK_EQ(m_list.first(), &extra_test_node_0);

  // move construct from empty list
  extra_test_list.Clear();
  CHECK_EQ(extra_test_list.LengthForTest(), 0);
  ThreadedList<ThreadedListTestNode, ThreadedListTestNode::OtherTraits> m_list2(
      std::move(extra_test_list));
  CHECK_EQ(m_list2.LengthForTest(), 0);

  m_list2.Verify();
  CHECK_NULL(m_list2.first());
}

TEST_F(ThreadedListTest, Remove) {
  CHECK_EQ(list.LengthForTest(), 5);

  // Remove first
  CHECK_EQ(list.first(), &nodes[0]);
  list.Remove(&nodes[0]);
  list.Verify();
  CHECK_EQ(list.first(), &nodes[1]);
  CHECK_EQ(list.LengthForTest(), 4);

  // Remove middle
  list.Remove(&nodes[2]);
  list.Verify();
  CHECK_EQ(list.LengthForTest(), 3);
  CHECK_EQ(list.first(), &nodes[1]);
  CHECK_EQ(list.AtForTest(1), &nodes[3]);

  // Remove last
  list.Remove(&nodes[4]);
  list.Verify();
  CHECK_EQ(list.LengthForTest(), 2);
  CHECK_EQ(list.first(), &nodes[1]);
  CHECK_EQ(list.AtForTest(1), &nodes[3]);

  // Remove rest
  list.Remove(&nodes[1]);
  list.Remove(&nodes[3]);
  list.Verify();
  CHECK_EQ(list.LengthForTest(), 0);

  // Remove not found
  list.Remove(&nodes[4]);
  list.Verify();
  CHECK_EQ(list.LengthForTest(), 0);
}

TEST_F(ThreadedListTest, Rewind) {
  CHECK_EQ(extra_test_list.LengthForTest(), 3);
  for (auto iter = extra_test_list.begin(); iter != extra_test_list.end();
       ++iter) {
    if (*iter == &extra_test_node_2) {
      extra_test_list.Rewind(iter);
      break;
    }
  }
  CHECK_EQ(extra_test_list.LengthForTest(), 2);
  auto iter = extra_test_list.begin();
  CHECK_EQ(*iter, &extra_test_node_0);
  std::advance(iter, 1);
  CHECK_EQ(*iter, &extra_test_node_1);

  extra_test_list.Rewind(extra_test_list.begin());
  CHECK_EQ(extra_test_list.LengthForTest(), 0);
}

TEST_F(ThreadedListTest, IterComp) {
  ThreadedList<ThreadedListTestNode, ThreadedListTestNode::OtherTraits> c_list =
      std::move(extra_test_list);
  bool found_first;
  for (auto iter = c_list.begin(); iter != c_list.end(); ++iter) {
    // This triggers the operator== on the iterator
    if (iter == c_list.begin()) {
      found_first = true;
    }
  }
  CHECK(found_first);
}

TEST_F(ThreadedListTest, ConstIterComp) {
  const ThreadedList<ThreadedListTestNode, ThreadedListTestNode::OtherTraits>
      c_list = std::move(extra_test_list);
  bool found_first;
  for (auto iter = c_list.begin(); iter != c_list.end(); ++iter) {
    // This triggers the operator== on the iterator
    if (iter == c_list.begin()) {
      found_first = true;
    }
  }
  CHECK(found_first);
}

}  // namespace base
}  // namespace v8
