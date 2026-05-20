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
#include "LIEF/PE/LoadConfigurations/CHPEMetadata/Metadata.hpp"
#include "LIEF/PE/LoadConfigurations/CHPEMetadata/MetadataARM64.hpp"
#include "LIEF/PE/LoadConfigurations/CHPEMetadata/MetadataX86.hpp"
#include "LIEF/PE/Header.hpp"
#include "LIEF/PE/Parser.hpp"
#include "LIEF/PE/Binary.hpp"

#include "LIEF/BinaryStream/BinaryStream.hpp"

#include "logging.hpp"

namespace LIEF::PE {
std::unique_ptr<CHPEMetadata> CHPEMetadata::parse(Parser& ctx, BinaryStream& stream) {
  LIEF_DEBUG("Parsing CHPEMetadata");
  auto version = stream.read<uint32_t>();
  if (!version) {
    LIEF_DEBUG("Failed to parse CHPEMetadata version (line={})", __LINE__);
    return nullptr;
  }
  const Header::MACHINE_TYPES arch = ctx.bin().header().machine();

  switch (arch) {
    case Header::MACHINE_TYPES::AMD64:
    case Header::MACHINE_TYPES::ARM64:
      return CHPEMetadataARM64::parse(ctx, stream, *version);

    case Header::MACHINE_TYPES::I386:
      return CHPEMetadataX86::parse(ctx, stream, *version);

    default:
      LIEF_WARN("Compiled Hybrid PE (CHPE) is not supported for arch: {}",
                LIEF::PE::to_string(arch));
      return nullptr;
  }

  return nullptr;
}
}
