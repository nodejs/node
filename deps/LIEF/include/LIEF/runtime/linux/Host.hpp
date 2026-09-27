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
#ifndef LIEF_RUNTIME_LINUX_HOST_H
#define LIEF_RUNTIME_LINUX_HOST_H
#include <LIEF/visibility.h>

#include <string>

namespace LIEF {
namespace runtime {
namespace Linux {

/// This class exposes Linux-specific host information.
class LIEF_API Host {
  public:
  /// Operating system name (e.g., `Linux`)
  static std::string sys_name();

  /// Operating system release (e.g., `2.6.28`)
  static std::string sys_release();

  /// Operating system version
  static std::string sys_version();

  /// Hardware type identifier (e.g., `x86_64`)
  static std::string hardware();
};


}
}
}
#endif
