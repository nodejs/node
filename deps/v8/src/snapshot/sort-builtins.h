// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SNAPSHOT_SORT_BUILTINS_H_
#define V8_SNAPSHOT_SORT_BUILTINS_H_

#include <unordered_map>
#include <vector>

#include "src/builtins/builtins.h"
#include "src/diagnostics/basic-block-profiler.h"

// The inputs were the builtin size, call graph and basic block execution count.
// There are 3 steps in this sorting algorithm:
// 1. Initializing cluster and sorting:
//  A cluster represents a group of functions. At the beginning, each
//  function was in an individual cluster, and we sort these clusters
//  by their density (which means how much probabilities this function was
//  invoked).
//
// 2. Merge the best predecessor:
//  After step 1, we will get lots of clusters which may contain only
//  one function. According to this order, we iterate each function
//  and merge cluster with some conditions, like:
//   1) The most incoming probability.
//   2) Incoming probability must be bigger than a threshold, like 0.1
//   3) Merged cluster size couldn't be bigger than a threshold, like 1 mb.
//   4) Predecessor cluster density couldn't be bigger N times than the new
//   merged cluster, N is 8 now.
//
// 3. Sorting clusters:
//  After step 2, we obtain lots of clusters which comprise several functions.
//  We will finally sort these clusters by their density.

namespace v8 {
namespace internal {

class Cluster;
struct CallProbability {
  CallProbability(int32_t incoming = 0, int32_t outgoing = 0)
      : incoming_(incoming), outgoing_(outgoing) {}

  // There are a caller and a callee, we assume caller was invoked
  // "caller-count" times, it calls callee "call-count" times, the callee was
  // invoked "callee-count" times. imcoming_ means the possibity the callee
  // calls from caller, it was calculted by call-count / callee-count. If
  // callee-count is 0 (may not be compiled by TurboFan or normalized as 0 due
  // to too small), we set imcoming_ as -1.
  int32_t incoming_;
  // outgoing_ means the possibity the caller
  // calls to callee, it was calculted by call-count / caller-count. If
  // caller-count is 0 (may not be compiled by TurboFan or normalized as 0 due
  // to too small), we set outgoing_ as -1. We didn't use outgoing_ as condition
  // for reordering builtins yet, but we could try to do some experiments with
  // it later for obtaining a better order of builtins.
  int32_t outgoing_;
};
// The key is the callee builtin, the value is call probabilities in percent
// (mostly range in 0 ~ 100, except one call happend in a loop block which was
// executed more times than block 0 of this builtin).
using CallProbabilities = std::unordered_map<Builtin, CallProbability>;
// The key is the caller builtin.
using CallGraph = std::unordered_map<Builtin, CallProbabilities>;
// The key is the builtin id, the value is density of builtin (range in 0 ~
// 10000).
using BuiltinDensityMap = std::unordered_map<Builtin, uint32_t>;
// The index is the builtin id, the value is size of builtin (in bytes).
using BuiltinSize = std::vector<uint32_t>;
// The key is the builtin id, the value is the cluster which it was comprised.
using BuiltinClusterMap = std::unordered_map<Builtin, Cluster*>;

class BuiltinsSorter {
  const int32_t kMinEdgeProbabilityThreshold = 10;
  const uint32_t kMaxClusterSize = 1 * MB;
  const uint32_t kMaxDensityDecreaseThreshold = 8;

  const std::string kBuiltinCallBlockDensityMarker = "block_count";
  const std::string kBuiltinDensityMarker = "builtin_count";

  // Pair of denstity of builtin and builtin id.
  struct BuiltinDensitySlot {
    BuiltinDensitySlot(uint32_t density, Builtin builtin)
        : density_(density), builtin_(builtin) {}

    uint32_t density_;
    Builtin builtin_;
  };

 public:
  BuiltinsSorter();
  ~BuiltinsSorter();
  std::vector<Builtin> SortBuiltins(const char* profiling_file,
                                    const std::vector<uint32_t>& builtin_size);

 private:
  void InitializeCallGraph(const char* profiling_file,
                           const std::vector<uint32_t>& size);
  void InitializeClusters();
  void MergeBestPredecessors();
  void SortClusters();
  Builtin FindBestPredecessorOf(Builtin callee);
  void ProcessBlockCountLineInfo(
      std::istringstream& line_stream,
      std::unordered_map<std::string, Builtin>& name2id);
  void ProcessBuiltinDensityLineInfo(
      std::istringstream& line_stream,
      std::unordered_map<std::string, Builtin>& name2id);

  std::vector<Cluster*> clusters_;

  std::vector<BuiltinDensitySlot> builtin_density_order_;

  CallGraph call_graph_;

  BuiltinDensityMap builtin_density_map_;

  BuiltinSize builtin_size_;

  BuiltinClusterMap builtin_cluster_map_;

  friend class Cluster;
};

class Cluster {
 public:
  Cluster(uint32_t density, uint32_t size, Builtin target,
          BuiltinsSorter* sorter);
  void Merge(Cluster* other);
  uint64_t time_approximation();

 private:
  // Max initialized density was normalized as 10000.
  uint32_t density_;
  // Size of the cluster in bytes.
  uint32_t size_;
  std::vector<Builtin> targets_;
  BuiltinsSorter* sorter_;

  friend class BuiltinsSorter;
};

}  // namespace internal

}  // namespace v8

#endif  // V8_SNAPSHOT_SORT_BUILTINS_H_
