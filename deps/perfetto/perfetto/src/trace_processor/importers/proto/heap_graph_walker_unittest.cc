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

#include "src/trace_processor/importers/proto/heap_graph_walker.h"

#include "perfetto/base/logging.h"
#include "test/gtest_and_gmock.h"

namespace perfetto {
namespace trace_processor {
namespace {

using ::testing::UnorderedElementsAre;
using ::testing::UnorderedElementsAreArray;

class HeapGraphWalkerTestDelegate : public HeapGraphWalker::Delegate {
 public:
  ~HeapGraphWalkerTestDelegate() override = default;

  void MarkReachable(int64_t row) override { reachable_.emplace(row); }

  void SetRetained(int64_t row,
                   int64_t retained,
                   int64_t unique_retained) override {
    bool inserted;
    std::tie(std::ignore, inserted) = retained_.emplace(row, retained);
    PERFETTO_CHECK(inserted);
    std::tie(std::ignore, inserted) =
        unique_retained_.emplace(row, unique_retained);
    PERFETTO_CHECK(inserted);
  }

  bool Reachable(int64_t row) {
    return reachable_.find(row) != reachable_.end();
  }

  int64_t Retained(int64_t row) {
    auto it = retained_.find(row);
    PERFETTO_CHECK(it != retained_.end());
    return it->second;
  }

  int64_t UniqueRetained(int64_t row) {
    auto it = unique_retained_.find(row);
    PERFETTO_CHECK(it != unique_retained_.end());
    return it->second;
  }

