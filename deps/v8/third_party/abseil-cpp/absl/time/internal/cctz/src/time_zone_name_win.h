// Copyright 2025 Google Inc. All Rights Reserved.
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

#ifndef ABSL_TIME_INTERNAL_CCTZ_TIME_ZONE_NAME_WIN_H_
#define ABSL_TIME_INTERNAL_CCTZ_TIME_ZONE_NAME_WIN_H_

#include <string>

#include "absl/base/config.h"

namespace absl {
ABSL_NAMESPACE_BEGIN
namespace time_internal {
namespace cctz {

// Returns the local time zone ID in IANA format (e.g. "America/Los_Angeles"),
// or the empty string on failure. Not supported on Windows 10 1809 and earlier,
// where "icu.dll" is not available in the System32 directory.
std::string GetWindowsLocalTimeZone();

}  // namespace cctz
}  // namespace time_internal
ABSL_NAMESPACE_END
}  // namespace absl

#endif  // ABSL_TIME_INTERNAL_CCTZ_TIME_ZONE_NAME_WIN_H_
