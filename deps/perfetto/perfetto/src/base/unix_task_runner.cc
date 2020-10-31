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

#include "perfetto/base/build_config.h"
#if !PERFETTO_BUILDFLAG(PERFETTO_OS_WIN)

#include "perfetto/ext/base/unix_task_runner.h"

#include <errno.h>
#include <stdlib.h>
#include <unistd.h>

#include <limits>

#include "perfetto/ext/base/watchdog.h"

namespace perfetto {
namespace base {

UnixTaskRunner::UnixTaskRunner() {
  AddFileDescriptorWatch(event_.fd(), [] {
    // Not reached -- see PostFileDescriptorWatches().
    PERFETTO_DFATAL("Should be unreachable.");
  });
}

UnixTaskRunner::~UnixTaskRunner() = default;

void UnixTaskRunner::WakeUp() {
  event_.Notify();
}

void UnixTaskRunner::Run() {
  PERFETTO_DCHECK_THREAD(thread_checker_);
  created_thread_id_ = GetThreadId();
  quit_ = false;
  for (;;) {
    int poll_timeout_ms;
    {
      std::lock_guard<std::mutex> lock(lock_);
      if (quit_)
        return;
      poll_timeout_ms = GetDelayMsToNextTaskLocked();
      UpdateWatchTasksLocked();
    }
    int ret = PERFETTO_EINTR(poll(
        &poll_fds_[0], static_cast<nfds_t>(poll_fds_.size()), poll_timeout_ms));
    PERFETTO_CHECK(ret >= 0);

    // To avoid starvation we always interleave all types of tasks -- immediate,
    // delayed and file descriptor watches.
    PostFileDescriptorWatches();
    RunImmediateAndDelayedTask();
  }
}

void UnixTaskRunner::Quit() {
  std::lock_guard<std::mutex> lock(lock_);
  quit_ = true;
  WakeUp();
}

bool UnixTaskRunner::QuitCalled() {
  std::lock_guard<std::mutex> lock(lock_);
  return quit_;
}

bool UnixTaskRunner::IsIdleForTesting() {
  std::lock_guard<std::mutex> lock(lock_);
  return immediate_tasks_.empty();
}

void UnixTaskRunner::UpdateWatchTasksLocked() {
  PERFETTO_DCHECK_THREAD(thread_checker_);
  if (!watch_tasks_changed_)
    return;
  watch_tasks_changed_ = false;
  poll_fds_.clear();
  for (auto& it : watch_tasks_) {
    it.second.poll_fd_index = poll_fds_.size();
    poll_fds_.push_back({it.first, POLLIN | POLLHUP, 0});
  }
}

void UnixTaskRunner::RunImmediateAndDelayedTask() {
  // If locking overhead becomes an issue, add a separate work queue.
  std::function<void()> immediate_task;
  std::function<void()> delayed_task;
  TimeMillis now = GetWallTimeMs();
  {
    std::lock_guard<std::mutex> lock(lock_);
    if (!immediate_tasks_.empty()) {
      immediate_task = std::move(immediate_tasks_.front());
      immediate_tasks_.pop_front();
    }
    if (!delayed_tasks_.empty()) {
      auto it = delayed_tasks_.begin();
      if (now >= it->first) {
        delayed_task = std::move(it->second);
        delayed_tasks_.erase(it);
      }
    }
  }

  errno = 0;
  if (immediate_task)
    RunTaskWithWatchdogGuard(immediate_task);
  errno = 0;
  if (delayed_task)
    RunTaskWithWatchdogGuard(delayed_task);
}

void UnixTaskRunner::PostFileDescriptorWatches() {
  PERFETTO_DCHECK_THREAD(thread_checker_);
  for (size_t i = 0; i < poll_fds_.size(); i++) {
    if (!(poll_fds_[i].revents & (POLLIN | POLLHUP)))
      continue;
    poll_fds_[i].revents = 0;

    // The wake-up event is handled inline to avoid an infinite recursion of
    // posted tasks.
    if (poll_fds_[i].fd == event_.fd()) {
      event_.Clear();
      continue;
    }

    // Binding to |this| is safe since we are the only object executing the
    // task.
    PostTask(std::bind(&UnixTaskRunner::RunFileDescriptorWatch, this,
                       poll_fds_[i].fd));

    // Make the fd negative while a posted task is pending. This makes poll(2)
    // ignore the fd.
    PERFETTO_DCHECK(poll_fds_[i].fd >= 0);
    poll_fds_[i].fd = -poll_fds_[i].fd;
  }
}

void UnixTaskRunner::RunFileDescriptorWatch(int fd) {
  std::function<void()> task;
  {
    std::lock_guard<std::mutex> lock(lock_);
    auto it = watch_tasks_.find(fd);
    if (it == watch_tasks_.end())
      return;
    // Make poll(2) pay attention to the fd again. Since another thread may have
    // updated this watch we need to refresh the set first.
    UpdateWatchTasksLocked();
    size_t fd_index = it->second.poll_fd_index;
    PERFETTO_DCHECK(fd_index < poll_fds_.size());
    PERFETTO_DCHECK(::abs(poll_fds_[fd_index].fd) == fd);
    poll_fds_[fd_index].fd = fd;
    task = it->second.callback;
  }
  errno = 0;
  RunTaskWithWatchdogGuard(task);
}

int UnixTaskRunner::GetDelayMsToNextTaskLocked() const {
  PERFETTO_DCHECK_THREAD(thread_checker_);
  if (!immediate_tasks_.empty())
    return 0;
  if (!delayed_tasks_.empty()) {
    TimeMillis diff = delayed_tasks_.begin()->first - GetWallTimeMs();
    return std::max(0, static_cast<int>(diff.count()));
  }
  return -1;
}

void UnixTaskRunner::PostTask(std::function<void()> task) {
  bool was_empty;
  {
    std::lock_guard<std::mutex> lock(lock_);
    was_empty = immediate_tasks_.empty();
    immediate_tasks_.push_back(std::move(task));
  }
  if (was_empty)
    WakeUp();
}

void UnixTaskRunner::PostDelayedTask(std::function<void()> task,
                                     uint32_t delay_ms) {
  TimeMillis runtime = GetWallTimeMs() + TimeMillis(delay_ms);
  {
    std::lock_guard<std::mutex> lock(lock_);
    delayed_tasks_.insert(std::make_pair(runtime, std::move(task)));
  }
  WakeUp();
}

void UnixTaskRunner::AddFileDescriptorWatch(int fd,
                                            std::function<void()> task) {
  PERFETTO_DCHECK(fd >= 0);
  {
    std::lock_guard<std::mutex> lock(lock_);
    PERFETTO_DCHECK(!watch_tasks_.count(fd));
    watch_tasks_[fd] = {std::move(task), SIZE_MAX};
    watch_tasks_changed_ = true;
  }
  WakeUp();
}

void UnixTaskRunner::RemoveFileDescriptorWatch(int fd) {
  PERFETTO_DCHECK(fd >= 0);
  {
    std::lock_guard<std::mutex> lock(lock_);
    PERFETTO_DCHECK(watch_tasks_.count(fd));
    watch_tasks_.erase(fd);
    watch_tasks_changed_ = true;
  }
  // No need to schedule a wake-up for this.
}

bool UnixTaskRunner::RunsTasksOnCurrentThread() const {
  return GetThreadId() == created_thread_id_;
}

}  // namespace base
}  // namespace perfetto

#endif  // !PERFETTO_BUILDFLAG(PERFETTO_OS_WIN)
