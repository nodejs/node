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
#include <sstream>

#include "LIEF/PE/debug/Pogo.hpp"
#include "LIEF/Visitor.hpp"

#include "frozen.hpp"

#include <spdlog/fmt/fmt.h>

namespace LIEF {
namespace PE {

void Pogo::accept(Visitor& visitor) const {
  visitor.visit(*this);
}

std::string Pogo::to_string() const {
  using namespace fmt;
  std::ostringstream os;
  os << Debug::to_string() << '\n'
     << "Pogo:\n"
     << format("  Signature: {} (0x{:06x})\n",
                    PE::to_string(signature()), (uint32_t)signature());

  for (const PogoEntry& pentry : entries()) {
    os << "    " << pentry << '\n';
  }

  return os.str();
}

const char* to_string(Pogo::SIGNATURES e) {
  #define ENTRY(X) std::pair(Pogo::SIGNATURES::X, #X)
  STRING_MAP enums2str {
    ENTRY(UNKNOWN),
    ENTRY(ZERO),
    ENTRY(LCTG),
    ENTRY(PGI),
  };
  #undef ENTRY
  if (const auto it = enums2str.find(e); it != enums2str.end()) {
    return it->second;
  }
  return "UNKNOWN";
}

} // namespace PE
} // namespace LIEF
