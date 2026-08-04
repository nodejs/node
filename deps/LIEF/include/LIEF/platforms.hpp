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
#ifndef LIEF_PLATFORMS_H
#define LIEF_PLATFORMS_H
#include "LIEF/platforms/android.hpp"

#if defined(__APPLE__)
  #include "TargetConditionals.h"
#endif

namespace LIEF {

enum PLATFORMS {
  PLAT_UNKNOWN = 0,
  PLAT_LINUX,
  PLAT_ANDROID,
  PLAT_WINDOWS,
  PLAT_IOS,
  PLAT_OSX,
};

constexpr PLATFORMS current_platform() {
#if defined(__ANDROID__)
  return PLATFORMS::PLAT_ANDROID;
#elif defined(__linux__)
  return PLATFORMS::PLAT_LINUX;
#elif defined(_WIN64) || defined(_WIN32)
  return PLATFORMS::PLAT_WINDOWS;
#elif defined(__APPLE__)
  #if defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE
    return PLATFORMS::PLAT_IOS;
  #else
    return PLATFORMS::PLAT_OSX;
  #endif
#else
  return PLATFORMS::PLAT_UNKNOWN;
#endif

}


}

#endif
