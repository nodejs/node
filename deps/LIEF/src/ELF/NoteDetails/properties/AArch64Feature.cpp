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
#include "LIEF/ELF/NoteDetails/properties/AArch64Feature.hpp"
#include "LIEF/BinaryStream/BinaryStream.hpp"

#include "frozen.hpp"
#include "fmt_formatter.hpp"

FMT_FORMATTER(LIEF::ELF::AArch64Feature::FEATURE, LIEF::ELF::to_string);

namespace LIEF {
namespace ELF {

std::unique_ptr<AArch64Feature> AArch64Feature::create(BinaryStream& stream) {
  static constexpr auto GNU_PROPERTY_AARCH64_FEATURE_1_BTI = 1U << 0;
  static constexpr auto GNU_PROPERTY_AARCH64_FEATURE_1_PAC = 1U << 1;

  uint32_t bitmask = stream.read<uint32_t>().value_or(0);

  std::vector<FEATURE> features;
  while (bitmask) {
    uint32_t bit = bitmask & (- bitmask);
    bitmask &= ~bit;

    switch (bit) {
      case GNU_PROPERTY_AARCH64_FEATURE_1_BTI: features.push_back(FEATURE::BTI); break;
      case GNU_PROPERTY_AARCH64_FEATURE_1_PAC: features.push_back(FEATURE::PAC); break;
      default: features.push_back(FEATURE::UNKNOWN); break;
    }
  }

  return std::unique_ptr<AArch64Feature>(new AArch64Feature(std::move(features)));
}

const char* to_string(AArch64Feature::FEATURE type) {
  #define ENTRY(X) std::pair(AArch64Feature::FEATURE::X, #X)
  STRING_MAP enums2str {
    ENTRY(UNKNOWN),
    ENTRY(BTI),
    ENTRY(PAC),
  };
  #undef ENTRY

  if (auto it = enums2str.find(type); it != enums2str.end()) {
    return it->second;
  }

  return "UNKNOWN";
}

void AArch64Feature::dump(std::ostream &os) const {
  os << "AArch64 feature(s): " << fmt::to_string(features());
}
}
}
