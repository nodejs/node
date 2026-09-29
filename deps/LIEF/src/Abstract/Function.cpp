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
#include <algorithm>
#include <ostream>
#include <spdlog/fmt/fmt.h>
#include "LIEF/Visitor.hpp"

#include "LIEF/Abstract/Function.hpp"

#include "internal_utils.hpp"
#include "frozen.hpp"

namespace LIEF {

static constexpr auto ARRAY_FLAGS = {
    Function::FLAGS::NONE,       Function::FLAGS::CONSTRUCTOR,
    Function::FLAGS::DESTRUCTOR, Function::FLAGS::DEBUG_INFO,
    Function::FLAGS::EXPORTED,   Function::FLAGS::IMPORTED,
};

std::vector<Function::FLAGS> Function::flags_list() const {
  std::vector<FLAGS> flags;
  std::copy_if(ARRAY_FLAGS.begin(), ARRAY_FLAGS.end(), std::back_inserter(flags),
               [this](FLAGS f) { return has(f); });
  return flags;
}

void Function::accept(Visitor& visitor) const {
  visitor.visit(*this);
}

std::ostream& operator<<(std::ostream& os, const Function& entry) {
  os << fmt::format("{:#012x}: {} ({:#06x} bytes)", entry.address(),
                    printable_string(entry.name()), entry.size());
  return os;
}

const char* to_string(Function::FLAGS e) {
#define ENTRY(X) std::pair(Function::FLAGS::X, #X)
  STRING_MAP enums2str{
      ENTRY(NONE),       ENTRY(CONSTRUCTOR), ENTRY(DESTRUCTOR),
      ENTRY(DEBUG_INFO), ENTRY(EXPORTED),    ENTRY(IMPORTED),
  };
#undef ENTRY

  if (auto it = enums2str.find(e); it != enums2str.end()) {
    return it->second;
  }
  return "UNKNOWN";
}

}
