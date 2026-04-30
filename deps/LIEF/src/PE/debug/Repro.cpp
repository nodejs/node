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
#include "LIEF/PE/debug/Repro.hpp"
#include "LIEF/Visitor.hpp"

#include "internal_utils.hpp"

#include <spdlog/fmt/fmt.h>
#include <spdlog/fmt/ranges.h>

namespace LIEF {
namespace PE {

void Repro::accept(Visitor& visitor) const {
  visitor.visit(*this);
}

std::string Repro::to_string() const {
  std::ostringstream os;
  if (hash().empty()) {
    os << "  Repro (empty hash)";
  }

  os << Debug::to_string() << '\n'
     << fmt::format("  Repro: {}", to_hex(hash(), 19));
  return os.str();
}

} // namespace PE
} // namespace LIEF
