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
#ifndef LIEF_RUNTIME_HOST_H
#define LIEF_RUNTIME_HOST_H

#include "LIEF/visibility.h"
#include <string>

namespace LIEF {
namespace runtime {

/// This class represents the current host.
class LIEF_API Host {
  public:
  /// The machine hostname
  static std::string name();

  /// The user home dir (e.g. `/home/romain` or `C:\Users\romain`)
  static std::string home_dir();

  /// Temporary directory.
  ///
  /// This function looks at the environment variables to determine the suitable
  /// temp directory (e.g. `TEMP`, `TMPDIR`)
  static std::string tmp_dir();

  /// The directory to store user-specific configuration
  static std::string config_dir();

  /// The directory where software should store their cache files
  /// (e.g. `$HOME/.cache`)
  static std::string cache_dir();
};
}
}
#endif
