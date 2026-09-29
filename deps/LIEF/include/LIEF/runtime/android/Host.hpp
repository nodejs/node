/* Copyright 2017 - 2026 R. Thomas
 * Copyright 2017 - 2026 Quarkslab
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
#ifndef LIEF_RUNTIME_ANDROID_HOST_H
#define LIEF_RUNTIME_ANDROID_HOST_H
#include <LIEF/visibility.h>
#include <LIEF/optional.hpp>

#include <cstdint>

namespace LIEF {
namespace runtime {
namespace android {

/// This class exposes Android-specific host information.
class LIEF_API Host {
  public:
  /// Return the Android SDK/API level of the device (e.g. `34` for Android
  /// 14).
  static optional<uint32_t> sdk_version();
};


}
}
}
#endif
