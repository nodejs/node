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
#include "LIEF/PE/debug/ExDllCharacteristics.hpp"

#include "logging.hpp"
#include "fmt_formatter.hpp"

FMT_FORMATTER(LIEF::PE::ExDllCharacteristics::CHARACTERISTICS, LIEF::PE::to_string);

namespace LIEF::PE {

static constexpr auto CHARACTERISTICS_ARRAY = {
    ExDllCharacteristics::CHARACTERISTICS::CET_COMPAT,
    ExDllCharacteristics::CHARACTERISTICS::CET_COMPAT_STRICT_MODE,
    ExDllCharacteristics::CHARACTERISTICS::CET_SET_CONTEXT_IP_VALIDATION_RELAXED_MODE,
    ExDllCharacteristics::CHARACTERISTICS::CET_DYNAMIC_APIS_ALLOW_IN_PROC,
    ExDllCharacteristics::CHARACTERISTICS::CET_RESERVED_1,
    ExDllCharacteristics::CHARACTERISTICS::CET_RESERVED_2,
    ExDllCharacteristics::CHARACTERISTICS::FORWARD_CFI_COMPAT,
    ExDllCharacteristics::CHARACTERISTICS::HOTPATCH_COMPATIBLE,
};

std::unique_ptr<ExDllCharacteristics> ExDllCharacteristics::parse(
  const details::pe_debug& hdr, Section* section, span<uint8_t> payload)
{
  if (payload.size() < sizeof(uint32_t)) {
    return nullptr;
  }

  auto value = *reinterpret_cast<uint32_t*>(payload.data());
  return std::make_unique<ExDllCharacteristics>(hdr, section, value);
}

std::vector<ExDllCharacteristics::CHARACTERISTICS> ExDllCharacteristics::characteristics_list() const {
  std::vector<CHARACTERISTICS> out;
  out.reserve(3);

  std::copy_if(CHARACTERISTICS_ARRAY.begin(), CHARACTERISTICS_ARRAY.end(),
               std::back_inserter(out),
               [this] (CHARACTERISTICS c) { return has(c); });
  return out;
}

std::string ExDllCharacteristics::to_string() const {
  std::ostringstream os;
  using namespace fmt;
  os << Debug::to_string() << '\n'
     << format("  Characteristics: {}", join(characteristics_list(), ", "));
  return os.str();
}

const char* to_string(ExDllCharacteristics::CHARACTERISTICS e) {
  using CHARACTERISTICS = ExDllCharacteristics::CHARACTERISTICS;
  switch (e) {
    default:
      return "UNKNOWN";
    case CHARACTERISTICS::CET_COMPAT:
      return "CET_COMPAT";
    case CHARACTERISTICS::CET_COMPAT_STRICT_MODE:
      return "CET_COMPAT_STRICT_MODE";
    case CHARACTERISTICS::CET_SET_CONTEXT_IP_VALIDATION_RELAXED_MODE:
      return "CET_SET_CONTEXT_IP_VALIDATION_RELAXED_MODE";
    case CHARACTERISTICS::CET_DYNAMIC_APIS_ALLOW_IN_PROC:
      return "CET_DYNAMIC_APIS_ALLOW_IN_PROC";
    case CHARACTERISTICS::CET_RESERVED_1:
      return "CET_RESERVED_1";
    case CHARACTERISTICS::CET_RESERVED_2:
      return "CET_RESERVED_2";
    case CHARACTERISTICS::FORWARD_CFI_COMPAT:
      return "FORWARD_CFI_COMPAT";
    case CHARACTERISTICS::HOTPATCH_COMPATIBLE:
      return "HOTPATCH_COMPATIBLE";
  }
  return "UNKNOWN";
}

}
