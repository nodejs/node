// Copyright 2024 Google LLC
// SPDX-License-Identifier: Apache-2.0
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "hwy/contrib/thread_pool/topology.h"

#include <stddef.h>
#include <stdio.h>

#include <vector>

#include "hwy/base.h"
#include "hwy/tests/hwy_gtest.h"
#include "hwy/tests/test_util-inl.h"
#include "hwy/timer.h"

namespace hwy {
namespace {

TEST(TopologyTest, TestNum) {
  const size_t total = TotalLogicalProcessors();
  fprintf(stderr, "TotalLogical %zu\n", total);

  LogicalProcessorSet lps;
  if (GetThreadAffinity(lps)) {
    fprintf(stderr, "Active %zu\n", lps.Count());
    HWY_ASSERT(lps.Count() <= total);
  }
}

TEST(TopologyTest, TestTopology) {
  char cpu100[100];
  if (hwy::platform::GetCpuString(cpu100)) {
    fprintf(stderr, "%s\n", cpu100);
  }

  Topology topology;
  if (topology.packages.empty()) return;

  fprintf(stderr, "Topology: %zuP %zuX %zuC\n", topology.packages.size(),
          topology.packages[0].clusters.size(),
          topology.packages[0].clusters[0].lps.Count());

  HWY_ASSERT(!topology.lps.empty());
  LogicalProcessorSet nodes;
  for (size_t lp = 0; lp < topology.lps.size(); ++lp) {
    const size_t node = static_cast<size_t>(topology.lps[lp].node);
    if (!nodes.Get(node)) {
      fprintf(stderr, "Found NUMA node %zu, LP %zu\n", node, lp);
      nodes.Set(node);
    }
  }

  size_t lps_by_cluster = 0;
  size_t lps_by_core = 0;
  LogicalProcessorSet all_lps;
  for (const Topology::Package& pkg : topology.packages) {
    HWY_ASSERT(!pkg.clusters.empty());
    HWY_ASSERT(!pkg.cores.empty());
    HWY_ASSERT(pkg.clusters.size() <= pkg.cores.size());

    for (const Topology::Cluster& c : pkg.clusters) {
      lps_by_cluster += c.lps.Count();
      c.lps.Foreach([&all_lps](size_t lp) { all_lps.Set(lp); });
    }
    for (const Topology::Core& c : pkg.cores) {
      lps_by_core += c.lps.Count();
      c.lps.Foreach([&all_lps](size_t lp) { all_lps.Set(lp); });
    }
  }
  // Ensure the per-cluster and per-core sets sum to the total.
  HWY_ASSERT(lps_by_cluster == topology.lps.size());
  HWY_ASSERT(lps_by_core == topology.lps.size());
  // .. and are a partition of unity (all LPs are covered)
  HWY_ASSERT(all_lps.Count() == topology.lps.size());
}

void PrintCache(const Cache& c, size_t level) {
  fprintf(stderr,
          "L%zu: size %u KiB, line size %u, assoc %u, sets %u, cores %u\n",
          level, c.size_kib, c.bytes_per_line, c.associativity, c.sets,
          c.cores_sharing);
}

static void CheckCache(const Cache& c, size_t level) {
  // L1-L2 must exist, L3 is not guaranteed.
  if (level == 3 && c.size_kib == 0) {
    HWY_ASSERT(c.associativity == 0 && c.bytes_per_line == 0 && c.sets == 0);
    return;
  }

  // size and thus sets are not necessarily powers of two.
  HWY_ASSERT(c.size_kib != 0);
  HWY_ASSERT(c.sets != 0);

  // Intel Skylake has non-pow2 L3 associativity, and Apple L2 also, so we can
  // only check loose bounds.
  HWY_ASSERT(c.associativity >= 2);
  HWY_ASSERT(c.associativity <= Cache::kMaxAssociativity);

  // line sizes are always powers of two because CPUs partition addresses into
  // line offsets (the lower bits), set, and tag.
  const auto is_pow2 = [](uint32_t x) { return x != 0 && (x & (x - 1)) == 0; };
  HWY_ASSERT(is_pow2(c.bytes_per_line));
  HWY_ASSERT(32 <= c.bytes_per_line && c.bytes_per_line <= 1024);

  HWY_ASSERT(c.cores_sharing != 0);
  // +1 observed on RISC-V.
  HWY_ASSERT(c.cores_sharing <= TotalLogicalProcessors() + 1);
}

TEST(TopologyTest, TestCaches) {
  const Cache* caches = DataCaches();
  if (!caches) return;
  for (size_t level = 1; level <= 3; ++level) {
    PrintCache(caches[level], level);
    CheckCache(caches[level], level);
  }
}

}  // namespace
}  // namespace hwy

HWY_TEST_MAIN();
