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
  CHECK(V8_ARRAY_BUFFER_EXTENSION_BOOL);

  TryAbortResult abort_result =
      heap_->isolate()->cancelable_task_manager()->TryAbort(job_.id);

  switch (abort_result) {
    case TryAbortResult::kTaskAborted: {
      Sweep();
      Merge();
      break;
    }

    case TryAbortResult::kTaskRemoved: {
      CHECK_NE(job_.state, SweepingState::Uninitialized);
      if (job_.state == SweepingState::Prepared) Sweep();
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

  DecrementExternalMemoryCounters();
  sweeping_in_progress_ = false;
}

void ArrayBufferSweeper::DecrementExternalMemoryCounters() {
  size_t bytes = freed_bytes_.load(std::memory_order_relaxed);
  if (bytes == 0) return;

  while (!freed_bytes_.compare_exchange_weak(bytes, 0)) {
    // empty body
  }

  if (bytes == 0) return;

  heap_->DecrementExternalBackingStoreBytes(
      ExternalBackingStoreType::kArrayBuffer, bytes);
  heap_->update_external_memory(-static_cast<int64_t>(bytes));
}

void ArrayBufferSweeper::RequestSweepYoung() {
  RequestSweep(SweepingScope::Young);
}

void ArrayBufferSweeper::RequestSweepFull() {
  RequestSweep(SweepingScope::Full);
}

size_t ArrayBufferSweeper::YoungBytes() { return young_bytes_; }

size_t ArrayBufferSweeper::OldBytes() { return old_bytes_; }

void ArrayBufferSweeper::RequestSweep(SweepingScope scope) {
  DCHECK(!sweeping_in_progress_);

  if (young_.IsEmpty() && (old_.IsEmpty() || scope == SweepingScope::Young))
    return;

  CHECK(V8_ARRAY_BUFFER_EXTENSION_BOOL);

  if (!heap_->IsTearingDown() && !heap_->ShouldReduceMemory() &&
      FLAG_concurrent_array_buffer_sweeping) {
    Prepare(scope);

    auto task = MakeCancelableTask(heap_->isolate(), [this] {
      TRACE_BACKGROUND_GC(
          heap_->tracer(),
          GCTracer::BackgroundScope::BACKGROUND_ARRAY_BUFFER_SWEEP);
      base::MutexGuard guard(&sweeping_mutex_);
      Sweep();
      job_finished_.NotifyAll();
    });
    job_.id = task->id();
    V8::GetCurrentPlatform()->CallOnWorkerThread(std::move(task));
    sweeping_in_progress_ = true;
  } else {
    Prepare(scope);
    Sweep();
    Merge();
    DecrementExternalMemoryCounters();
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
  young_bytes_ = young_.Bytes();
  old_bytes_ = old_.Bytes();
  job_.state = SweepingState::Uninitialized;
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
  CHECK(V8_ARRAY_BUFFER_EXTENSION_BOOL);
  size_t bytes = extension->accounting_length();

  if (Heap::InYoungGeneration(object)) {
    young_.Append(extension);
    young_bytes_ += bytes;
  } else {
    old_.Append(extension);
    old_bytes_ += bytes;
  }

  DecrementExternalMemoryCounters();
  IncrementExternalMemoryCounters(bytes);
}

void ArrayBufferSweeper::IncrementExternalMemoryCounters(size_t bytes) {
  heap_->IncrementExternalBackingStoreBytes(
      ExternalBackingStoreType::kArrayBuffer, bytes);
  reinterpret_cast<v8::Isolate*>(heap_->isolate())
      ->AdjustAmountOfExternalAllocatedMemory(static_cast<int64_t>(bytes));
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

void ArrayBufferSweeper::Sweep() {
  CHECK_EQ(job_.state, SweepingState::Prepared);

  if (job_.scope == SweepingScope::Young) {
    SweepYoung();
  } else {
    CHECK_EQ(job_.scope, SweepingScope::Full);
    SweepFull();
  }
  job_.state = SweepingState::Swept;
}

void ArrayBufferSweeper::SweepFull() {
  CHECK_EQ(job_.scope, SweepingScope::Full);
  ArrayBufferList promoted = SweepListFull(&job_.young);
  ArrayBufferList survived = SweepListFull(&job_.old);

  job_.old = promoted;
  job_.old.Append(&survived);
}

ArrayBufferList ArrayBufferSweeper::SweepListFull(ArrayBufferList* list) {
  ArrayBufferExtension* current = list->head_;
  ArrayBufferList survivor_list;

  while (current) {
    ArrayBufferExtension* next = current->next();

    if (!current->IsMarked()) {
      size_t bytes = current->accounting_length();
      delete current;
      IncrementFreedBytes(bytes);
    } else {
      current->Unmark();
      survivor_list.Append(current);
    }

    current = next;
  }

  list->Reset();
  return survivor_list;
}

void ArrayBufferSweeper::SweepYoung() {
  CHECK_EQ(job_.scope, SweepingScope::Young);
  ArrayBufferExtension* current = job_.young.head_;

  ArrayBufferList new_young;
  ArrayBufferList new_old;

  while (current) {
    ArrayBufferExtension* next = current->next();

    if (!current->IsYoungMarked()) {
      size_t bytes = current->accounting_length();
      delete current;
      IncrementFreedBytes(bytes);
    } else if (current->IsYoungPromoted()) {
      current->YoungUnmark();
      new_old.Append(current);
    } else {
      current->YoungUnmark();
      new_young.Append(current);
    }

    current = next;
  }

  job_.old = new_old;
  job_.young = new_young;
}

void ArrayBufferSweeper::IncrementFreedBytes(size_t bytes) {
  if (bytes == 0) return;
  freed_bytes_.fetch_add(bytes);
}

}  // namespace internal
}  // namespace v8
