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
#ifndef LIEF_RUNTIME_OSX_HOST_H
#define LIEF_RUNTIME_OSX_HOST_H
#include <LIEF/visibility.h>
#include <cstdint>
#include <string>
#include <ostream>

namespace LIEF {
namespace runtime {
namespace osx {

class LIEF_API Host {
  public:
  struct LIEF_API version_t {
    uint32_t major = 0;
    uint32_t minor = 0;
    uint32_t patch = 0;

    version_t(uint32_t major, uint32_t minor, uint32_t patch) :
      major(major),
      minor(minor),
      patch(patch) {}

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

    static const version_t& BigSur();
    static const version_t& Monterey();
    static const version_t& Ventura();
    static const version_t& Sonoma();
    static const version_t& Sequoia();
    static const version_t& Tahoe();

    LIEF_API friend std::ostream& operator<<(std::ostream& os,
                                             const version_t version) {
      os << version.to_string();
      return os;
    }
  };

  /// The OS version string (e.g. `Version 26.2 (Build 25C56)`)
  static std::string os_version_name();

  /// The OS version (e.g. `13.0.0`)
  static version_t os_version();

  /// Whether System Integrity Protection (SIP) is enabled on this host.
  ///
  /// This conservatively returns ``true`` when the status can't be determined
  /// (including on a non-macOS build).
  static bool is_sip_enabled();
};


}
}
}
#endif
