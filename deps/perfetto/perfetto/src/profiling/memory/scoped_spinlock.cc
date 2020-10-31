/*
 * Copyright (C) 2019 The Android Open Source Project
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

#include "src/profiling/memory/scoped_spinlock.h"

#include <unistd.h>

#include <atomic>

#include "perfetto/ext/base/utils.h"

namespace {
// Wait for ~1s before timing out (+- spurious wakeups from the sleeps).
constexpr unsigned kSleepAttempts = 1000;
constexpr unsigned kLockAttemptsPerSleep = 1000;
constexpr unsigned kSleepDurationUs = 1000;
}  // namespace

namespace perfetto {
namespace profiling {

void ScopedSpinlock::LockSlow(Mode mode) {
  for (size_t attempt = 0; mode == Mode::Blocking ||
                           attempt < kLockAttemptsPerSleep * kSleepAttempts;
       attempt++) {
    if (!lock_->load(std::memory_order_relaxed) &&
        PERFETTO_LIKELY(!lock_->exchange(true, std::memory_order_acquire))) {
      locked_ = true;
      return;
    }
    if (attempt && attempt % kLockAttemptsPerSleep == 0)
      usleep(kSleepDurationUs);
  }
}

}  // namespace profiling
}  // namespace perfetto
