// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/array-buffer-sweeper.h"

#include <atomic>

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

  bytes_ += extension->accounting_length();
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

  bytes_ += list->Bytes();
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

size_t ArrayBufferList::BytesSlow() {
  ArrayBufferExtension* current = head_;
  size_t sum = 0;

  while (current) {
    sum += current->accounting_length();
    current = current->next();
  }

  return sum;
}

void ArrayBufferSweeper::EnsureFinished() {
  if (!sweeping_in_progress_) return;

  TryAbortResult abort_result =
      heap_->isolate()->cancelable_task_manager()->TryAbort(job_->id_);

  switch (abort_result) {
    case TryAbortResult::kTaskAborted: {
      job_->Sweep();
      Merge();
      break;
    }

    case TryAbortResult::kTaskRemoved: {
      if (job_->state_ == SweepingState::kInProgress) job_->Sweep();
      if (job_->state_ == SweepingState::kDone) Merge();
      break;
    }

    case TryAbortResult::kTaskRunning: {
      base::MutexGuard guard(&sweeping_mutex_);
      // Wait until task is finished with its work.
      while (job_->state_ != SweepingState::kDone) {
        job_finished_.Wait(&sweeping_mutex_);
      }
      Merge();
      break;
    }

    default:
      UNREACHABLE();
  }

  DecrementExternalMemoryCounters();
  sweeping_in_progress_ = false;
}

void ArrayBufferSweeper::AdjustCountersAndMergeIfPossible() {
  if (sweeping_in_progress_) {
    DCHECK(job_.has_value());
    if (job_->state_ == SweepingState::kDone) {
      Merge();
      sweeping_in_progress_ = false;
    } else {
      DecrementExternalMemoryCounters();
    }
  }
}

void ArrayBufferSweeper::DecrementExternalMemoryCounters() {
  size_t freed_bytes = freed_bytes_.exchange(0, std::memory_order_relaxed);

  if (freed_bytes > 0) {
    heap_->DecrementExternalBackingStoreBytes(
        ExternalBackingStoreType::kArrayBuffer, freed_bytes);
    heap_->update_external_memory(-static_cast<int64_t>(freed_bytes));
  }
}

void ArrayBufferSweeper::RequestSweepYoung() {
  RequestSweep(SweepingScope::kYoung);
}

void ArrayBufferSweeper::RequestSweepFull() {
  RequestSweep(SweepingScope::kFull);
}

size_t ArrayBufferSweeper::YoungBytes() { return young_bytes_; }

size_t ArrayBufferSweeper::OldBytes() { return old_bytes_; }

void ArrayBufferSweeper::RequestSweep(SweepingScope scope) {
  DCHECK(!sweeping_in_progress_);

  if (young_.IsEmpty() && (old_.IsEmpty() || scope == SweepingScope::kYoung))
    return;

  if (!heap_->IsTearingDown() && !heap_->ShouldReduceMemory() &&
      FLAG_concurrent_array_buffer_sweeping) {
    Prepare(scope);

    auto task = MakeCancelableTask(heap_->isolate(), [this, scope] {
      GCTracer::Scope::ScopeId scope_id =
          scope == SweepingScope::kYoung
              ? GCTracer::Scope::BACKGROUND_YOUNG_ARRAY_BUFFER_SWEEP
              : GCTracer::Scope::BACKGROUND_FULL_ARRAY_BUFFER_SWEEP;
      TRACE_GC_EPOCH(heap_->tracer(), scope_id, ThreadKind::kBackground);
      base::MutexGuard guard(&sweeping_mutex_);
      job_->Sweep();
      job_finished_.NotifyAll();
    });
    job_->id_ = task->id();
    V8::GetCurrentPlatform()->CallOnWorkerThread(std::move(task));
    sweeping_in_progress_ = true;
  } else {
    Prepare(scope);
    job_->Sweep();
    Merge();
    DecrementExternalMemoryCounters();
  }
}

