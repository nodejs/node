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
#ifndef LIEF_MACHO_BUILD_TOOL_VERSION_COMMAND_H
#define LIEF_MACHO_BUILD_TOOL_VERSION_COMMAND_H
#include <ostream>
#include <array>
#include <cstdint>

#include "LIEF/Object.hpp"
#include "LIEF/visibility.h"

namespace LIEF {
namespace MachO {

namespace details {
struct build_tool_version;
}

/// Class that represents a tool's version that was
/// involved in the build of the binary
class LIEF_API BuildToolVersion : public Object {
  public:
  /// A version is an array of **3** integers
  using version_t = std::array<uint32_t, 3>;

  public:
  enum class TOOLS {
    UNKNOWN = 0,
    CLANG   = 1,
    SWIFT   = 2,
    LD      = 3,
    LLD     = 4,

    METAL           = 1024,
    AIRLLD          = 1025,
    AIRNT           = 1026,
    AIRNT_PLUGIN    = 1027,
    AIRPACK         = 1028,
    GPUARCHIVER     = 1031,
    METAL_FRAMEWORK = 1032,
  };

  public:
  BuildToolVersion() = default;
  BuildToolVersion(const details::build_tool_version& tool);

  /// The tools used
  TOOLS tool() const {
    return tool_;
  }

  /// Version associated with the tool
  version_t version() const {
    return version_;
  }

  ~BuildToolVersion() override = default;

  void accept(Visitor& visitor) const override;

  LIEF_API friend
  std::ostream& operator<<(std::ostream& os, const BuildToolVersion& tool);

  private:
  TOOLS tool_ = TOOLS::UNKNOWN;
  version_t version_;
};

LIEF_API const char* to_string(BuildToolVersion::TOOLS tool);

}
}
#endif
