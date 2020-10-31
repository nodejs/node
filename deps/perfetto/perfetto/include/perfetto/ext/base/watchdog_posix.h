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

#ifndef INCLUDE_PERFETTO_EXT_BASE_WATCHDOG_POSIX_H_
#define INCLUDE_PERFETTO_EXT_BASE_WATCHDOG_POSIX_H_

#include "perfetto/ext/base/thread_checker.h"

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

namespace perfetto {
namespace base {

struct ProcStat {
  unsigned long int utime = 0l;
  unsigned long int stime = 0l;
  long int rss_pages = -1l;
};

bool ReadProcStat(int fd, ProcStat* out);

// Ensures that the calling program does not exceed certain hard limits on
// resource usage e.g. time, memory and CPU. If exceeded, the program is
// crashed.
class Watchdog {
 public:
  // Handle to the timer set to crash the program. If the handle is dropped,
  // the timer is removed so the program does not crash.
  class Timer {
   public:
    ~Timer();
    Timer(Timer&&) noexcept;

   private:
    friend class Watchdog;

    explicit Timer(uint32_t ms);
    Timer(const Timer&) = delete;
    Timer& operator=(const Timer&) = delete;

    timer_t timerid_ = nullptr;
  };
  virtual ~Watchdog();

  static Watchdog* GetInstance();

  // Sets a timer which will crash the program in |ms| milliseconds if the
  // returned handle is not destroyed before this point.
  Timer CreateFatalTimer(uint32_t ms);

  // Starts the watchdog thread which monitors the memory and CPU usage
  // of the program.
  void Start();

  // Sets a limit on the memory (defined as the RSS) used by the program
  // averaged over the last |window_ms| milliseconds. If |kb| is 0, any
  // existing limit is removed.
  // Note: |window_ms| has to be a multiple of |polling_interval_ms_|.
  void SetMemoryLimit(uint64_t bytes, uint32_t window_ms);

  // Sets a limit on the CPU usage used by the program averaged over the last
  // |window_ms| milliseconds. If |percentage| is 0, any existing limit is
  // removed.
  // Note: |window_ms| has to be a multiple of |polling_interval_ms_|.
  void SetCpuLimit(uint32_t percentage, uint32_t window_ms);

 protected:
  // Protected for testing.
  Watchdog(uint32_t polling_interval_ms);

 private:
  // Represents a ring buffer in which integer values can be stored.
  class WindowedInterval {
   public:
    // Pushes a new value into a ring buffer wrapping if necessary and returns
    // whether the ring buffer is full.
    bool Push(uint64_t sample);

    // Returns the mean of the values in the buffer.
    double Mean() const;

    // Clears the ring buffer while keeping the existing size.
    void Clear();

    // Resets the size of the buffer as well as clearing it.
    void Reset(size_t new_size);

    // Gets the oldest value inserted in the buffer. The buffer must be full
    // (i.e. Push returned true) before this method can be called.
    uint64_t OldestWhenFull() const {
      PERFETTO_CHECK(filled_);
      return buffer_[position_];
    }

    // Gets the newest value inserted in the buffer. The buffer must be full
    // (i.e. Push returned true) before this method can be called.
    uint64_t NewestWhenFull() const {
      PERFETTO_CHECK(filled_);
      return buffer_[(position_ + size_ - 1) % size_];
    }

    // Returns the size of the ring buffer.
    size_t size() const { return size_; }

   private:
    bool filled_ = false;
    size_t position_ = 0;
    size_t size_ = 0;
    std::unique_ptr<uint64_t[]> buffer_;
  };

  explicit Watchdog(const Watchdog&) = delete;
  Watchdog& operator=(const Watchdog&) = delete;

  // Main method for the watchdog thread.
  void ThreadMain();

  // Check each type of resource every |polling_interval_ms_| miillis.
  void CheckMemory(uint64_t rss_bytes);
  void CheckCpu(uint64_t cpu_time);

  // Computes the time interval spanned by a given ring buffer with respect
  // to |polling_interval_ms_|.
  uint32_t WindowTimeForRingBuffer(const WindowedInterval& window);

  const uint32_t polling_interval_ms_;
  std::atomic<bool> enabled_{false};
  std::thread thread_;
  std::condition_variable exit_signal_;

  // --- Begin lock-protected members ---

  std::mutex mutex_;

  uint64_t memory_limit_bytes_ = 0;
  WindowedInterval memory_window_bytes_;

  uint32_t cpu_limit_percentage_ = 0;
  WindowedInterval cpu_window_time_ticks_;

  // --- End lock-protected members ---
};

}  // namespace base
}  // namespace perfetto
#endif  // INCLUDE_PERFETTO_EXT_BASE_WATCHDOG_POSIX_H_
