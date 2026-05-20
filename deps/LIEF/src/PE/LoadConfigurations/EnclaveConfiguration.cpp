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
#include "LIEF/PE/Parser.hpp"
#include "LIEF/PE/Binary.hpp"
#include "LIEF/PE/LoadConfigurations/EnclaveConfiguration.hpp"
#include "LIEF/PE/LoadConfigurations/EnclaveImport.hpp"
#include "LIEF/BinaryStream/BinaryStream.hpp"

#include "PE/Structures.hpp"

#include "logging.hpp"
#include "internal_utils.hpp"

namespace LIEF::PE {

std::string EnclaveConfiguration::to_string() const {
  using namespace fmt;
  static constexpr auto WIDTH = 39;
  std::ostringstream os;
  os << format("{:>{}}: 0x{:08x}\n", "Size", WIDTH, size())
     << format("{:>{}}: 0x{:08x}\n", "Minimum Required Config Size", WIDTH,
               min_required_config_size())
     << format("{:>{}}: 0x{:08x} (debuggable={})\n", "Policy Flags", WIDTH,
               policy_flags(), is_debuggable())
     << format("{:>{}}: {}\n", "Number of Enclave Import Descriptors", WIDTH,
               nb_imports())
     << format("{:>{}}: 0x{:08x}\n", "RVA to enclave imports", WIDTH,
               import_list_rva())
     << format("{:>{}}: 0x{:04x}\n", "Size of enclave import", WIDTH,
               import_entry_size())
     << format("{:>{}}: {}\n", "Image version", WIDTH, image_version())
     << format("{:>{}}: {}\n", "Security version", WIDTH, security_version())
     << format("{:>{}}: 0x{:016x}\n", "Enclave Size", WIDTH, enclave_size())
     << format("{:>{}}: {}\n", "Number of Threads", WIDTH, nb_threads())
     << format("{:>{}}: 0x{:08x}\n", "Enclave flags", WIDTH, enclave_flags());

  os << format("{:>{}}: {}\n", "Image ID", WIDTH,
               hex_dump(image_id(), /*sep=*/" "));
  os << format("{:>{}}: {}", "Family ID", WIDTH,
               hex_dump(family_id(), /*sep=*/" "));

  if (!imports_.empty()) {
    os << '\n';
  }

  for (const EnclaveImport& imp : imports()) {
    os << indent(imp.to_string(), 4) << '\n';
  }

  return os.str();
}

template<class PE_T>
std::unique_ptr<EnclaveConfiguration>
  EnclaveConfiguration::parse(Parser& ctx, BinaryStream& stream)
{
  using ptr_t = typename PE_T::uint;
  auto config = std::make_unique<EnclaveConfiguration>();
  auto Size = stream.read<uint32_t>();
  if (!Size) {
    LIEF_DEBUG("{}:{}", __FUNCTION__, __LINE__);
    return nullptr;
  }

  if (*Size < MIN_SIZE) {
    LIEF_DEBUG("{}:{}", __FUNCTION__, __LINE__);
    return nullptr;
  }

  auto MinimumRequiredConfigSize = stream.read<uint32_t>();
  if (!MinimumRequiredConfigSize) {
    LIEF_DEBUG("{}:{}", __FUNCTION__, __LINE__);
    return nullptr;
  }

  auto PolicyFlags = stream.read<uint32_t>();
  if (!PolicyFlags) {
    LIEF_DEBUG("{}:{}", __FUNCTION__, __LINE__);
    return nullptr;
  }

  auto NumberOfImports = stream.read<uint32_t>();
  if (!NumberOfImports) {
    LIEF_DEBUG("{}:{}", __FUNCTION__, __LINE__);
    return nullptr;
  }

  auto ImportList = stream.read<uint32_t>();
  if (!ImportList) {
    LIEF_DEBUG("{}:{}", __FUNCTION__, __LINE__);
    return nullptr;
  }
  auto ImportEntrySize = stream.read<uint32_t>();
  if (!ImportEntrySize) {
    LIEF_DEBUG("{}:{}", __FUNCTION__, __LINE__);
    return nullptr;
  }

  if (!stream.read_array(config->family_id_)) {
    LIEF_DEBUG("{}:{}", __FUNCTION__, __LINE__);
    return nullptr;
  }

  if (!stream.read_array(config->image_id_)) {
    LIEF_DEBUG("{}:{}", __FUNCTION__, __LINE__);
    return nullptr;
  }

  auto ImageVersion = stream.read<uint32_t>();
  if (!ImageVersion) {
    LIEF_DEBUG("{}:{}", __FUNCTION__, __LINE__);
    return nullptr;
  }

  auto SecurityVersion = stream.read<uint32_t>();
  if (!SecurityVersion) {
    LIEF_DEBUG("{}:{}", __FUNCTION__, __LINE__);
    return nullptr;
  }

  auto EnclaveSize = stream.read<ptr_t>();
  if (!EnclaveSize) {
    LIEF_DEBUG("{}:{}", __FUNCTION__, __LINE__);
    return nullptr;
  }

  auto NumberOfThreads = stream.read<uint32_t>();
  if (!NumberOfThreads) {
    LIEF_DEBUG("{}:{}", __FUNCTION__, __LINE__);
    return nullptr;
  }

  auto EnclaveFlags = stream.read<uint32_t>();
  if (!EnclaveFlags) {
    LIEF_DEBUG("{}:{}", __FUNCTION__, __LINE__);
    return nullptr;
  }

  (*config)
    .size(*Size)
    .min_required_config_size(*MinimumRequiredConfigSize)
    .policy_flags(*PolicyFlags)
    .import_list_rva(*ImportList)
    .import_entry_size(*ImportEntrySize)
    .image_version(*ImageVersion)
    .security_version(*SecurityVersion)
    .enclave_size(*EnclaveSize)
    .nb_threads(*NumberOfThreads)
    .enclave_flags(*EnclaveFlags)
  ;

  if (*ImportEntrySize == 0 || *NumberOfImports == 0) {
    return config;
  }

  if (*ImportEntrySize > 0x1000) {
    return config;
  }

  uint32_t base_offset = ctx.bin().rva_to_offset(*ImportList);

  for (size_t i = 0; i < *NumberOfImports; ++i) {
    uint32_t offset = base_offset + i * (*ImportEntrySize);
    ScopedStream scope(ctx.stream(), offset);
    auto import = EnclaveImport::parse(ctx, *scope);
    if (!import) {
      LIEF_DEBUG("Error: {}:{}", __FUNCTION__, __LINE__);
      return config;
    }

    config->imports_.push_back(std::move(*import));
  }

  return config;
}

template std::unique_ptr<EnclaveConfiguration>
  EnclaveConfiguration::parse<details::PE32>(Parser& ctx, BinaryStream& stream);

template std::unique_ptr<EnclaveConfiguration>
  EnclaveConfiguration::parse<details::PE64>(Parser& ctx, BinaryStream& stream);

}
