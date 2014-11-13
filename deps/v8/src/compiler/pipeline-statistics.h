// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_PIPELINE_STATISTICS_H_
#define V8_COMPILER_PIPELINE_STATISTICS_H_

#include <string>

#include "src/compilation-statistics.h"
#include "src/compiler/zone-pool.h"

namespace v8 {
namespace internal {
namespace compiler {

class PhaseScope;

class PipelineStatistics : public Malloced {
 public:
  PipelineStatistics(CompilationInfo* info, ZonePool* zone_pool);
  ~PipelineStatistics();

  void BeginPhaseKind(const char* phase_kind_name);

 private:
  size_t OuterZoneSize() {
    return static_cast<size_t>(outer_zone_->allocation_size());
  }

  class CommonStats {
   public:
    CommonStats() : outer_zone_initial_size_(0) {}

    void Begin(PipelineStatistics* pipeline_stats);
    void End(PipelineStatistics* pipeline_stats,
             CompilationStatistics::BasicStats* diff);

    SmartPointer<ZonePool::StatsScope> scope_;
    base::ElapsedTimer timer_;
    size_t outer_zone_initial_size_;
    size_t allocated_bytes_at_start_;
  };

  bool InPhaseKind() { return !phase_kind_stats_.scope_.is_empty(); }
  void EndPhaseKind();

  friend class PhaseScope;
  bool InPhase() { return !phase_stats_.scope_.is_empty(); }
  void BeginPhase(const char* name);
  void EndPhase();

  Isolate* isolate_;
  Zone* outer_zone_;
  ZonePool* zone_pool_;
  CompilationStatistics* compilation_stats_;
  std::string function_name_;

  // Stats for the entire compilation.
  CommonStats total_stats_;
  size_t source_size_;

  // Stats for phase kind.
  const char* phase_kind_name_;
  CommonStats phase_kind_stats_;

  // Stats for phase.
  const char* phase_name_;
  CommonStats phase_stats_;

  DISALLOW_COPY_AND_ASSIGN(PipelineStatistics);
};


class PhaseScope {
 public:
  PhaseScope(PipelineStatistics* pipeline_stats, const char* name)
      : pipeline_stats_(pipeline_stats) {
    if (pipeline_stats_ != NULL) pipeline_stats_->BeginPhase(name);
  }
  ~PhaseScope() {
    if (pipeline_stats_ != NULL) pipeline_stats_->EndPhase();
  }

 private:
  PipelineStatistics* const pipeline_stats_;

  DISALLOW_COPY_AND_ASSIGN(PhaseScope);
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif
