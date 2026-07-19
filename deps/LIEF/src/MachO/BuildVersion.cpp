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
#include <spdlog/fmt/fmt.h>
#include <spdlog/fmt/ranges.h>
#include "LIEF/Visitor.hpp"

#include "frozen.hpp"

#include "LIEF/MachO/BuildVersion.hpp"
#include "MachO/Structures.hpp"

namespace LIEF {
namespace MachO {

BuildVersion::BuildVersion(const details::build_version_command& ver) :
  LoadCommand::LoadCommand{LoadCommand::TYPE(ver.cmd), ver.cmdsize},
  platform_{PLATFORMS(ver.platform)},
  minos_{{
    static_cast<uint32_t>((ver.minos >> 16) & 0xFFFF),
    static_cast<uint32_t>((ver.minos >>  8) & 0xFF),
    static_cast<uint32_t>((ver.minos >>  0) & 0xFF)
  }},
  sdk_{{
    static_cast<uint32_t>((ver.sdk >> 16) & 0xFFFF),
    static_cast<uint32_t>((ver.sdk >>  8) & 0xFF),
    static_cast<uint32_t>((ver.sdk >>  0) & 0xFF)
  }}
{
}

BuildVersion::BuildVersion(const PLATFORMS platform,
                           const version_t &minos,
                           const version_t &sdk,
                           const tools_list_t &tools) :
  LoadCommand::LoadCommand{LoadCommand::TYPE::BUILD_VERSION,
                           static_cast<uint32_t>(sizeof(details::build_version_command) +
                           sizeof(details::build_tool_version) * tools.size())},
  platform_{platform}, minos_{minos}, sdk_{sdk}, tools_{tools}
{
  original_data_.resize(size());
}

void BuildVersion::accept(Visitor& visitor) const {
  visitor.visit(*this);
}

std::ostream& BuildVersion::print(std::ostream& os) const {
  LoadCommand::print(os) << '\n';
  os << fmt::format("Platform: {}", to_string(platform())) << '\n';
  os << fmt::format("Min OS:   {}", fmt::join(minos(), ".")) << '\n';
  os << fmt::format("SDK:      {}", fmt::join(sdk(), ".")) << '\n';
  for (const BuildToolVersion& version : tools()) {
    os << "  " << version << '\n';
  }
  return os;
}

const char* to_string(BuildVersion::PLATFORMS e) {
  #define ENTRY(X) std::pair(BuildVersion::PLATFORMS::X, #X)
  STRING_MAP enums2str {
    ENTRY(UNKNOWN),
    ENTRY(MACOS),
    ENTRY(IOS),
    ENTRY(TVOS),
    ENTRY(WATCHOS),
    ENTRY(BRIDGEOS),
    ENTRY(MAC_CATALYST),
    ENTRY(IOS_SIMULATOR),
    ENTRY(TVOS_SIMULATOR),
    ENTRY(WATCHOS_SIMULATOR),
    ENTRY(DRIVERKIT),
    ENTRY(VISIONOS),
    ENTRY(VISIONOS_SIMULATOR),
    ENTRY(FIRMWARE),
    ENTRY(SEPOS),
    ENTRY(MACOS_EXCLAVE_CORE),
    ENTRY(MACOS_EXCLAVE_KIT),
    ENTRY(IOS_EXCLAVE_CORE),
    ENTRY(IOS_EXCLAVE_KIT),
    ENTRY(TVOS_EXCLAVE_CORE),
    ENTRY(TVOS_EXCLAVE_KIT),
    ENTRY(WATCHOS_EXCLAVE_CORE),
    ENTRY(WATCHOS_EXCLAVE_KIT),
    ENTRY(VISIONOS_EXCLAVE_CORE),
    ENTRY(VISIONOS_EXCLAVE_KIT),

    ENTRY(ANY),
  };
  #undef ENTRY

  if (auto it = enums2str.find(e); it != enums2str.end()) {
    return it->second;
  }
  return "UNKNOWN";
}

}
}
