// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/crankshaft/compilation-phase.h"

#include "src/crankshaft/hydrogen.h"
#include "src/isolate.h"
#include "src/objects-inl.h"

namespace v8 {
namespace internal {

CompilationPhase::CompilationPhase(const char* name, CompilationInfo* info)
    : name_(name), info_(info), zone_(info->isolate()->allocator(), ZONE_NAME) {
  if (FLAG_hydrogen_stats) {
    info_zone_start_allocation_size_ = info->zone()->allocation_size();
    timer_.Start();
  }
}

CompilationPhase::~CompilationPhase() {
  if (FLAG_hydrogen_stats) {
    size_t size = zone()->allocation_size();
    size += info_->zone()->allocation_size() - info_zone_start_allocation_size_;
    isolate()->GetHStatistics()->SaveTiming(name_, timer_.Elapsed(), size);
  }
}

bool CompilationPhase::ShouldProduceTraceOutput() const {
  // Trace if the appropriate trace flag is set and the phase name's first
  // character is in the FLAG_trace_phase command line parameter.
  AllowHandleDereference allow_deref;
  bool tracing_on =
      info()->IsStub()
          ? FLAG_trace_hydrogen_stubs
          : (FLAG_trace_hydrogen &&
             info()->shared_info()->PassesFilter(FLAG_trace_hydrogen_filter));
  return (tracing_on &&
          base::OS::StrChr(const_cast<char*>(FLAG_trace_phase), name_[0]) !=
              NULL);
}

}  // namespace internal
}  // namespace v8
