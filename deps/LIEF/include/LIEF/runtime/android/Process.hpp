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
#ifndef LIEF_RUNTIME_ANDROID_PROCESS_H
#define LIEF_RUNTIME_ANDROID_PROCESS_H
#include <LIEF/visibility.h>
#include <LIEF/runtime/Process.hpp>
#include <LIEF/runtime/android/Property.hpp>

#include <string>
#include <vector>

namespace LIEF {
namespace runtime {
namespace android {

/// This class exposes Android-specific API for the current process
class LIEF_API Process : public runtime::Process {
  public:
  using runtime::Process::Process;

  using properties_t = std::vector<Property>;

  /// Return the content of `/proc/cmdline`
  static std::string cmdline();

  /// Return the value of the Android system property with the given `name`
  /// (e.g. `ro.build.version.sdk`).
  static optional<Property> get_system_property(const std::string& name);

  /// Get all the system properties
  static properties_t properties();

  static constexpr bool classof(const runtime::Process*) {
    return platform() == PLATFORMS::ANDROID_;
  }
};

}
}
}
#endif
