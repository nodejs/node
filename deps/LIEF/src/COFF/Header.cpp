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
#include "LIEF/COFF/Header.hpp"
#include "LIEF/COFF/RegularHeader.hpp"
#include "LIEF/COFF/BigObjHeader.hpp"
#include "LIEF/COFF/utils.hpp"

#include "internal_utils.hpp"

#include <spdlog/fmt/fmt.h>
#include <sstream>

namespace LIEF::COFF {
std::unique_ptr<Header> Header::create(BinaryStream& stream) {
  return create(stream, get_kind(stream));
}

std::unique_ptr<Header> Header::create(BinaryStream& stream, KIND kind) {
  switch (kind) {
    case KIND::REGULAR:
      return RegularHeader::create(stream);

    case KIND::BIGOBJ:
      return BigObjHeader::create(stream);

    case KIND::UNKNOWN:
      return nullptr;
  }
  return nullptr;
}

std::string Header::to_string() const {
  using namespace fmt;
  std::ostringstream oss;

  static constexpr auto WIDTH = 16;
  if (kind() == KIND::BIGOBJ) {
    oss << "BigObj ";
  }
  oss << "COFF Binary\n";
  oss << format("{:>#{}x} Machine ({})\n", (uint16_t)machine(), WIDTH,
                COFF::to_string(machine()));
  oss << format("{:>{}} Number of sections\n", (uint16_t)nb_sections(), WIDTH);
  oss << format("{:>#{}x} Time date stamp: {}\n", timedatestamp(), WIDTH,
                ts_to_str(timedatestamp()));
  oss << format("{:>#{}x} File pointer to symbol table\n", pointerto_symbol_table(), WIDTH);
  oss << format("{:>{}} Number of symbols", nb_symbols(), WIDTH);
  return oss.str();
}
}
