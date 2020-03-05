// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/array-buffer-sweeper.h"
#include "src/heap/gc-tracer.h"
#include "src/heap/heap-inl.h"
#include "src/objects/js-array-buffer.h"
#include "src/tasks/cancelable-task.h"
#include "src/tasks/task-utils.h"

namespace v8 {
namespace internal {

void ArrayBufferList::Append(ArrayBufferExtension* extension) {
  if (head_ == nullptr) {
    DCHECK_NULL(tail_);
    head_ = tail_ = extension;
  } else {
    tail_->set_next(extension);
    tail_ = extension;
  }

  extension->set_next(nullptr);
}

void ArrayBufferList::Append(ArrayBufferList* list) {
  if (head_ == nullptr) {
    DCHECK_NULL(tail_);
    head_ = list->head_;
    tail_ = list->tail_;
  } else if (list->head_) {
    DCHECK_NOT_NULL(list->tail_);
    tail_->set_next(list->head_);
    tail_ = list->tail_;
  } else {
    DCHECK_NULL(list->tail_);
  }

  list->Reset();
}

bool ArrayBufferList::Contains(ArrayBufferExtension* extension) {
  ArrayBufferExtension* current = head_;

  while (current) {
    if (current == extension) return true;
    current = current->next();
  }

  return false;
}

void ArrayBufferSweeper::EnsureFinished() {
  if (!sweeping_in_progress_) return;

  TryAbortResult abort_result =
      heap_->isolate()->cancelable_task_manager()->TryAbort(job_.id);

  switch (abort_result) {
    case TryAbortResult::kTaskAborted: {
      job_.Sweep();
      Merge();
      break;
    }

    case TryAbortResult::kTaskRemoved: {
      CHECK_NE(job_.state, SweepingState::Uninitialized);
      if (job_.state == SweepingState::Prepared) job_.Sweep();
      Merge();
      break;
    }

    case TryAbortResult::kTaskRunning: {
      base::MutexGuard guard(&sweeping_mutex_);
      CHECK_NE(job_.state, SweepingState::Uninitialized);
      // Wait until task is finished with its work.
      while (job_.state != SweepingState::Swept) {
        job_finished_.Wait(&sweeping_mutex_);
      }
      Merge();
      break;
    }

    default:
      UNREACHABLE();
  }

  sweeping_in_progress_ = false;
}

void ArrayBufferSweeper::RequestSweepYoung() {
  RequestSweep(SweepingScope::Young);
}

void ArrayBufferSweeper::RequestSweepFull() {
  RequestSweep(SweepingScope::Full);
}

void ArrayBufferSweeper::RequestSweep(SweepingScope scope) {
  DCHECK(!sweeping_in_progress_);

  if (young_.IsEmpty() && (old_.IsEmpty() || scope == SweepingScope::Young))
    return;

  if (!heap_->IsTearingDown() && !heap_->ShouldReduceMemory() &&
      FLAG_concurrent_array_buffer_sweeping) {
    Prepare(scope);

    auto task = MakeCancelableTask(heap_->isolate(), [this] {
      TRACE_BACKGROUND_GC(
          heap_->tracer(),
          GCTracer::BackgroundScope::BACKGROUND_ARRAY_BUFFER_SWEEP);
      base::MutexGuard guard(&sweeping_mutex_);
      job_.Sweep();
      job_finished_.NotifyAll();
    });
    job_.id = task->id();
    V8::GetCurrentPlatform()->CallOnWorkerThread(std::move(task));
    sweeping_in_progress_ = true;
  } else {
    Prepare(scope);
    job_.Sweep();
    Merge();
  }
}

void ArrayBufferSweeper::Prepare(SweepingScope scope) {
  CHECK_EQ(job_.state, SweepingState::Uninitialized);

  if (scope == SweepingScope::Young) {
    job_ =
        SweepingJob::Prepare(young_, ArrayBufferList(), SweepingScope::Young);
    young_.Reset();
  } else {
    CHECK_EQ(scope, SweepingScope::Full);
    job_ = SweepingJob::Prepare(young_, old_, SweepingScope::Full);
    young_.Reset();
    old_.Reset();
  }
}

void ArrayBufferSweeper::Merge() {
  CHECK_EQ(job_.state, SweepingState::Swept);
  young_.Append(&job_.young);
  old_.Append(&job_.old);
  job_.state = SweepingState::Uninitialized;
}

void ArrayBufferSweeper::ReleaseAll() {
  EnsureFinished();
  ReleaseAll(&old_);
  ReleaseAll(&young_);
}

void ArrayBufferSweeper::ReleaseAll(ArrayBufferList* list) {
  ArrayBufferExtension* current = list->head_;

  while (current) {
    ArrayBufferExtension* next = current->next();
    delete current;
    current = next;
  }

  list->Reset();
}

void ArrayBufferSweeper::Append(JSArrayBuffer object,
                                ArrayBufferExtension* extension) {
  if (Heap::InYoungGeneration(object)) {
    young_.Append(extension);
  } else {
    old_.Append(extension);
  }
}

ArrayBufferSweeper::SweepingJob::SweepingJob()
    : state(SweepingState::Uninitialized) {}

ArrayBufferSweeper::SweepingJob ArrayBufferSweeper::SweepingJob::Prepare(
    ArrayBufferList young, ArrayBufferList old, SweepingScope scope) {
  SweepingJob job;
  job.young = young;
  job.old = old;
  job.scope = scope;
  job.id = 0;
  job.state = SweepingState::Prepared;
  return job;
}

void ArrayBufferSweeper::SweepingJob::Sweep() {
  CHECK_EQ(state, SweepingState::Prepared);

  if (scope == SweepingScope::Young) {
    SweepYoung();
  } else {
    CHECK_EQ(scope, SweepingScope::Full);
    SweepFull();
  }
  state = SweepingState::Swept;
}

void ArrayBufferSweeper::SweepingJob::SweepFull() {
  CHECK_EQ(scope, SweepingScope::Full);
  ArrayBufferList promoted = SweepListFull(&young);
  ArrayBufferList survived = SweepListFull(&old);

  old = promoted;
  old.Append(&survived);
}

ArrayBufferList ArrayBufferSweeper::SweepingJob::SweepListFull(
    ArrayBufferList* list) {
  ArrayBufferExtension* current = list->head_;
  ArrayBufferList survived;

  while (current) {
    ArrayBufferExtension* next = current->next();

    if (!current->IsMarked()) {
      delete current;
    } else {
      current->Unmark();
      survived.Append(current);
    }

    current = next;
  }

  list->Reset();
  return survived;
}

void ArrayBufferSweeper::SweepingJob::SweepYoung() {
  CHECK_EQ(scope, SweepingScope::Young);
  ArrayBufferExtension* current = young.head_;

  ArrayBufferList new_young;
  ArrayBufferList new_old;

  while (current) {
    ArrayBufferExtension* next = current->next();

    if (!current->IsYoungMarked()) {
      delete current;
    } else if (current->IsYoungPromoted()) {
      current->YoungUnmark();
      new_old.Append(current);
    } else {
      current->YoungUnmark();
      new_young.Append(current);
    }

    current = next;
  }

  old = new_old;
  young = new_young;
}

}  // namespace internal
}  // namespace v8
