/*
 * Copyright (C) 2017 The Android Open Source Project
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

#include "src/base/test/test_task_runner.h"

#include <stdio.h>
#include <unistd.h>

#include <chrono>

#include "perfetto/base/logging.h"

namespace perfetto {
namespace base {

TestTaskRunner::TestTaskRunner() = default;

TestTaskRunner::~TestTaskRunner() = default;

void TestTaskRunner::Run() {
  PERFETTO_DCHECK_THREAD(thread_checker_);
  for (;;)
    task_runner_.Run();
}

void TestTaskRunner::RunUntilIdle() {
  PERFETTO_DCHECK_THREAD(thread_checker_);
  task_runner_.PostTask(std::bind(&TestTaskRunner::QuitIfIdle, this));
  task_runner_.Run();
}

void TestTaskRunner::QuitIfIdle() {
  PERFETTO_DCHECK_THREAD(thread_checker_);
  if (task_runner_.IsIdleForTesting()) {
    task_runner_.Quit();
    return;
  }
  task_runner_.PostTask(std::bind(&TestTaskRunner::QuitIfIdle, this));
}

void TestTaskRunner::RunUntilCheckpoint(const std::string& checkpoint,
                                        uint32_t timeout_ms) {
  PERFETTO_DCHECK_THREAD(thread_checker_);
  if (checkpoints_.count(checkpoint) == 0) {
    fprintf(stderr, "[TestTaskRunner] Checkpoint \"%s\" does not exist.\n",
            checkpoint.c_str());
    abort();
  }
  if (checkpoints_[checkpoint])
    return;

  task_runner_.PostDelayedTask(
      [this, checkpoint] {
        if (checkpoints_[checkpoint])
          return;
        fprintf(stderr, "[TestTaskRunner] Failed to reach checkpoint \"%s\"\n",
                checkpoint.c_str());
        abort();
      },
      timeout_ms);

  pending_checkpoint_ = checkpoint;
  task_runner_.Run();
}

std::function<void()> TestTaskRunner::CreateCheckpoint(
    const std::string& checkpoint) {
  PERFETTO_DCHECK_THREAD(thread_checker_);
  PERFETTO_DCHECK(checkpoints_.count(checkpoint) == 0);
  auto checkpoint_iter = checkpoints_.emplace(checkpoint, false);
  return [this, checkpoint_iter] {
    PERFETTO_DCHECK_THREAD(thread_checker_);
    checkpoint_iter.first->second = true;
    if (pending_checkpoint_ == checkpoint_iter.first->first) {
      pending_checkpoint_.clear();
      task_runner_.Quit();
    }
  };
}

// TaskRunner implementation.
void TestTaskRunner::PostTask(std::function<void()> closure) {
  task_runner_.PostTask(std::move(closure));
}

void TestTaskRunner::PostDelayedTask(std::function<void()> closure,
                                     uint32_t delay_ms) {
  task_runner_.PostDelayedTask(std::move(closure), delay_ms);
}

void TestTaskRunner::AddFileDescriptorWatch(int fd,
                                            std::function<void()> callback) {
  task_runner_.AddFileDescriptorWatch(fd, std::move(callback));
}

void TestTaskRunner::RemoveFileDescriptorWatch(int fd) {
  task_runner_.RemoveFileDescriptorWatch(fd);
}

bool TestTaskRunner::RunsTasksOnCurrentThread() const {
  return task_runner_.RunsTasksOnCurrentThread();
}

}  // namespace base
}  // namespace perfetto
