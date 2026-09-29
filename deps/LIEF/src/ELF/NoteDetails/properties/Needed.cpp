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
#include "LIEF/ELF/NoteDetails/properties/Needed.hpp"
#include "LIEF/BinaryStream/BinaryStream.hpp"

#include "fmt_formatter.hpp"

FMT_FORMATTER(LIEF::ELF::Needed::NEED, LIEF::ELF::to_string);

namespace LIEF::ELF {

static constexpr auto GNU_PROPERTY_1_NEEDED_INDIRECT_EXTERN_ACCESS = 1U << 0;

std::unique_ptr<Needed> Needed::create(BinaryStream& stream) {
  uint32_t bitmask = stream.read<uint32_t>().value_or(0);

  std::vector<NEED> needs;
  while (bitmask) {
    uint32_t bit = bitmask & (-bitmask);
    bitmask &= ~bit;

    // NOLINTNEXTLINE(bugprone-unhandled-code-paths)
    switch (bit) {
      case GNU_PROPERTY_1_NEEDED_INDIRECT_EXTERN_ACCESS:
        needs.push_back(NEED::INDIRECT_EXTERN_ACCESS);
        break;
      default: needs.push_back(NEED::UNKNOWN); break;
    }
  }

  return std::unique_ptr<Needed>(new Needed(std::move(needs)));
}

const char* to_string(Needed::NEED type) {
  switch (type) {
    case Needed::NEED::INDIRECT_EXTERN_ACCESS: return "INDIRECT_EXTERN_ACCESS";
    case Needed::NEED::UNKNOWN: return "UNKNOWN";
    default: break;
  }

  return "UNKNOWN";
}

void Needed::dump(std::ostream& os) const {
  os << "Needed: " << fmt::to_string(needs());
}

}
