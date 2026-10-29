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
#ifndef LIEF_RUNTIME_WINDOWS_HOST_H
#define LIEF_RUNTIME_WINDOWS_HOST_H
#include <LIEF/visibility.h>
#include <cstdint>
#include <string>
#include <ostream>

namespace LIEF {
namespace runtime {
namespace windows {

/// This class exposes Windows-specific host information.
class LIEF_API Host {
  public:
  struct LIEF_API version_t {
    uint32_t major = 0;
    uint32_t minor = 0;
    uint32_t build_number = 0;

    version_t() = default;

    version_t(uint32_t major, uint32_t minor, uint32_t build_number) :
      major(major),
      minor(minor),
      build_number(build_number) {}

    bool operator<=(const version_t& rhs) const;
    bool operator>(const version_t& rhs) const {
      return !(*this <= rhs);
    }

    bool operator>=(const version_t& rhs) const;
    bool operator<(const version_t& rhs) const {
      return !(*this >= rhs);
    }

    bool operator==(const version_t& other) const;
    bool operator!=(const version_t& other) const {
      return !(*this == other);
    }


    std::string to_string() const;
    LIEF_API friend std::ostream& operator<<(std::ostream& os,
                                             const version_t& version) {
      os << version.to_string();
      return os;
    }
  };

  /// Return the Windows version (e.g., `10.0.26200`)
  static version_t version();
};


}
}
}
#endif
