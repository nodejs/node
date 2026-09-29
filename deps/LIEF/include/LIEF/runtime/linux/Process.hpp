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
#ifndef LIEF_RUNTIME_LINUX_PROCESS_H
#define LIEF_RUNTIME_LINUX_PROCESS_H
#include <LIEF/visibility.h>
#include <LIEF/runtime/Process.hpp>
#include <string>

namespace LIEF {
namespace runtime {
namespace Linux {

/// This class exposes Linux-specific API for the current process
class LIEF_API Process : public runtime::Process {
  public:
  using runtime::Process::Process;

  /// Return the content of `/proc/cmdline`
  static std::string cmdline();

  /// Return the version of the GNU C Library (glibc) loaded in the current
  /// process (e.g. ``2.39``).
  ///
  /// Return an empty string if the version cannot be determined.
  static std::string glibc_version();

  static constexpr bool classof(const runtime::Process*) {
    return platform() == PLATFORMS::LINUX;
  }
};

}
}
}
#endif
