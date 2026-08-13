// Copyright 2026 The Abseil Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "absl/time/clock_interface.h"

#include "absl/base/config.h"
#include "absl/base/internal/raw_logging.h"
#include "absl/base/no_destructor.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"

namespace absl {
ABSL_NAMESPACE_BEGIN

namespace {

class RealTimeClock final : public Clock {
 private:
#ifdef _MSC_VER
  // Disable MSVC warning "destructor never returns, potential memory leak"
#pragma warning(push)
#pragma warning(disable : 4722)
#endif
  ~RealTimeClock() override {
    ABSL_RAW_LOG(FATAL, "RealTimeClock should never be destroyed");
  }
#ifdef _MSC_VER
#pragma warning(pop)
#endif

 public:
  absl::Time TimeNow() override { return absl::Now(); }

  void Sleep(absl::Duration d) override { absl::SleepFor(d); }

  void SleepUntil(absl::Time wakeup_time) override {
    absl::Duration d = wakeup_time - TimeNow();
    if (d > absl::ZeroDuration()) {
      Sleep(d);
    }
  }

  bool AwaitWithDeadline(absl::Mutex* mu, const absl::Condition& cond,
                         absl::Time deadline) override {
    return mu->AwaitWithDeadline(cond, deadline);
  }
};

}  // namespace

Clock::~Clock() = default;  // go/key-method

Clock& Clock::GetRealClock() {
  static absl::NoDestructor<RealTimeClock> rtclock;
  return *rtclock;
}

ABSL_NAMESPACE_END
}  // namespace absl
