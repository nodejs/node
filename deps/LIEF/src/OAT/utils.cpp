
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
#include <string>
#include "OAT/Structures.hpp"
#include "LIEF/OAT/utils.hpp"
#include "LIEF/ELF/Binary.hpp"
#include "LIEF/ELF/Parser.hpp"
#include "LIEF/ELF/Symbol.hpp"
#include "LIEF/ELF/utils.hpp"
#include "frozen.hpp"

namespace LIEF {
namespace OAT {

bool is_oat(const std::string& file) {
  if (!ELF::is_elf(file)) {
    return false;
  }

  if (const auto elf_binary = ELF::Parser::parse(file)) {
    return is_oat(*elf_binary);
  }
  return false;
}


bool is_oat(const std::vector<uint8_t>& raw) {
  if (const auto elf_binary = ELF::Parser::parse(raw)) {
    return is_oat(*elf_binary);
  }
  return false;
}

bool is_oat(const ELF::Binary& elf) {
  if (const auto* oatdata = elf.get_dynamic_symbol("oatdata")) {
    span<const uint8_t> header =
      elf.get_content_from_virtual_address(oatdata->value(), sizeof(details::oat_magic));
    return std::equal(std::begin(header), std::end(header),
                      std::begin(details::oat_magic));

  }
  return false;
}

oat_version_t version(const std::string& file) {
  if (!is_oat(file)) {
    return 0;
  }

  if (const auto elf = ELF::Parser::parse(file)) {
    return version(*elf);
  }
  return 0;
}

oat_version_t version(const std::vector<uint8_t>& raw) {
  if (!is_oat(raw)) {
    return 0;
  }

  if (const auto elf = ELF::Parser::parse(raw)) {
    return version(*elf);
  }
  return 0;
}


oat_version_t version(const ELF::Binary& elf) {
  if (const auto* oatdata = elf.get_dynamic_symbol("oatdata")) {
    span<const uint8_t> header =
      elf.get_content_from_virtual_address(oatdata->value() + sizeof(details::oat_magic),
                                           sizeof(details::oat_version));

    if (header.size() != sizeof(details::oat_version)) {
      return 0;
    }
    return std::stoul(std::string(reinterpret_cast<const char*>(header.data()), 3));
  }
  return 0;
}

Android::ANDROID_VERSIONS android_version(oat_version_t version) {
  CONST_MAP(oat_version_t, Android::ANDROID_VERSIONS, 6) oat2android {
    { 64,  Android::ANDROID_VERSIONS::VERSION_601 },
    { 79,  Android::ANDROID_VERSIONS::VERSION_700 },
    { 88,  Android::ANDROID_VERSIONS::VERSION_712 },
    { 124, Android::ANDROID_VERSIONS::VERSION_800 },
    { 131, Android::ANDROID_VERSIONS::VERSION_810 },
    { 138, Android::ANDROID_VERSIONS::VERSION_900 },

  };
  auto   it  = oat2android.lower_bound(version);
  return it == oat2android.end() ?
               Android::ANDROID_VERSIONS::VERSION_UNKNOWN : it->second;
}




} // namespace OAT
} // namespace LIEF
