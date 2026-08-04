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

#include "LIEF/MachO/SourceVersion.hpp"
#include "MachO/Structures.hpp"

namespace LIEF {
namespace MachO {


SourceVersion::SourceVersion(const details::source_version_command& ver) :
  LoadCommand::LoadCommand{LoadCommand::TYPE(ver.cmd), ver.cmdsize},
  version_{{
    static_cast<uint32_t>((ver.version >> 40) & 0xffffff),
    static_cast<uint32_t>((ver.version >> 30) & 0x3ff),
    static_cast<uint32_t>((ver.version >> 20) & 0x3ff),
    static_cast<uint32_t>((ver.version >> 10) & 0x3ff),
    static_cast<uint32_t>((ver.version >>  0) & 0x3ff)
  }}
{}

void SourceVersion::accept(Visitor& visitor) const {
  visitor.visit(*this);
}

std::ostream& SourceVersion::print(std::ostream& os) const {
  LoadCommand::print(os) << '\n';
  os << fmt::format("Version: {}", fmt::join(version(), "."));
  return os;
}


}
}
