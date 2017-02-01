// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CRANKSHAFT_COMPILATION_PHASE_H_
#define V8_CRANKSHAFT_COMPILATION_PHASE_H_

#include "src/allocation.h"
#include "src/base/platform/elapsed-timer.h"
#include "src/compilation-info.h"
#include "src/zone/zone.h"

namespace v8 {
namespace internal {

class CompilationPhase BASE_EMBEDDED {
 public:
  CompilationPhase(const char* name, CompilationInfo* info);
  ~CompilationPhase();

 protected:
  bool ShouldProduceTraceOutput() const;

  const char* name() const { return name_; }
  CompilationInfo* info() const { return info_; }
  Isolate* isolate() const { return info()->isolate(); }
  Zone* zone() { return &zone_; }

 private:
  const char* name_;
  CompilationInfo* info_;
  Zone zone_;
  size_t info_zone_start_allocation_size_;
  base::ElapsedTimer timer_;

  DISALLOW_COPY_AND_ASSIGN(CompilationPhase);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_CRANKSHAFT_COMPILATION_PHASE_H_
