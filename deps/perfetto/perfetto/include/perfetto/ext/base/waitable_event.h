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

#ifndef INCLUDE_PERFETTO_EXT_BASE_WAITABLE_EVENT_H_
#define INCLUDE_PERFETTO_EXT_BASE_WAITABLE_EVENT_H_

#include <condition_variable>
#include <mutex>

namespace perfetto {
namespace base {

// A waitable event for cross-thread synchronization.
// All methods on this class can be called from any thread.
class WaitableEvent {
 public:
  WaitableEvent();
  ~WaitableEvent();
  WaitableEvent(const WaitableEvent&) = delete;
  WaitableEvent operator=(const WaitableEvent&) = delete;

  // Synchronously block until the event is notified.
  void Wait();

  // Signal the event, waking up blocked waiters.
  void Notify();

 private:
  std::mutex mutex_;
  std::condition_variable event_;
  bool notified_ = false;
};

}  // namespace base
}  // namespace perfetto

#endif  // INCLUDE_PERFETTO_EXT_BASE_WAITABLE_EVENT_H_
