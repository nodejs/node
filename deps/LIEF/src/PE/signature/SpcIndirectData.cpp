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
#include "LIEF/PE/signature/SpcIndirectData.hpp"
#include "LIEF/PE/EnumToString.hpp"

#include "internal_utils.hpp"

#include <spdlog/fmt/fmt.h>

namespace LIEF {
namespace PE {

void SpcIndirectData::accept(Visitor& visitor) const {
  visitor.visit(*this);
}

void SpcIndirectData::print(std::ostream& os) const {
  if (!file().empty()) {
    os << fmt::format("{} - {} - {}\n", to_string(digest_algorithm()),
                      file(), hex_dump(digest()));
  } else {
    os << fmt::format("{}: {}\n", to_string(digest_algorithm()), hex_dump(digest()));
  }
}

}
}
