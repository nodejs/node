// Copyright 2018 The Abseil Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <algorithm>
#include <cstdint>
#include <vector>

#include "absl/base/internal/raw_logging.h"
#include "absl/synchronization/internal/graphcycles.h"
#include "benchmark/benchmark.h"

namespace {

void BM_StressTest(benchmark::State& state) {
  const int num_nodes = state.range(0);
  while (state.KeepRunningBatch(num_nodes)) {
    absl::synchronization_internal::GraphCycles g;
    std::vector<absl::synchronization_internal::GraphId> nodes(num_nodes);
    for (int i = 0; i < num_nodes; i++) {
      nodes[i] = g.GetId(reinterpret_cast<void*>(static_cast<uintptr_t>(i)));
    }
    for (int i = 0; i < num_nodes; i++) {
      int end = std::min(num_nodes, i + 5);
      for (int j = i + 1; j < end; j++) {
        ABSL_RAW_CHECK(g.InsertEdge(nodes[i], nodes[j]), "");
      }
    }
  }
}
BENCHMARK(BM_StressTest)->Range(2048, 1048576);

using absl::synchronization_internal::GraphCycles;
using absl::synchronization_internal::GraphId;

// Returns a node for a new pointer value, next_ptr, and increments next_ptr.
GraphId NewNode(GraphCycles& g, uintptr_t& next_ptr) {
  return g.GetId(reinterpret_cast<void*>(next_ptr++));
}

// Adds a chain of n new nodes after node.
void AddChain(GraphCycles& g, GraphId node, int n, uintptr_t& next_ptr) {
  for (int i = 0; i < n; i++) {
    const GraphId next = NewNode(g, next_ptr);
    ABSL_RAW_CHECK(g.InsertEdge(node, next), "");
    node = next;
  }
}

// Inserts edges from new nodes to an old node that has a chain of
// state.range(0) descendants. New nodes rank above old ones, so each edge
// violates the rank order. Deadlock detection sees this pattern whenever a
// newly created Mutex is held while an older one is acquired.
void BM_NewNodeToOldNode(benchmark::State& state) {
  const int num_descendants = static_cast<int>(state.range(0));
  constexpr int kNewNodes = 10000;
  while (state.KeepRunningBatch(kNewNodes)) {
    GraphCycles g;
    uintptr_t next_ptr = 1;
    const GraphId old_node = NewNode(g, next_ptr);
    AddChain(g, old_node, num_descendants, next_ptr);
    for (int i = 0; i < kNewNodes; i++) {
      const GraphId new_node = NewNode(g, next_ptr);
      ABSL_RAW_CHECK(g.InsertEdge(new_node, old_node), "");
    }
  }
}
BENCHMARK(BM_NewNodeToOldNode)->Arg(256);

// Inserts edges u -> new node -> y, where u -> y are old nodes and y has a
// chain of state.range(0) descendants. Each new node must move between u and y,
// so these moves all go into the same gap. Deadlock detection sees this
// pattern when newly created Mutexes are each acquired while one Mutex is held,
// and then held while another one is acquired.
void BM_NewNodesBetweenOldNodes(benchmark::State& state) {
  const int num_descendants = static_cast<int>(state.range(0));
  constexpr int kNewNodes = 10000;
  while (state.KeepRunningBatch(kNewNodes)) {
    GraphCycles g;
    uintptr_t next_ptr = 1;
    const GraphId u = NewNode(g, next_ptr);
    const GraphId y = NewNode(g, next_ptr);
    ABSL_RAW_CHECK(g.InsertEdge(u, y), "");
    AddChain(g, y, num_descendants, next_ptr);
    for (int i = 0; i < kNewNodes; i++) {
      const GraphId new_node = NewNode(g, next_ptr);
      ABSL_RAW_CHECK(g.InsertEdge(u, new_node), "");
      ABSL_RAW_CHECK(g.InsertEdge(new_node, y), "");
    }
  }
}
BENCHMARK(BM_NewNodesBetweenOldNodes)->Arg(256);

}  // namespace
