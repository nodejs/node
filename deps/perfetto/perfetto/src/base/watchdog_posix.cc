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

#include "perfetto/ext/base/watchdog.h"

#if PERFETTO_BUILDFLAG(PERFETTO_WATCHDOG)

#include <fcntl.h>
#include <inttypes.h>
#include <signal.h>
#include <stdint.h>

#include <fstream>
#include <thread>

#include "perfetto/base/build_config.h"
#include "perfetto/base/logging.h"
#include "perfetto/base/thread_utils.h"
#include "perfetto/ext/base/scoped_file.h"

namespace perfetto {
namespace base {

namespace {

constexpr uint32_t kDefaultPollingInterval = 30 * 1000;

bool IsMultipleOf(uint32_t number, uint32_t divisor) {
  return number >= divisor && number % divisor == 0;
}

double MeanForArray(const uint64_t array[], size_t size) {
  uint64_t total = 0;
  for (size_t i = 0; i < size; i++) {
    total += array[i];
  }
  return static_cast<double>(total / size);

}

}  //  namespace

bool ReadProcStat(int fd, ProcStat* out) {
  char c[512];
  size_t c_pos = 0;
  while (c_pos < sizeof(c) - 1) {
    ssize_t rd = PERFETTO_EINTR(read(fd, c + c_pos, sizeof(c) - c_pos));
    if (rd < 0) {
      PERFETTO_ELOG("Failed to read stat file to enforce resource limits.");
      return false;
    }
    if (rd == 0)
      break;
    c_pos += static_cast<size_t>(rd);
  }
  PERFETTO_CHECK(c_pos < sizeof(c));
  c[c_pos] = '\0';

  if (sscanf(c,
             "%*d %*s %*c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %lu"
             "%lu %*d %*d %*d %*d %*d %*d %*u %*u %ld",
             &out->utime, &out->stime, &out->rss_pages) != 3) {
    PERFETTO_ELOG("Invalid stat format: %s", c);
    return false;
  }
  return true;
}

Watchdog::Watchdog(uint32_t polling_interval_ms)
    : polling_interval_ms_(polling_interval_ms) {}

Watchdog::~Watchdog() {
  if (!thread_.joinable()) {
    PERFETTO_DCHECK(!enabled_);
    return;
  }
  PERFETTO_DCHECK(enabled_);
  enabled_ = false;
  exit_signal_.notify_one();
  thread_.join();
}

Watchdog* Watchdog::GetInstance() {
  static Watchdog* watchdog = new Watchdog(kDefaultPollingInterval);
  return watchdog;
}

Watchdog::Timer Watchdog::CreateFatalTimer(uint32_t ms) {
  if (!enabled_.load(std::memory_order_relaxed))
    return Watchdog::Timer(0);

  return Watchdog::Timer(ms);
}

void Watchdog::Start() {
  std::lock_guard<std::mutex> guard(mutex_);
  if (thread_.joinable()) {
    PERFETTO_DCHECK(enabled_);
  } else {
    PERFETTO_DCHECK(!enabled_);

#if PERFETTO_BUILDFLAG(PERFETTO_OS_LINUX) || \
    PERFETTO_BUILDFLAG(PERFETTO_OS_ANDROID)
    // Kick the thread to start running but only on Android or Linux.
    enabled_ = true;
    thread_ = std::thread(&Watchdog::ThreadMain, this);
#endif
  }
}

void Watchdog::SetMemoryLimit(uint64_t bytes, uint32_t window_ms) {
  // Update the fields under the lock.
  std::lock_guard<std::mutex> guard(mutex_);

  PERFETTO_CHECK(IsMultipleOf(window_ms, polling_interval_ms_) || bytes == 0);

  size_t size = bytes == 0 ? 0 : window_ms / polling_interval_ms_ + 1;
  memory_window_bytes_.Reset(size);
  memory_limit_bytes_ = bytes;
}

void Watchdog::SetCpuLimit(uint32_t percentage, uint32_t window_ms) {
  std::lock_guard<std::mutex> guard(mutex_);

  PERFETTO_CHECK(percentage <= 100);
  PERFETTO_CHECK(IsMultipleOf(window_ms, polling_interval_ms_) ||
                 percentage == 0);

  size_t size = percentage == 0 ? 0 : window_ms / polling_interval_ms_ + 1;
  cpu_window_time_ticks_.Reset(size);
  cpu_limit_percentage_ = percentage;
}

void Watchdog::ThreadMain() {
  base::ScopedFile stat_fd(base::OpenFile("/proc/self/stat", O_RDONLY));
  if (!stat_fd) {
    PERFETTO_ELOG("Failed to open stat file to enforce resource limits.");
    return;
  }

  std::unique_lock<std::mutex> guard(mutex_);
  for (;;) {
    exit_signal_.wait_for(guard,
                          std::chrono::milliseconds(polling_interval_ms_));
    if (!enabled_)
      return;

    lseek(stat_fd.get(), 0, SEEK_SET);

    ProcStat stat;
    if (!ReadProcStat(stat_fd.get(), &stat)) {
      return;
    }

    uint64_t cpu_time = stat.utime + stat.stime;
    uint64_t rss_bytes =
        static_cast<uint64_t>(stat.rss_pages) * base::kPageSize;

    CheckMemory(rss_bytes);
    CheckCpu(cpu_time);
  }
}

void Watchdog::CheckMemory(uint64_t rss_bytes) {
  if (memory_limit_bytes_ == 0)
    return;

  // Add the current stat value to the ring buffer and check that the mean
  // remains under our threshold.
  if (memory_window_bytes_.Push(rss_bytes)) {
    if (memory_window_bytes_.Mean() > static_cast<double>(memory_limit_bytes_)) {
      PERFETTO_ELOG(
          "Memory watchdog trigger. Memory window of %f bytes is above the "
          "%" PRIu64 " bytes limit.",
          memory_window_bytes_.Mean(), memory_limit_bytes_);
      kill(getpid(), SIGABRT);
    }
  }
}

void Watchdog::CheckCpu(uint64_t cpu_time) {
  if (cpu_limit_percentage_ == 0)
    return;

  // Add the cpu time to the ring buffer.
  if (cpu_window_time_ticks_.Push(cpu_time)) {
    // Compute the percentage over the whole window and check that it remains
    // under the threshold.
    uint64_t difference_ticks = cpu_window_time_ticks_.NewestWhenFull() -
                                cpu_window_time_ticks_.OldestWhenFull();
    double window_interval_ticks =
        (static_cast<double>(WindowTimeForRingBuffer(cpu_window_time_ticks_)) /
         1000.0) *
        static_cast<double>(sysconf(_SC_CLK_TCK));
    double percentage = static_cast<double>(difference_ticks) /
                        static_cast<double>(window_interval_ticks) * 100;
    if (percentage > cpu_limit_percentage_) {
      PERFETTO_ELOG("CPU watchdog trigger. %f%% CPU use is above the %" PRIu32
                    "%% CPU limit.",
                    percentage, cpu_limit_percentage_);
      kill(getpid(), SIGABRT);
    }
  }
}

uint32_t Watchdog::WindowTimeForRingBuffer(const WindowedInterval& window) {
  return static_cast<uint32_t>(window.size() - 1) * polling_interval_ms_;
}

bool Watchdog::WindowedInterval::Push(uint64_t sample) {
  // Add the sample to the current position in the ring buffer.
  buffer_[position_] = sample;

  // Update the position with next one circularily.
  position_ = (position_ + 1) % size_;

  // Set the filled flag the first time we wrap.
  filled_ = filled_ || position_ == 0;
  return filled_;
}

double Watchdog::WindowedInterval::Mean() const {
  return MeanForArray(buffer_.get(), size_);
}

void Watchdog::WindowedInterval::Clear() {
  position_ = 0;
  buffer_.reset(new uint64_t[size_]());
}

void Watchdog::WindowedInterval::Reset(size_t new_size) {
  position_ = 0;
  size_ = new_size;
  buffer_.reset(new_size == 0 ? nullptr : new uint64_t[new_size]());
}

Watchdog::Timer::Timer(uint32_t ms) {
  if (!ms)
    return;  // No-op timer created when the watchdog is disabled.

  struct sigevent sev = {};
  sev.sigev_notify = SIGEV_THREAD_ID;
  sev._sigev_un._tid = base::GetThreadId();
  sev.sigev_signo = SIGABRT;
  PERFETTO_CHECK(timer_create(CLOCK_MONOTONIC, &sev, &timerid_) != -1);
  struct itimerspec its = {};
  its.it_value.tv_sec = ms / 1000;
  its.it_value.tv_nsec = 1000000L * (ms % 1000);
  PERFETTO_CHECK(timer_settime(timerid_, 0, &its, nullptr) != -1);
}

Watchdog::Timer::~Timer() {
  if (timerid_ != nullptr) {
    timer_delete(timerid_);
  }
}

Watchdog::Timer::Timer(Timer&& other) noexcept {
  timerid_ = other.timerid_;
  other.timerid_ = nullptr;
}

}  // namespace base
}  // namespace perfetto

#endif  // PERFETTO_BUILDFLAG(PERFETTO_WATCHDOG)
