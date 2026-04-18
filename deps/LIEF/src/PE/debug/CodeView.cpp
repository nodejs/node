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

#include <LIEF/PE/debug/CodeView.hpp>
#include "LIEF/Visitor.hpp"
#include "spdlog/fmt/fmt.h"
#include "frozen.hpp"

namespace LIEF {
namespace PE {

void CodeView::accept(Visitor& visitor) const {
  visitor.visit(*this);
}

std::string CodeView::to_string() const {
  std::ostringstream os;
  os << Debug::to_string() << '\n'
     << "CodeView:\n"
     << fmt::format("  Signature: {}", PE::to_string(signature()));
  return os.str();
}

const char* to_string(CodeView::SIGNATURES e) {
  #define ENTRY(X) std::pair(CodeView::SIGNATURES::X, #X)
  STRING_MAP enums2str {
    ENTRY(UNKNOWN),
    ENTRY(PDB_70),
    ENTRY(PDB_20),
    ENTRY(CV_50),
    ENTRY(CV_41),
  };
  #undef ENTRY
  if (const auto it = enums2str.find(e); it != enums2str.end()) {
    return it->second;
  }
  return "UNKNOWN";
}

}
}
