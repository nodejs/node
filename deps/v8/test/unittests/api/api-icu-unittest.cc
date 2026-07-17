// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef V8_INTL_SUPPORT

#include <stdlib.h>

#include "include/v8-isolate.h"
#include "include/v8-local-handle.h"
#include "src/objects/objects-inl.h"
#include "test/unittests/test-utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "unicode/locid.h"

class ApiIcuTest : public v8::TestWithContext {
 public:
  void CheckLocaleSpecificValues(const char* locale, const char* date,
                                 const char* number) {
    CHECK(NewString(locale)->StrictEquals(
        RunJS("Intl.NumberFormat().resolvedOptions().locale")));
    CHECK(NewString(date)->StrictEquals(
        RunJS("new Date('02/14/2020 13:45').toLocaleString()")));
    CHECK(NewString(number)->StrictEquals(
        RunJS("Number(10000.3).toLocaleString()")));
  }

  void SetIcuLocale(const char* locale_name) {
    UErrorCode error_code = U_ZERO_ERROR;
    icu::Locale locale(locale_name);
    icu::Locale::setDefault(locale, error_code);
    CHECK(U_SUCCESS(error_code));
  }
};

TEST_F(ApiIcuTest, LocaleConfigurationChangeNotification) {
  icu::Locale default_locale = icu::Locale::getDefault();

  SetIcuLocale("en_US");
  isolate()->LocaleConfigurationChangeNotification();
  CheckLocaleSpecificValues("en-US", "2/14/2020, 1:45:00 PM", "10,000.3");

  SetIcuLocale("ru_RU");
  isolate()->LocaleConfigurationChangeNotification();
  CheckLocaleSpecificValues("ru-RU", "14.02.2020, 13:45:00", "10 000,3");

  SetIcuLocale("zh_CN");
  isolate()->LocaleConfigurationChangeNotification();
  CheckLocaleSpecificValues("zh-CN", "2020/2/14 13:45:00", "10,000.3");

  UErrorCode error_code = U_ZERO_ERROR;
  icu::Locale::setDefault(default_locale, error_code);
  CHECK(U_SUCCESS(error_code));
}

#endif  // V8_INTL_SUPPORT
