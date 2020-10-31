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

#ifndef INCLUDE_PERFETTO_EXT_BASE_WATCHDOG_NOOP_H_
#define INCLUDE_PERFETTO_EXT_BASE_WATCHDOG_NOOP_H_

#include <stdint.h>

namespace perfetto {
namespace base {

class Watchdog {
 public:
  class Timer {
   public:
    // Define an empty dtor to avoid "unused variable" errors on the call site.
    Timer() {}
    Timer(const Timer&) {}
    ~Timer() {}
  };
  static Watchdog* GetInstance() {
    static Watchdog* watchdog = new Watchdog();
    return watchdog;
  }
  Timer CreateFatalTimer(uint32_t /*ms*/) { return Timer(); }
  void Start() {}
  void SetMemoryLimit(uint64_t /*bytes*/, uint32_t /*window_ms*/) {}
  void SetCpuLimit(uint32_t /*percentage*/, uint32_t /*window_ms*/) {}
};

}  // namespace base
}  // namespace perfetto

#endif  // INCLUDE_PERFETTO_EXT_BASE_WATCHDOG_NOOP_H_
