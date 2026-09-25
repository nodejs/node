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
#include "LIEF/PE/Binary.hpp"
#include "LIEF/PE/Parser.hpp"
#include "LIEF/PE/LoadConfigurations/EnclaveConfiguration.hpp"
#include "LIEF/PE/LoadConfigurations/EnclaveImport.hpp"
#include "LIEF/BinaryStream/BinaryStream.hpp"

#include "logging.hpp"
#include "internal_utils.hpp"

namespace LIEF::PE {

std::string EnclaveImport::to_string() const {
  using namespace fmt;
  static constexpr auto WIDTH = 26;

  std::ostringstream os;

  os << format("{} (RVA: 0x{:08x})\n", import_name(), import_name_rva())
     << format("  {:{}}: {}\n", "Minimum Security Version", WIDTH, min_security_version())
     << format("  {:{}}: {}\n", "Reserved", WIDTH, reserved())
     << format("  {:{}}: {}\n", "Type", WIDTH, PE::to_string(type()))
     << format("  {:{}}: {}\n", "Family ID", WIDTH, hex_dump(family_id(), " "))
     << format("  {:{}}: {}\n", "Image ID", WIDTH, hex_dump(image_id(), " "));
  {
    span<const uint8_t> id = this->id();
    auto chunk_1 = id.subspan(0, 16);
    auto chunk_2 = id.subspan(16);
    os << format("  {:{}}: {}\n{:{}}{}", "unique/author ID", WIDTH,
                 hex_dump(chunk_1, " "), " ", WIDTH + 4, hex_dump(chunk_2, " "));
  }


  return os.str();
}

result<EnclaveImport> EnclaveImport::parse(Parser& ctx, BinaryStream& stream)
{
  EnclaveImport imp;
  auto MatchType = stream.read<uint32_t>();
  if (!MatchType) {
    return make_error_code(MatchType.error());
  }

  auto MinimumSecurityVersion = stream.read<uint32_t>();
  if (!MinimumSecurityVersion) {
    return make_error_code(MinimumSecurityVersion.error());
  }

  if (!stream.read_array(imp.id_)) {
    return make_error_code(lief_errors::read_error);
  }

  if (!stream.read_array(imp.family_id_)) {
    return make_error_code(lief_errors::read_error);
  }

  if (!stream.read_array(imp.image_id_)) {
    return make_error_code(lief_errors::read_error);
  }

  auto ImportName = stream.read<uint32_t>();
  if (!ImportName) {
    return make_error_code(ImportName.error());
  }

  auto Reserved = stream.read<uint32_t>();
  if (!Reserved) {
    return make_error_code(Reserved.error());
  }

  imp
    .type((TYPE)*MatchType)
    .min_security_version(*MinimumSecurityVersion)
    .import_name_rva(*ImportName)
    .reserved(*Reserved)
  ;

  {
    uint32_t name_offset = ctx.bin().rva_to_offset(*ImportName);
    ScopedStream scope(ctx.stream(), name_offset);
    imp.import_name(scope->read_string().value_or(""));
  }

  return imp;
}

const char* to_string(EnclaveImport::TYPE e) {
  switch (e) {
    case EnclaveImport::TYPE::NONE: return "NONE";
    case EnclaveImport::TYPE::UNIQUE_ID: return "UNIQUE_ID";
    case EnclaveImport::TYPE::AUTHOR_ID: return "AUTHOR_ID";
    case EnclaveImport::TYPE::FAMILY_ID: return "FAMILY_ID";
    case EnclaveImport::TYPE::IMAGE_ID: return "IMAGE_ID";
    default: return "UNKNOWN";
  }
  return "UNKNOWN";
}


}
