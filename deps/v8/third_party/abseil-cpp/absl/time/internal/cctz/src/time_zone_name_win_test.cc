// Copyright 2026 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//   https://www.apache.org/licenses/LICENSE-2.0
//
//   Unless required by applicable law or agreed to in writing, software
//   distributed under the License is distributed on an "AS IS" BASIS,
//   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//   See the License for the specific language governing permissions and
//   limitations under the License.

#include "absl/time/internal/cctz/src/time_zone_name_win.h"

#include <windows.h>

#include <string>

#include "gtest/gtest.h"
#include "absl/base/config.h"
#include "absl/time/internal/cctz/include/cctz/time_zone.h"

namespace absl {
ABSL_NAMESPACE_BEGIN
namespace time_internal {
namespace cctz {

TEST(TimeZoneNameWin, GetWindowsLocalTimeZone) {
  // On Windows 10 1809+ (where icu.dll is available in System32),
  // GetWindowsLocalTimeZone() should return a valid IANA time zone name.
  // Note that LOAD_LIBRARY_SEARCH_SYSTEM32 is not sufficient to reliably load
  // "icu.dll" in the production code, but it should be OK for testing purposes.
  HMODULE icu_dll =
      ::LoadLibraryExW(L"icu.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
  const std::string tz = GetWindowsLocalTimeZone();
  if (icu_dll != nullptr) {
    EXPECT_FALSE(tz.empty());
    ::FreeLibrary(icu_dll);
  } else {
    EXPECT_TRUE(tz.empty());
  }
}

}  // namespace cctz
}  // namespace time_internal
ABSL_NAMESPACE_END
}  // namespace absl
