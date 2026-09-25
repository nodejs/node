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
#include "LIEF/PE/ParserConfig.hpp"
#include <spdlog/fmt/fmt.h>
#include <sstream>

namespace LIEF::PE {
std::string ParserConfig::to_string() const {
  static constexpr auto WIDTH = 20;
  using namespace fmt;
  std::ostringstream os;
  os << "ParserConfig {\n"
     << format("  {:{}}: {}\n", "parse_signature", WIDTH, parse_signature)
     << format("  {:{}}: {}\n", "parse_exports", WIDTH, parse_exports)
     << format("  {:{}}: {}\n", "parse_imports", WIDTH, parse_imports)
     << format("  {:{}}: {}\n", "parse_rsrc", WIDTH, parse_rsrc)
     << format("  {:{}}: {}\n", "parse_reloc", WIDTH, parse_reloc)
     << format("  {:{}}: {}\n", "parse_exceptions", WIDTH, parse_exceptions)
     << format("  {:{}}: {}\n", "parse_arm64x_binary", WIDTH, parse_arm64x_binary)
     << "}\n";
  return os.str();
}
}
