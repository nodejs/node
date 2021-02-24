// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_CPPGC_CONCURRENT_MARKER_H_
#define V8_HEAP_CPPGC_CONCURRENT_MARKER_H_

#include "include/cppgc/platform.h"
#include "src/heap/cppgc/incremental-marking-schedule.h"
#include "src/heap/cppgc/marking-state.h"
#include "src/heap/cppgc/marking-visitor.h"
#include "src/heap/cppgc/marking-worklists.h"

namespace cppgc {
namespace internal {

class V8_EXPORT_PRIVATE ConcurrentMarkerBase {
 public:
  ConcurrentMarkerBase(HeapBase&, MarkingWorklists&,
                       IncrementalMarkingSchedule&, cppgc::Platform*);
  virtual ~ConcurrentMarkerBase();

  ConcurrentMarkerBase(const ConcurrentMarkerBase&) = delete;
  ConcurrentMarkerBase& operator=(const ConcurrentMarkerBase&) = delete;

  void Start();
  void Cancel();

  void JoinForTesting();

  void NotifyIncrementalMutatorStepCompleted();

  bool IsActive() const;

  HeapBase& heap() const { return heap_; }
  MarkingWorklists& marking_worklists() const { return marking_worklists_; }
  IncrementalMarkingSchedule& incremental_marking_schedule() const {
    return incremental_marking_schedule_;
  }

  virtual std::unique_ptr<Visitor> CreateConcurrentMarkingVisitor(
      ConcurrentMarkingState&) const = 0;

 protected:
  void IncreaseMarkingPriorityIfNeeded();

 private:
  HeapBase& heap_;
  MarkingWorklists& marking_worklists_;
  IncrementalMarkingSchedule& incremental_marking_schedule_;
  cppgc::Platform* const platform_;

  // The job handle doubles as flag to denote concurrent marking was started.
  std::unique_ptr<JobHandle> concurrent_marking_handle_{nullptr};

  size_t last_concurrently_marked_bytes_ = 0;
  v8::base::TimeTicks last_concurrently_marked_bytes_update_;
  bool concurrent_marking_priority_increased_{false};
};

class V8_EXPORT_PRIVATE ConcurrentMarker : public ConcurrentMarkerBase {
 public:
  ConcurrentMarker(HeapBase& heap, MarkingWorklists& marking_worklists,
                   IncrementalMarkingSchedule& incremental_marking_schedule,
                   cppgc::Platform* platform)
      : ConcurrentMarkerBase(heap, marking_worklists,
                             incremental_marking_schedule, platform) {}

  std::unique_ptr<Visitor> CreateConcurrentMarkingVisitor(
      ConcurrentMarkingState&) const final;
};

}  // namespace internal
}  // namespace cppgc

#endif  // V8_HEAP_CPPGC_CONCURRENT_MARKER_H_
