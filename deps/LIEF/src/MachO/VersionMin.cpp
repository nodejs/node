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
#include "spdlog/fmt/fmt.h"
#include "spdlog/fmt/ranges.h"
#include "LIEF/Visitor.hpp"

#include "LIEF/MachO/VersionMin.hpp"
#include "MachO/Structures.hpp"

namespace LIEF {
namespace MachO {

VersionMin::VersionMin(const details::version_min_command& version_cmd) :
  LoadCommand::LoadCommand{LoadCommand::TYPE(version_cmd.cmd), version_cmd.cmdsize},
  version_{{
    static_cast<uint32_t>((version_cmd.version >> 16) & 0xFFFF),
    static_cast<uint32_t>((version_cmd.version >>  8) & 0xFF),
    static_cast<uint32_t>((version_cmd.version >>  0) & 0xFF)
  }},
  sdk_{{
    static_cast<uint32_t>((version_cmd.sdk >> 16) & 0xFFFF),
    static_cast<uint32_t>((version_cmd.sdk >>  8) & 0xFF),
    static_cast<uint32_t>((version_cmd.sdk >>  0) & 0xFF)
  }}
{
}

void VersionMin::accept(Visitor& visitor) const {
  visitor.visit(*this);
}

std::ostream& VersionMin::print(std::ostream& os) const {
  LoadCommand::print(os) << '\n';
  os << fmt::format("Version: {}", fmt::join(version(), ".")) << '\n';
  os << fmt::format("SDK:     {}", fmt::join(sdk(), "."));
  return os;
}


}
}
