/* Copyright 2017 - 2025 R. Thomas
 * Copyright 2017 - 2025 Quarkslab
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef LIEF_PLATFORMS_ANDROID_VERSIONS_H
#define LIEF_PLATFORMS_ANDROID_VERSIONS_H
#include "LIEF/visibility.h"

namespace LIEF {
namespace Android {

enum class ANDROID_VERSIONS {
  VERSION_UNKNOWN = 0,
  VERSION_601     = 1,

  VERSION_700     = 2,
  VERSION_710     = 3,
  VERSION_712     = 4,

  VERSION_800     = 5,
  VERSION_810     = 6,

  VERSION_900     = 7,
};

LIEF_API const char* code_name(ANDROID_VERSIONS version);
LIEF_API const char* version_string(ANDROID_VERSIONS version);
LIEF_API const char* to_string(ANDROID_VERSIONS version);


}
}
#endif
