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
#include "LIEF/Visitor.hpp"
#include "LIEF/MachO/BuildToolVersion.hpp"

#include "MachO/Structures.hpp"
#include "frozen.hpp"
#include "spdlog/fmt/fmt.h"

namespace LIEF {
namespace MachO {

BuildToolVersion::BuildToolVersion(const details::build_tool_version& tool) :
  tool_{BuildToolVersion::TOOLS(tool.tool)},
  version_{{
    static_cast<uint32_t>((tool.version >> 16) & 0xFFFF),
    static_cast<uint32_t>((tool.version >>  8) & 0xFF),
    static_cast<uint32_t>((tool.version >>  0) & 0xFF)
  }}
{}

void BuildToolVersion::accept(Visitor& visitor) const {
  visitor.visit(*this);
}

std::ostream& operator<<(std::ostream& os, const BuildToolVersion& tool) {
  BuildToolVersion::version_t version = tool.version();

  std::string tool_str = to_string(tool.tool());
  if (tool.tool() == BuildToolVersion::TOOLS::UNKNOWN) {
    tool_str += fmt::format("({})", (uint32_t)tool.tool());
  }

  os << fmt::format("{} ({}.{}.{})",
        tool_str, version[0], version[1], version[2]);
  return os;
}

const char* to_string(BuildToolVersion::TOOLS tool) {
  #define ENTRY(X) std::pair(BuildToolVersion::TOOLS::X, #X)
  STRING_MAP enums2str {
    ENTRY(UNKNOWN),
    ENTRY(CLANG),
    ENTRY(SWIFT),
    ENTRY(LD),
    ENTRY(LLD),
    ENTRY(METAL),
    ENTRY(AIRLLD),
    ENTRY(AIRNT),
    ENTRY(AIRNT_PLUGIN),
    ENTRY(AIRPACK),
    ENTRY(GPUARCHIVER),
    ENTRY(METAL_FRAMEWORK),
  };
  #undef ENTRY

  if (auto it = enums2str.find(tool); it != enums2str.end()) {
    return it->second;
  }

  return "UNKNOWN";
}

}
}
