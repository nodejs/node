// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef V8_INTL_SUPPORT

#include <stdlib.h>

#include "include/v8.h"
#include "src/objects/objects-inl.h"
#include "test/cctest/cctest.h"
#include "unicode/locid.h"

namespace {
void CheckLocaleSpecificValues(const char* locale, const char* date,
                               const char* number) {
  CHECK(v8_str(locale)->StrictEquals(
      CompileRun("Intl.NumberFormat().resolvedOptions().locale")));
  CHECK(v8_str(date)->StrictEquals(
      CompileRun("new Date('02/14/2020 13:45').toLocaleString()")));
  CHECK(v8_str(number)->StrictEquals(
      CompileRun("Number(10000.3).toLocaleString()")));
}

void SetIcuLocale(const char* locale_name) {
  UErrorCode error_code = U_ZERO_ERROR;
  icu::Locale locale(locale_name);
  icu::Locale::setDefault(locale, error_code);
  CHECK(U_SUCCESS(error_code));
}
}  // namespace

TEST(LocaleConfigurationChangeNotification) {
  icu::Locale default_locale = icu::Locale::getDefault();

  LocalContext context;
  v8::Isolate* isolate = context->GetIsolate();
  v8::HandleScope scope(isolate);

  SetIcuLocale("en_US");
  isolate->LocaleConfigurationChangeNotification();
  CheckLocaleSpecificValues("en-US", "2/14/2020, 1:45:00 PM", "10,000.3");

  SetIcuLocale("ru_RU");
  isolate->LocaleConfigurationChangeNotification();
  CheckLocaleSpecificValues("ru-RU", "14.02.2020, 13:45:00", "10 000,3");

  SetIcuLocale("zh_CN");
  isolate->LocaleConfigurationChangeNotification();
  CheckLocaleSpecificValues("zh-CN", "2020/2/14 下午1:45:00", "10,000.3");

  UErrorCode error_code = U_ZERO_ERROR;
  icu::Locale::setDefault(default_locale, error_code);
  CHECK(U_SUCCESS(error_code));
}

#endif  // V8_INTL_SUPPORT
