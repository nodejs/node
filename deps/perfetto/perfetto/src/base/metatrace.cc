/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "perfetto/ext/base/metatrace.h"

#include "perfetto/base/compiler.h"
#include "perfetto/base/task_runner.h"
#include "perfetto/base/time.h"
#include "perfetto/ext/base/file_utils.h"

namespace perfetto {
namespace metatrace {

std::atomic<uint32_t> g_enabled_tags{0};
std::atomic<uint64_t> g_enabled_timestamp{0};

// static members
constexpr size_t RingBuffer::kCapacity;
std::array<Record, RingBuffer::kCapacity> RingBuffer::records_;
std::atomic<bool> RingBuffer::read_task_queued_;
std::atomic<uint64_t> RingBuffer::wr_index_;
std::atomic<uint64_t> RingBuffer::rd_index_;
std::atomic<bool> RingBuffer::has_overruns_;
Record RingBuffer::bankruptcy_record_;

constexpr uint16_t Record::kTypeMask;
constexpr uint16_t Record::kTypeCounter;
constexpr uint16_t Record::kTypeEvent;

namespace {

// std::function<> is not trivially de/constructible. This struct wraps it in a
// heap-allocated struct to avoid static initializers.
struct Delegate {
  static Delegate* GetInstance() {
    static Delegate* instance = new Delegate();
    return instance;
  }

  base::TaskRunner* task_runner = nullptr;
  std::function<void()> read_task;
};

}  // namespace

bool Enable(std::function<void()> read_task,
            base::TaskRunner* task_runner,
            uint32_t tags) {
  PERFETTO_DCHECK(read_task);
  PERFETTO_DCHECK(task_runner->RunsTasksOnCurrentThread());
  if (g_enabled_tags.load(std::memory_order_acquire))
    return false;

  Delegate* dg = Delegate::GetInstance();
  dg->task_runner = task_runner;
  dg->read_task = std::move(read_task);
  RingBuffer::Reset();
  g_enabled_timestamp.store(TraceTimeNowNs(), std::memory_order_relaxed);
  g_enabled_tags.store(tags, std::memory_order_release);
  return true;
}

void Disable() {
  g_enabled_tags.store(0, std::memory_order_release);
  Delegate* dg = Delegate::GetInstance();
  PERFETTO_DCHECK(!dg->task_runner ||
                  dg->task_runner->RunsTasksOnCurrentThread());
  dg->task_runner = nullptr;
  dg->read_task = nullptr;
}

// static
void RingBuffer::Reset() {
  bankruptcy_record_.clear();
  for (Record& record : records_)
    record.clear();
  wr_index_ = 0;
  rd_index_ = 0;
  has_overruns_ = false;
  read_task_queued_ = false;
}

// static
Record* RingBuffer::AppendNewRecord() {
  auto wr_index = wr_index_.fetch_add(1, std::memory_order_acq_rel);

  // rd_index can only monotonically increase, we don't care if we read an
  // older value, we'll just hit the slow-path a bit earlier if it happens.
  auto rd_index = rd_index_.load(std::memory_order_relaxed);

  PERFETTO_DCHECK(wr_index >= rd_index);
  auto size = wr_index - rd_index;
  if (PERFETTO_LIKELY(size < kCapacity / 2))
    return At(wr_index);

  // Slow-path: Enqueue the read task and handle overruns.
  bool expected = false;
  if (RingBuffer::read_task_queued_.compare_exchange_strong(expected, true)) {
    Delegate* dg = Delegate::GetInstance();
    if (dg->task_runner) {
      dg->task_runner->PostTask([] {
        // Meta-tracing might have been disabled in the meantime.
        auto read_task = Delegate::GetInstance()->read_task;
        if (read_task)
          read_task();
        RingBuffer::read_task_queued_ = false;
      });
    }
  }

  if (PERFETTO_LIKELY(size < kCapacity))
    return At(wr_index);

  has_overruns_.store(true, std::memory_order_release);
  wr_index_.fetch_sub(1, std::memory_order_acq_rel);

  // In the case of overflows, threads will race writing on the same memory
  // location and TSan will rightly complain. This is fine though because nobody
  // will read the bankruptcy record and it's designed to contain garbage.
  PERFETTO_ANNOTATE_BENIGN_RACE_SIZED(&bankruptcy_record_, sizeof(Record),
                                      "nothing reads bankruptcy_record_")
  return &bankruptcy_record_;
}

// static
bool RingBuffer::IsOnValidTaskRunner() {
  auto* task_runner = Delegate::GetInstance()->task_runner;
  return task_runner && task_runner->RunsTasksOnCurrentThread();
}

}  // namespace metatrace
}  // namespace perfetto