void ArrayBufferSweeper::Prepare(SweepingScope scope) {
  DCHECK(!job_.has_value());

  if (scope == SweepingScope::kYoung) {
    job_.emplace(this, young_, ArrayBufferList(), SweepingScope::kYoung);
    young_.Reset();
    young_bytes_ = 0;
  } else {
    CHECK_EQ(scope, SweepingScope::kFull);
    job_.emplace(this, young_, old_, SweepingScope::kFull);
    young_.Reset();
    old_.Reset();
    young_bytes_ = old_bytes_ = 0;
  }
}

void ArrayBufferSweeper::Merge() {
  DCHECK(job_.has_value());
  CHECK_EQ(job_->state_, SweepingState::kDone);
  young_.Append(&job_->young_);
  old_.Append(&job_->old_);
  young_bytes_ = young_.Bytes();
  old_bytes_ = old_.Bytes();

  job_.reset();
}

void ArrayBufferSweeper::ReleaseAll() {
  EnsureFinished();
  ReleaseAll(&old_);
  ReleaseAll(&young_);
  old_bytes_ = young_bytes_ = 0;
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
  size_t bytes = extension->accounting_length();

  if (Heap::InYoungGeneration(object)) {
    young_.Append(extension);
    young_bytes_ += bytes;
  } else {
    old_.Append(extension);
    old_bytes_ += bytes;
  }

  AdjustCountersAndMergeIfPossible();
  DecrementExternalMemoryCounters();
  IncrementExternalMemoryCounters(bytes);
}

void ArrayBufferSweeper::IncrementExternalMemoryCounters(size_t bytes) {
  heap_->IncrementExternalBackingStoreBytes(
      ExternalBackingStoreType::kArrayBuffer, bytes);
  reinterpret_cast<v8::Isolate*>(heap_->isolate())
      ->AdjustAmountOfExternalAllocatedMemory(static_cast<int64_t>(bytes));
}

void ArrayBufferSweeper::IncrementFreedBytes(size_t bytes) {
  if (bytes == 0) return;
  freed_bytes_.fetch_add(bytes, std::memory_order_relaxed);
}

void ArrayBufferSweeper::SweepingJob::Sweep() {
  CHECK_EQ(state_, SweepingState::kInProgress);

  if (scope_ == SweepingScope::kYoung) {
    SweepYoung();
  } else {
    CHECK_EQ(scope_, SweepingScope::kFull);
    SweepFull();
  }
  state_ = SweepingState::kDone;
}

void ArrayBufferSweeper::SweepingJob::SweepFull() {
  CHECK_EQ(scope_, SweepingScope::kFull);
  ArrayBufferList promoted = SweepListFull(&young_);
  ArrayBufferList survived = SweepListFull(&old_);

  old_ = promoted;
  old_.Append(&survived);
}

ArrayBufferList ArrayBufferSweeper::SweepingJob::SweepListFull(
    ArrayBufferList* list) {
  ArrayBufferExtension* current = list->head_;
  ArrayBufferList survivor_list;

  while (current) {
    ArrayBufferExtension* next = current->next();

    if (!current->IsMarked()) {
      size_t bytes = current->accounting_length();
      delete current;
      sweeper_->IncrementFreedBytes(bytes);
    } else {
      current->Unmark();
      survivor_list.Append(current);
    }

    current = next;
  }

  list->Reset();
  return survivor_list;
}

void ArrayBufferSweeper::SweepingJob::SweepYoung() {
  CHECK_EQ(scope_, SweepingScope::kYoung);
  ArrayBufferExtension* current = young_.head_;

  ArrayBufferList new_young;
  ArrayBufferList new_old;

  while (current) {
    ArrayBufferExtension* next = current->next();

    if (!current->IsYoungMarked()) {
      size_t bytes = current->accounting_length();
      delete current;
      sweeper_->IncrementFreedBytes(bytes);
    } else if (current->IsYoungPromoted()) {
      current->YoungUnmark();
      new_old.Append(current);
    } else {
      current->YoungUnmark();
      new_young.Append(current);
    }

    current = next;
  }

  old_ = new_old;
  young_ = new_young;
}

}  // namespace internal
}  // namespace v8
