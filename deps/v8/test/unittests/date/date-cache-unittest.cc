// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifdef V8_INTL_SUPPORT
#include "src/base/platform/platform.h"
#include "src/date/date.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "unicode/strenum.h"
#include "unicode/timezone.h"

namespace v8 {
namespace internal {

// A recent time for the test.
// 2019-05-08T04:16:04.845Z
static const int64_t kStartTime = 1557288964845;

class AdoptDefaultThread final : public base::Thread {
 public:
  AdoptDefaultThread() : base::Thread(Options("AdoptDefault")) {}

  void Run() override {
    printf("AdoptDefaultThread Start\n");
    std::unique_ptr<icu::StringEnumeration> timezones(
        icu::TimeZone::createEnumeration());
    UErrorCode status = U_ZERO_ERROR;
    const icu::UnicodeString* timezone = timezones->snext(status);
    icu::TimeZone::adoptDefault(icu::TimeZone::createTimeZone(*timezone));
    printf("AdoptDefaultThread End\n");
  }
};

class GetLocalOffsetFromOSThread final : public base::Thread {
 public:
  explicit GetLocalOffsetFromOSThread(bool utc)
      : base::Thread(Options("GetLocalOffsetFromOS")), utc_(utc) {}

  void Run() override {
    printf("GetLocalOffsetFromOSThread Start\n");
    DateCache date_cache;
    date_cache.GetLocalOffsetFromOS(kStartTime, utc_);
    printf("GetLocalOffsetFromOSThread End\n");
  }

 private:
  bool utc_;
};

class LocalTimezoneThread final : public base::Thread {
 public:
  LocalTimezoneThread() : base::Thread(Options("LocalTimezone")) {}

  void Run() override {
    printf("LocalTimezoneThread Start\n");
    DateCache date_cache;
    date_cache.LocalTimezone(kStartTime);
    printf("LocalTimezoneThread End\n");
  }
};

TEST(DateCache, AdoptDefaultFirst) {
  AdoptDefaultThread t1;
  GetLocalOffsetFromOSThread t2(true);
  GetLocalOffsetFromOSThread t3(false);
  LocalTimezoneThread t4;

  // The AdoptDefaultFirst will always pass. Just a test to ensure
  // our testing code itself is correct.
  // We finish all the operation AdoptDefaultThread before
  // running all other thread so it won't show the problem of
  // AdoptDefault trashing newly create default.
  CHECK(t1.Start());
  t1.Join();

  CHECK(t2.Start());
  CHECK(t3.Start());
  CHECK(t4.Start());

  t2.Join();
  t3.Join();
  t4.Join();
}

TEST(DateCache, AdoptDefaultMixed) {
  AdoptDefaultThread t1;
  GetLocalOffsetFromOSThread t2(true);
  GetLocalOffsetFromOSThread t3(false);
  LocalTimezoneThread t4;

  // The AdoptDefaultMixed run AdoptDefaultThread concurrently
  // with other thread so if the AdoptDefault is not thread safe
  // it will cause crash in other thread because the TimeZone
  // newly created by createDefault could be trashed by AdoptDefault
  // while a deleted DEFAULT_ZONE got cloned.
  CHECK(t1.Start());
  CHECK(t2.Start());
  CHECK(t3.Start());
  CHECK(t4.Start());

  t1.Join();
  t2.Join();
  t3.Join();
  t4.Join();
}

}  // namespace internal
}  // namespace v8

#endif  // V8_INTL_SUPPORT
