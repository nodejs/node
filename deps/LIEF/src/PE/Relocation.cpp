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
#include "LIEF/Visitor.hpp"
#include "LIEF/BinaryStream/BinaryStream.hpp"


#include "PE/Structures.hpp"
#include "LIEF/PE/Binary.hpp"
#include "LIEF/PE/Parser.hpp"
#include "LIEF/PE/Relocation.hpp"
#include "LIEF/PE/RelocationEntry.hpp"

#include <spdlog/fmt/fmt.h>

#include "logging.hpp"

namespace LIEF {
namespace PE {

Relocation::Relocation(Relocation&&) = default;
Relocation& Relocation::operator=(Relocation&& other) = default;
Relocation::~Relocation() = default;

Relocation::Relocation() = default;

Relocation::Relocation(uint32_t base, uint32_t block_size) :
  Object(),
  block_size_(block_size),
  virtual_address_(base)
{}

Relocation::Relocation(uint32_t base) :
  Relocation(base, 0)
{}

Relocation::Relocation(const Relocation& other) :
  Object{other},
  block_size_{other.block_size_},
  virtual_address_{other.virtual_address_}
{
  entries_.reserve(other.entries_.size());
  for (const std::unique_ptr<RelocationEntry>& r : other.entries_) {
    auto copy = std::make_unique<RelocationEntry>(*r);
    copy->relocation_ = this;
    entries_.push_back(std::move(copy));
  }
}

Relocation& Relocation::operator=(Relocation other) {
  swap(other);
  return *this;
}

Relocation::Relocation(const details::pe_base_relocation_block& header) :
  block_size_{header.BlockSize},
  virtual_address_{header.PageRVA}
{}


void Relocation::swap(Relocation& other) {
  std::swap(block_size_,      other.block_size_);
  std::swap(virtual_address_, other.virtual_address_);
  std::swap(entries_,         other.entries_);
}

RelocationEntry& Relocation::add_entry(const RelocationEntry& entry) {
  auto newone = std::make_unique<RelocationEntry>(entry);
  newone->relocation_ = this;
  entries_.push_back(std::move(newone));
  return *entries_.back();
}

void Relocation::accept(LIEF::Visitor& visitor) const {
  visitor.visit(*this);
}

std::ostream& operator<<(std::ostream& os, const Relocation& relocation) {
  using namespace fmt;
  os << format("Page RVA: 0x{:08x} (SizeOfBlock: {} bytes)\n",
               relocation.virtual_address(), relocation.block_size());
  for (const RelocationEntry& entry : relocation.entries()) {
    os << "  " << entry << '\n';
  }

  return os;
}

Relocation::relocations_t Relocation::parse(Parser& ctx, BinaryStream& stream) {
  relocations_t relocs;
  const Header::MACHINE_TYPES arch = ctx.bin().header().machine();
  while (stream) {
    auto PageRVA = stream.read<uint32_t>();
    if (!PageRVA) {
      LIEF_DEBUG("Error: {}:{}", __FUNCTION__, __LINE__);
      return relocs;
    }

    auto BlockSize = stream.read<uint32_t>();
    if (!BlockSize) {
      LIEF_DEBUG("Error: {}:{}", __FUNCTION__, __LINE__);
      return relocs;
    }

    auto relocation = std::make_unique<Relocation>(*PageRVA, *BlockSize);
    // Must be at least sizeof(PageRVA + BlockSize)
    if (*BlockSize < 2 * 4) {
      relocs.push_back(std::move(relocation));
      return relocs;
    }

    const size_t nb_entries = (*BlockSize - 8) / sizeof(uint16_t);
    {
      ScopedStream block_strm(stream);
      for (size_t i = 0; i < nb_entries; ++i) {
        auto data = block_strm->read<uint16_t>();
        if (!data) {
          LIEF_DEBUG("Error: {}:{} (entry: {})", __FUNCTION__, __LINE__, i);
          break;
        }

        uint16_t pos = RelocationEntry::get_position(*data);
        RelocationEntry::BASE_TYPES ty = RelocationEntry::type_from_data(arch, *data);
        auto entry = std::make_unique<RelocationEntry>(pos, ty);
        entry->relocation_ = relocation.get();
        relocation->entries_.push_back(std::move(entry));
      }
    }
    relocs.push_back(std::move(relocation));
    stream.increment_pos(*BlockSize - 8);
  }
  return relocs;
}

}
}