 private:
  std::map<int64_t, int64_t> retained_;
  std::map<int64_t, int64_t> unique_retained_;
  std::set<int64_t> reachable_;
};

//     1     |
//    ^^     |
//   /  \    |
//   2   3   |
//   ^   ^   |
//    \ /    |
//     4R    |
TEST(HeapGraphWalkerTest, Diamond) {
  HeapGraphWalkerTestDelegate delegate;
  HeapGraphWalker walker(&delegate);
  walker.AddNode(1, 1);
  walker.AddNode(2, 2);
  walker.AddNode(3, 3);
  walker.AddNode(4, 4);

  walker.AddEdge(2, 1);
  walker.AddEdge(3, 1);
  walker.AddEdge(4, 2);
  walker.AddEdge(4, 3);

  walker.MarkRoot(4);
  walker.CalculateRetained();

  EXPECT_EQ(delegate.Retained(1), 1);
  EXPECT_EQ(delegate.Retained(2), 3);
  EXPECT_EQ(delegate.Retained(3), 4);
  EXPECT_EQ(delegate.Retained(4), 10);

  EXPECT_EQ(delegate.UniqueRetained(1), 1);
  EXPECT_EQ(delegate.UniqueRetained(2), 2);
  EXPECT_EQ(delegate.UniqueRetained(3), 3);
  EXPECT_EQ(delegate.UniqueRetained(4), 10);
}

// 1       2  |
// ^       ^  |
//  \     /   |
//  3R<->4    |
TEST(HeapGraphWalkerTest, Loop) {
  HeapGraphWalkerTestDelegate delegate;
  HeapGraphWalker walker(&delegate);
  walker.AddNode(1, 1);
  walker.AddNode(2, 2);
  walker.AddNode(3, 3);
  walker.AddNode(4, 4);

  walker.AddEdge(3, 1);
  walker.AddEdge(3, 4);
  walker.AddEdge(4, 2);
  walker.AddEdge(4, 3);

  walker.MarkRoot(3);
  walker.CalculateRetained();

  EXPECT_EQ(delegate.Retained(1), 1);
  EXPECT_EQ(delegate.Retained(2), 2);
  EXPECT_EQ(delegate.Retained(3), 10);
  EXPECT_EQ(delegate.Retained(4), 10);

  EXPECT_EQ(delegate.UniqueRetained(1), 1);
  EXPECT_EQ(delegate.UniqueRetained(2), 2);
  EXPECT_EQ(delegate.UniqueRetained(3), 4);
  EXPECT_EQ(delegate.UniqueRetained(4), 6);
}

//    1R    |
//    ^\    |
//   /  v   |
//   3<-2   |
TEST(HeapGraphWalkerTest, Triangle) {
  HeapGraphWalkerTestDelegate delegate;
  HeapGraphWalker walker(&delegate);
  walker.AddNode(1, 1);
  walker.AddNode(2, 2);
  walker.AddNode(3, 3);

  walker.AddEdge(1, 2);
  walker.AddEdge(2, 3);
  walker.AddEdge(3, 1);

  walker.MarkRoot(1);
  walker.CalculateRetained();

  EXPECT_EQ(delegate.Retained(1), 6);
  EXPECT_EQ(delegate.Retained(2), 6);
  EXPECT_EQ(delegate.Retained(3), 6);

  EXPECT_EQ(delegate.UniqueRetained(1), 1);
  EXPECT_EQ(delegate.UniqueRetained(2), 2);
  EXPECT_EQ(delegate.UniqueRetained(3), 3);
}

// 1      |
// ^      |
// |      |
// 2   4  |
// ^   ^  |
// |   |  |
// 3R  5  |
TEST(HeapGraphWalkerTest, Disconnected) {
  HeapGraphWalkerTestDelegate delegate;
  HeapGraphWalker walker(&delegate);
  walker.AddNode(1, 1);
  walker.AddNode(2, 2);
  walker.AddNode(3, 3);
  walker.AddNode(4, 4);
  walker.AddNode(5, 5);

  walker.AddEdge(2, 1);
  walker.AddEdge(3, 2);
  walker.AddEdge(5, 4);

  walker.MarkRoot(3);
  walker.CalculateRetained();

  EXPECT_EQ(delegate.Retained(1), 1);
  EXPECT_EQ(delegate.Retained(2), 3);
  EXPECT_EQ(delegate.Retained(3), 6);

  EXPECT_EQ(delegate.UniqueRetained(1), 1);
  EXPECT_EQ(delegate.UniqueRetained(2), 3);
  EXPECT_EQ(delegate.UniqueRetained(3), 6);

  EXPECT_TRUE(delegate.Reachable(1));
  EXPECT_TRUE(delegate.Reachable(2));
  EXPECT_TRUE(delegate.Reachable(3));
  EXPECT_FALSE(delegate.Reachable(4));
  EXPECT_FALSE(delegate.Reachable(5));
}

//      1      |
//      ^^     |
//     / \     |
//    2   3    |
//    ^  ^^    |
//    |/  |    |
//    4   5    |
//    ^   ^    |
//    \  /     |
//      6R     |
TEST(HeapGraphWalkerTest, Complex) {
  HeapGraphWalkerTestDelegate delegate;
  HeapGraphWalker walker(&delegate);
  walker.AddNode(1, 1);
  walker.AddNode(2, 2);
  walker.AddNode(3, 3);
  walker.AddNode(4, 4);
  walker.AddNode(5, 5);
  walker.AddNode(6, 6);

  walker.AddEdge(2, 1);
  walker.AddEdge(3, 1);
  walker.AddEdge(4, 2);
  walker.AddEdge(4, 3);
  walker.AddEdge(5, 3);
  walker.AddEdge(6, 4);
  walker.AddEdge(6, 5);

  walker.MarkRoot(6);
  walker.CalculateRetained();

  EXPECT_EQ(delegate.Retained(1), 1);
  EXPECT_EQ(delegate.Retained(2), 3);
  EXPECT_EQ(delegate.Retained(3), 4);
  EXPECT_EQ(delegate.Retained(4), 10);
  EXPECT_EQ(delegate.Retained(5), 9);
  EXPECT_EQ(delegate.Retained(6), 21);

  EXPECT_EQ(delegate.UniqueRetained(1), 1);
  EXPECT_EQ(delegate.UniqueRetained(2), 2);
  EXPECT_EQ(delegate.UniqueRetained(3), 3);
  EXPECT_EQ(delegate.UniqueRetained(4), 6);
  EXPECT_EQ(delegate.UniqueRetained(5), 5);
  EXPECT_EQ(delegate.UniqueRetained(6), 21);
}

//    1      |
//    ^^     |
//   /  \    |
//  2<-> 3   |
//  ^        |
//  |        |
//  4R       |
TEST(HeapGraphWalkerTest, SharedInComponent) {
  HeapGraphWalkerTestDelegate delegate;
  HeapGraphWalker walker(&delegate);
  walker.AddNode(1, 1);
  walker.AddNode(2, 2);
  walker.AddNode(3, 3);
  walker.AddNode(4, 4);

  walker.AddEdge(2, 1);
  walker.AddEdge(2, 3);
  walker.AddEdge(3, 1);
  walker.AddEdge(3, 2);
  walker.AddEdge(4, 2);

  walker.MarkRoot(4);
  walker.CalculateRetained();

  EXPECT_EQ(delegate.Retained(1), 1);
  EXPECT_EQ(delegate.Retained(2), 6);
  EXPECT_EQ(delegate.Retained(3), 6);
  EXPECT_EQ(delegate.Retained(4), 10);

  EXPECT_EQ(delegate.UniqueRetained(1), 1);
  // TODO(fmayer): this should be 6, as it breaks away the component.
  EXPECT_EQ(delegate.UniqueRetained(2), 2);
  EXPECT_EQ(delegate.UniqueRetained(3), 3);
  EXPECT_EQ(delegate.UniqueRetained(4), 10);
}

// 1 <- 2   |
// ^    ^   |
// |    |   |
// 3<-> 4R  |
TEST(HeapGraphWalkerTest, TwoPaths) {
  HeapGraphWalkerTestDelegate delegate;
  HeapGraphWalker walker(&delegate);
  walker.AddNode(1, 1);
  walker.AddNode(2, 2);
  walker.AddNode(3, 3);
  walker.AddNode(4, 4);

  walker.AddEdge(2, 1);
  walker.AddEdge(3, 1);
  walker.AddEdge(3, 4);
  walker.AddEdge(4, 2);
  walker.AddEdge(4, 3);

  walker.MarkRoot(4);
  walker.CalculateRetained();

  EXPECT_EQ(delegate.Retained(1), 1);
  EXPECT_EQ(delegate.Retained(2), 3);
  EXPECT_EQ(delegate.Retained(3), 10);
  EXPECT_EQ(delegate.Retained(4), 10);

  EXPECT_EQ(delegate.UniqueRetained(1), 1);
  EXPECT_EQ(delegate.UniqueRetained(2), 2);
  EXPECT_EQ(delegate.UniqueRetained(3), 3);
  EXPECT_EQ(delegate.UniqueRetained(4), 6);
}

//    1     |
//   ^^     |
//  /  \    |
// 2R   3R  |
TEST(HeapGraphWalkerTest, Diverge) {
  HeapGraphWalkerTestDelegate delegate;
  HeapGraphWalker walker(&delegate);
  walker.AddNode(1, 1);
  walker.AddNode(2, 2);
  walker.AddNode(3, 3);

  walker.AddEdge(2, 1);
  walker.AddEdge(3, 1);

  walker.MarkRoot(2);
  walker.MarkRoot(3);
  walker.CalculateRetained();

  EXPECT_EQ(delegate.Retained(1), 1);
  EXPECT_EQ(delegate.Retained(2), 3);
  EXPECT_EQ(delegate.Retained(3), 4);

  EXPECT_EQ(delegate.UniqueRetained(1), 1);
  EXPECT_EQ(delegate.UniqueRetained(2), 2);
  EXPECT_EQ(delegate.UniqueRetained(3), 3);
}

//    1            |
//   ^^            |
//  /  \           |
// 2R   3 (dead)   |
TEST(HeapGraphWalkerTest, Dead) {
  HeapGraphWalkerTestDelegate delegate;
  HeapGraphWalker walker(&delegate);
  walker.AddNode(1, 1);
  walker.AddNode(2, 2);
  walker.AddNode(3, 3);

  walker.AddEdge(2, 1);
  walker.AddEdge(3, 1);

  walker.MarkRoot(2);
  walker.CalculateRetained();

  EXPECT_EQ(delegate.Retained(1), 1);
  EXPECT_EQ(delegate.Retained(2), 3);

  EXPECT_EQ(delegate.UniqueRetained(1), 1);
  EXPECT_EQ(delegate.UniqueRetained(2), 3);
}

//    2<->3  |
//    ^      |
//    |      |
//    1R     |
TEST(HeapGraphWalkerTest, Component) {
  HeapGraphWalkerTestDelegate delegate;
  HeapGraphWalker walker(&delegate);
  walker.AddNode(1, 1);
  walker.AddNode(2, 2);
  walker.AddNode(3, 3);

  walker.AddEdge(1, 2);
  walker.AddEdge(2, 3);
  walker.AddEdge(3, 2);

  walker.MarkRoot(1);
  walker.CalculateRetained();

  EXPECT_EQ(delegate.Retained(1), 6);
  EXPECT_EQ(delegate.Retained(2), 5);
  EXPECT_EQ(delegate.Retained(3), 5);

  EXPECT_EQ(delegate.UniqueRetained(1), 6);
  // TODO(fmayer): this should be 5, as this breaks away the component.
  EXPECT_EQ(delegate.UniqueRetained(2), 2);
  EXPECT_EQ(delegate.UniqueRetained(3), 3);
}

//    2<->3R |
//    ^      |
//    |      |
//    1R     |
TEST(HeapGraphWalkerTest, ComponentWithRoot) {
  HeapGraphWalkerTestDelegate delegate;
  HeapGraphWalker walker(&delegate);
  walker.AddNode(1, 1);
  walker.AddNode(2, 2);
  walker.AddNode(3, 3);

  walker.AddEdge(1, 2);
  walker.AddEdge(2, 3);
  walker.AddEdge(3, 2);

  walker.MarkRoot(1);
  walker.MarkRoot(3);
  walker.CalculateRetained();

  EXPECT_EQ(delegate.Retained(1), 6);
  EXPECT_EQ(delegate.Retained(2), 5);
  EXPECT_EQ(delegate.Retained(3), 5);

  EXPECT_EQ(delegate.UniqueRetained(1), 1);
  EXPECT_EQ(delegate.UniqueRetained(2), 2);
  EXPECT_EQ(delegate.UniqueRetained(3), 3);
}

// R
// 2 <-  3   |
//  ^   ^   |
//   \ /    |
//    1R    |
TEST(HeapGraphWalkerTest, TwoRoots) {
  HeapGraphWalkerTestDelegate delegate;
  HeapGraphWalker walker(&delegate);
  walker.AddNode(1, 1);
  walker.AddNode(2, 2);
  walker.AddNode(3, 3);

  walker.AddEdge(1, 2);
  walker.AddEdge(1, 3);
  walker.AddEdge(3, 2);

  walker.MarkRoot(1);
  walker.MarkRoot(2);
  walker.CalculateRetained();

  EXPECT_EQ(delegate.Retained(1), 6);
  EXPECT_EQ(delegate.Retained(2), 2);
  EXPECT_EQ(delegate.Retained(3), 5);

  EXPECT_EQ(delegate.UniqueRetained(1), 4);
  EXPECT_EQ(delegate.UniqueRetained(2), 2);
  EXPECT_EQ(delegate.UniqueRetained(3), 3);
}

// Call a function for every set in the powerset or the cartesian product
// of v with itself.
// TODO(fmayer): Find a smarter way to generate all graphs.
template <typename F>
void SquarePowerSet(const std::vector<int64_t>& v, F fn) {
  for (uint64_t subset = 0; subset < pow(2, pow(v.size(), 2)); ++subset) {
    std::vector<std::pair<int64_t, int64_t>> ps;
    uint64_t node = 0;
    for (int64_t n1 : v) {
      for (int64_t n2 : v) {
        if ((1 << node++) & subset)
          ps.emplace_back(n1, n2);
      }
    }
    fn(ps);
  }
}

// Call a function for every set in the powerset.
template <typename F>
void PowerSet(const std::vector<int64_t>& v, F fn) {
  for (uint64_t subset = 0; subset < pow(2, v.size()); ++subset) {
    std::vector<int64_t> ps;
    uint64_t node = 0;
    for (int64_t n : v) {
      if ((1 << node++) & subset)
        ps.emplace_back(n);
    }
    fn(ps);
  }
}

TEST(PowerSetTest, Simple) {
  std::vector<int64_t> s = {0, 1, 2};
  std::vector<std::vector<int64_t>> ps;
  PowerSet(s, [&ps](const std::vector<int64_t>& x) { ps.emplace_back(x); });
  EXPECT_THAT(ps, UnorderedElementsAre(std::vector<int64_t>{},      //
                                       std::vector<int64_t>{0},     //
                                       std::vector<int64_t>{1},     //
                                       std::vector<int64_t>{2},     //
                                       std::vector<int64_t>{0, 1},  //
                                       std::vector<int64_t>{0, 2},  //
                                       std::vector<int64_t>{1, 2},  //
                                       std::vector<int64_t>{0, 1, 2}));
}

TEST(SquarePowerSetTest, Simple) {
  std::vector<int64_t> s = {0, 1};
  std::vector<std::vector<std::pair<int64_t, int64_t>>> ps;
  SquarePowerSet(s, [&ps](const std::vector<std::pair<int64_t, int64_t>>& x) {
    ps.emplace_back(x);
  });

  std::vector<std::pair<int64_t, int64_t>> expected[] = {
      {},                        //
      {{0, 0}},                  //
      {{0, 1}},                  //
      {{1, 0}},                  //
      {{1, 1}},                  //
      {{0, 0}, {0, 1}},          //
      {{0, 0}, {1, 0}},          //
      {{0, 0}, {1, 1}},          //
      {{0, 1}, {1, 0}},          //
      {{0, 1}, {1, 1}},          //
      {{1, 0}, {1, 1}},          //
      {{0, 0}, {0, 1}, {1, 0}},  //
      {{0, 0}, {0, 1}, {1, 1}},  //
      {{0, 0}, {1, 0}, {1, 1}},  //
      {{0, 1}, {1, 0}, {1, 1}},  //
      {{0, 0}, {0, 1}, {1, 0}, {1, 1}}};
  EXPECT_THAT(ps, UnorderedElementsAreArray(expected));
}

// Generate all graphs with 4 nodes, and assert that deleting one node frees
// up more memory than that node's unique retained.
TEST(HeapGraphWalkerTest, DISABLED_AllGraphs) {
  std::vector<int64_t> nodes{1, 2, 3, 4};
  std::vector<uint64_t> sizes{0, 1, 2, 3, 4};
  PowerSet(nodes, [&nodes, &sizes](const std::vector<int64_t>& roots) {
    SquarePowerSet(
        nodes, [&nodes, &sizes,
                &roots](const std::vector<std::pair<int64_t, int64_t>>& edges) {
          HeapGraphWalkerTestDelegate delegate;
          HeapGraphWalker walker(&delegate);

          HeapGraphWalkerTestDelegate delegate2;
          HeapGraphWalker walker2(&delegate2);

          for (int64_t node : nodes) {
            walker.AddNode(node, sizes[static_cast<size_t>(node)]);
            // walker2 leaves out node 1.
            if (node != 1)
              walker2.AddNode(node, sizes[static_cast<size_t>(node)]);
          }

          for (const auto& p : edges) {
            walker.AddEdge(p.first, p.second);
            // walker2 leaves out node 1.
            if (p.first != 1 && p.second != 1)
              walker2.AddEdge(p.first, p.second);
          }

          for (int64_t r : roots) {
            walker.MarkRoot(r);
            // walker2 leaves out node 1.
            if (r != 1)
              walker2.MarkRoot(r);
          }

          walker.CalculateRetained();
          // We do not need to CalculateRetained on walker2, because we only
          // get the reachable nodes.

          int64_t reachable = 0;
          int64_t reachable2 = 0;

          ASSERT_FALSE(delegate2.Reachable(1));
          for (int64_t n : nodes) {
            if (delegate.Reachable(n))
              reachable += sizes[static_cast<size_t>(n)];
            if (delegate2.Reachable(n))
              reachable2 += sizes[static_cast<size_t>(n)];
          }
          EXPECT_LE(reachable2, reachable);
          if (delegate.Reachable(1)) {
            // TODO(fmayer): This should be EXPECT_EQ, but we do currently
            // undercount the unique retained, because we do not include the
            // memory that could get freed by the component being broken apart.
            EXPECT_LE(delegate.UniqueRetained(1), reachable - reachable2)
                << "roots: " << testing::PrintToString(roots)
                << ", edges: " << testing::PrintToString(edges);
          } else {
            EXPECT_EQ(reachable2, reachable);
          }
        });
  });
}

bool HasPath(const HeapGraphWalker::PathFromRoot& path,
             const HeapGraphWalker::PathFromRoot::Node& node,
             std::vector<HeapGraphWalker::ClassNameId> class_names) {
  if (class_names.empty())
    return true;
  auto it = node.children.find(class_names[0]);
  if (it == node.children.end())
    return false;
  class_names.erase(class_names.begin());
  return HasPath(path, path.nodes[it->second], std::move(class_names));
}

bool HasPath(const HeapGraphWalker::PathFromRoot& path,
             std::vector<HeapGraphWalker::ClassNameId> class_names) {
  return HasPath(path, path.nodes[HeapGraphWalker::PathFromRoot::kRoot],
                 std::move(class_names));
}

//    1      |
//    ^^     |
//   /  \    |
//  2<-> 3   |
//  ^        |
//  |        |
//  4R       |
TEST(HeapGraphWalkerTest, ShortestPath) {
  HeapGraphWalkerTestDelegate delegate;
  HeapGraphWalker walker(&delegate);
  walker.AddNode(1, 1, 1u);
  walker.AddNode(2, 2, 2u);
  walker.AddNode(3, 3, 3u);
  walker.AddNode(4, 4, 4);

  walker.AddEdge(2, 1);
  walker.AddEdge(2, 3);
  walker.AddEdge(3, 1);
  walker.AddEdge(3, 2);
  walker.AddEdge(4, 2);

  walker.MarkRoot(4);
  auto path = walker.FindPathsFromRoot();

  EXPECT_TRUE(HasPath(path, {4u, 2u, 1u}));
  EXPECT_TRUE(HasPath(path, {4u, 2u, 3u}));
  EXPECT_FALSE(HasPath(path, {4u, 2u, 3u, 1u}));
}

//    1      |
//    ^^     |
//   /  \    |
//  2R<->3   |
//  ^        |
//  |        |
//  4R       |
TEST(HeapGraphWalkerTest, ShortestPathMultipleRoots) {
  HeapGraphWalkerTestDelegate delegate;
  HeapGraphWalker walker(&delegate);
  walker.AddNode(1, 1, 1u);
  walker.AddNode(2, 2, 2u);
  walker.AddNode(3, 3, 3u);
  walker.AddNode(4, 4, 4u);

  walker.AddEdge(2, 1);
  walker.AddEdge(2, 3);
  walker.AddEdge(3, 1);
  walker.AddEdge(3, 2);
  walker.AddEdge(4, 2);

  walker.MarkRoot(4);
  walker.MarkRoot(2);
  auto path = walker.FindPathsFromRoot();

  EXPECT_TRUE(HasPath(path, {2u, 1u}));
  EXPECT_TRUE(HasPath(path, {2u, 3u}));
  EXPECT_FALSE(HasPath(path, {4u, 2u, 3u}));
}

}  // namespace
}  // namespace trace_processor
}  // namespace perfetto
