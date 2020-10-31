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

#include "perfetto/ext/base/thread_checker.h"

#if PERFETTO_BUILDFLAG(PERFETTO_OS_WIN)
#include <Windows.h>
#endif

namespace perfetto {
namespace base {

namespace {
constexpr ThreadID kDetached{};

ThreadID CurrentThreadId() {
#if PERFETTO_BUILDFLAG(PERFETTO_OS_WIN)
  return ::GetCurrentThreadId();
#else
  return pthread_self();
#endif
}
}  // namespace

ThreadChecker::ThreadChecker() {
  thread_id_.store(CurrentThreadId());
}

ThreadChecker::~ThreadChecker() = default;

ThreadChecker::ThreadChecker(const ThreadChecker& other) {
  thread_id_ = other.thread_id_.load();
}

ThreadChecker& ThreadChecker::operator=(const ThreadChecker& other) {
  thread_id_ = other.thread_id_.load();
  return *this;
}

bool ThreadChecker::CalledOnValidThread() const {
  auto self = CurrentThreadId();

  // Will re-attach if previously detached using DetachFromThread().
  auto prev_value = kDetached;
  if (thread_id_.compare_exchange_strong(prev_value, self))
    return true;
  return prev_value == self;
}

void ThreadChecker::DetachFromThread() {
  thread_id_.store(kDetached);
}

}  // namespace base
}  // namespace perfetto
