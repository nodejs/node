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
#include <algorithm>

#include <spdlog/fmt/fmt.h>

#include "LIEF/Visitor.hpp"
#include "PE/Structures.hpp"
#include "fmt_formatter.hpp"

#include "LIEF/PE/resources/ResourceAccelerator.hpp"

FMT_FORMATTER(LIEF::PE::ResourceAccelerator::FLAGS, LIEF::PE::to_string);

namespace LIEF {
namespace PE {

static constexpr auto ARRAY_FLAGS = {
  ResourceAccelerator::FLAGS::VIRTKEY, ResourceAccelerator::FLAGS::NOINVERT,
  ResourceAccelerator::FLAGS::SHIFT, ResourceAccelerator::FLAGS::CONTROL,
  ResourceAccelerator::FLAGS::ALT, ResourceAccelerator::FLAGS::END,
};

ResourceAccelerator::ResourceAccelerator(const details::pe_resource_acceltableentry& entry) :
  flags_{entry.fFlags},
  ansi_{entry.wAnsi},
  id_{static_cast<uint16_t>(entry.wId)},
  padding_{entry.padding}
{}

void ResourceAccelerator::accept(Visitor& visitor) const {
  visitor.visit(*this);
}

std::ostream& operator<<(std::ostream& os, const ResourceAccelerator& acc) {
  const auto& flags = acc.flags_list();
  os << fmt::format("{} (0x{:04x}) id: 0x{:04x}, flags: {}",
                    acc.ansi_str(), acc.ansi(), acc.id(),
                    fmt::to_string(flags));
  return os;
}

std::vector<ResourceAccelerator::FLAGS> ResourceAccelerator::flags_list() const {
  std::vector<FLAGS> flags;
  std::copy_if(std::begin(ARRAY_FLAGS), std::end(ARRAY_FLAGS),
               std::back_inserter(flags),
               [this] (FLAGS f) { return has(f); });
  return flags;
}

const char* to_string(ResourceAccelerator::FLAGS flag) {
  switch (flag) {
    case ResourceAccelerator::FLAGS::VIRTKEY:
      return "VIRTKEY";
    case ResourceAccelerator::FLAGS::NOINVERT:
      return "NOINVERT";
    case ResourceAccelerator::FLAGS::SHIFT:
      return "SHIFT";
    case ResourceAccelerator::FLAGS::CONTROL:
      return "CONTROL";
    case ResourceAccelerator::FLAGS::ALT:
      return "ALT";
    case ResourceAccelerator::FLAGS::END:
      return "END";
    default:
      return "UNKNOWN";
  }

  return "UNKNOWN";
}

}
}
