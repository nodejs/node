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
#include <memory>

#include "logging.hpp"

#include "LIEF/BinaryStream/VectorStream.hpp"
#include "LIEF/BinaryStream/MemoryStream.hpp"
#include "LIEF/BinaryStream/DumpStream.hpp"

#include "LIEF/MachO/FatBinary.hpp"
#include "LIEF/MachO/Binary.hpp"
#include "LIEF/MachO/Parser.hpp"
#include "LIEF/MachO/DyldInfo.hpp"
#include "LIEF/MachO/DyldBindingInfo.hpp"
#include "LIEF/MachO/BinaryParser.hpp"
#include "LIEF/MachO/utils.hpp"
#include "LIEF/MachO/Relocation.hpp"
#include "LIEF/MachO/RelocationFixup.hpp"
#include "LIEF/MachO/RelocationDyld.hpp"
#include "LIEF/MachO/Section.hpp"
#include "LIEF/MachO/SegmentCommand.hpp"
#include "LIEF/MachO/ThreadLocalVariables.hpp"
#include "MachO/Structures.hpp"


namespace LIEF::MachO {
Parser::Parser() = default;
Parser::~Parser() = default;


// From File
Parser::Parser(const std::string& file, const ParserConfig& conf) :
  LIEF::Parser{file},
  config_{conf} {
  auto stream = VectorStream::from_file(file);
  if (!stream) {
    LIEF_ERR("Failed to create stream");
  } else {
    stream_ = std::make_unique<VectorStream>(std::move(*stream));
  }
}


std::unique_ptr<FatBinary> Parser::parse(const std::string& filename,
                                         const ParserConfig& conf) {
  if (!is_macho(filename)) {
    LIEF_ERR("{} is not a MachO file", filename);
    return nullptr;
  }

  Parser parser{filename, conf};
  parser.parse();
  return std::unique_ptr<FatBinary>(new FatBinary{std::move(parser.binaries_)});
}

// From Vector
Parser::Parser(std::vector<uint8_t> data, const ParserConfig& conf) :
  stream_{std::make_unique<VectorStream>(std::move(data))},
  config_{conf} {}


std::unique_ptr<FatBinary> Parser::parse(const std::vector<uint8_t>& data,
                                         const ParserConfig& conf) {
  if (!is_macho(data)) {
    LIEF_ERR("Provided data does not appear to be a Mach-O binary");
    return nullptr;
  }

  Parser parser{data, conf};
  parser.parse();
  return std::unique_ptr<FatBinary>(new FatBinary{std::move(parser.binaries_)});
}

std::unique_ptr<FatBinary> Parser::parse(std::unique_ptr<BinaryStream> stream,
                                         const ParserConfig& conf) {
  if (stream == nullptr) {
    return nullptr;
  }

  {
    ScopedStream scoped(*stream, 0);
    if (!is_macho(*stream)) {
      return nullptr;
    }
  }

  Parser parser;
  parser.config_ = conf;
  parser.stream_ = std::move(stream);

  if (!parser.parse()) {
    return nullptr;
  }

  return std::unique_ptr<FatBinary>(new FatBinary{std::move(parser.binaries_)});
}

std::unique_ptr<FatBinary> Parser::parse_from_memory(uintptr_t address,
                                                     size_t size,
                                                     const ParserConfig& conf) {
  if (conf.fix_from_memory &&
      (!conf.parse_dyld_rebases || !conf.parse_dyld_bindings))
  {
    LIEF_WARN(
        "fix_from_memory requires both parse_dyld_rebases and parse_dyld_bindings"
    );
    return nullptr;
  }
  Parser parser;
  parser.stream_ = std::make_unique<MemoryStream>(address, size);
  parser.config_ = conf;
  if (!parser.parse()) {
    LIEF_WARN("Parsing errors at address {:#x} (size: {:#x})", address, size);
  }
  if (parser.binaries_.empty()) {
    return nullptr;
  }

  for (std::unique_ptr<Binary>& bin : parser.binaries_) {
    bin->in_memory_base_addr_ = address;
  }

  if (parser.config_.fix_from_memory) {
    parser.undo_reloc_bindings(address);
    parser.unpack_tlv();
  }
  return std::unique_ptr<FatBinary>(new FatBinary{std::move(parser.binaries_)});
}

std::unique_ptr<FatBinary> Parser::parse_from_memory(uintptr_t address,
                                                     const ParserConfig& conf) {
  static constexpr size_t MAX_SIZE = std::numeric_limits<size_t>::max() >> 2;
  return parse_from_memory(address, MAX_SIZE, conf);
}


std::unique_ptr<FatBinary> Parser::parse_from_dump(const std::string& filepath,
                                                   uint64_t addr,
                                                   const ParserConfig& conf) {
  auto stream = VectorStream::from_file(filepath);
  if (!stream) {
    return nullptr;
  }
  return parse_from_dump(std::make_unique<VectorStream>(std::move(*stream)), addr,
                         conf);
}

std::unique_ptr<FatBinary> Parser::parse_from_dump(BinaryStream& stream,
                                                   uint64_t addr,
                                                   const ParserConfig& conf) {
  return parse(std::make_unique<DumpStream>(addr, stream), conf);
}

std::unique_ptr<FatBinary>
    Parser::parse_from_dump(std::unique_ptr<BinaryStream> stream, uint64_t addr,
                            const ParserConfig& conf) {
  if (stream == nullptr) {
    return nullptr;
  }
  return parse(std::make_unique<DumpStream>(addr, std::move(stream)), conf);
}

ok_error_t Parser::parse_fat() {
  static constexpr size_t MAX_FAT_ARCH = 30;
  stream_->setpos(0);
  const auto header = stream_->read<details::fat_header>();
  if (!header) {
    LIEF_ERR("Failed to read FAT header");
    return make_error_code(lief_errors::read_error);
  }
  uint32_t nb_arch = Swap4Bytes(header->nfat_arch);
  LIEF_DEBUG("#{:d} architectures", nb_arch);

  if (nb_arch > MAX_FAT_ARCH) {
    LIEF_ERR("Too many architectures");
    return make_error_code(lief_errors::parsing_error);
  }

  for (size_t i = 0; i < nb_arch; ++i) {
    auto res_arch = stream_->read<details::fat_arch>();
    if (!res_arch) {
      LIEF_ERR("Failed to read arch #{}", i);
      break;
    }
    const auto arch = *res_arch;

    const uint32_t offset = get_swapped_endian(arch.offset);
    const uint32_t size = get_swapped_endian(arch.size);

    LIEF_DEBUG("Dealing with arch[{:d}]", i);
    LIEF_DEBUG("    [{:d}].offset: {:#06x}", i, offset);
    LIEF_DEBUG("    [{:d}].size  : {:#06x}", i, size);

    std::vector<uint8_t> macho_data;
    if (!stream_->peek_data(macho_data, offset, size)) {
      LIEF_ERR("Mach-O #{:d} is corrupted", i);
      continue;
    }

    std::unique_ptr<Binary> bin = BinaryParser::parse(macho_data, offset, config_);
    if (bin == nullptr) {
      LIEF_ERR("Failed to parse binary at index {:d}", i);
      continue;
    }
    binaries_.push_back(std::move(bin));
  }
  return ok();
}

ok_error_t Parser::parse() {
  auto res_type = stream_->peek<uint32_t>();
  if (!res_type) {
    return make_error_code(lief_errors::parsing_error);
  }
  auto type = static_cast<MACHO_TYPES>(*res_type);

  // Fat binary
  if (type == MACHO_TYPES::MAGIC_FAT || type == MACHO_TYPES::CIGAM_FAT) {
    if (!parse_fat()) {
      LIEF_WARN("Errors while parsing Fat Mach-O");
    }
    return ok();
  }

  const size_t original_size = stream_->size();
  std::unique_ptr<Binary> bin =
      BinaryParser::parse(std::move(stream_), 0, config_);
  if (bin == nullptr) {
    return make_error_code(lief_errors::parsing_error);
  }

  bin->original_size_ = original_size;
  binaries_.push_back(std::move(bin));

  return ok();
}

ok_error_t Parser::unpack_tlv() {
  using Thunk = ThreadLocalVariables::Thunk;
  // Since dyld-1285.19 the TLV thunk are packed at runtime:
  //
  // On-Disk:                 | In Memory
  // {                        | {
  //    uintptr_t func;       |   uintptr_t func;
  //    size_t    key;        |   uint32_t  key;
  //                          |   uint32_t  offset;
  //    size_t    offset;     |   int32_t   initialContentDelta;
  //                          |   uint32_t  initialContentSize;
  // }                        | }
  //
  // The heuristic to detect whether thunks are packed or not is to compare
  // the `initialContentSize` with Binary::tlv_initial_content_range
  if (!config_.fix_from_memory) {
    return ok();
  }

  for (std::unique_ptr<Binary>& bin : binaries_) {
    if (!bin->header().is_64bit()) {
      continue;
    }
    const Binary::range_t& initial_content = bin->tlv_initial_content_range();
    for (Section& S : bin->sections()) {
      auto* tlv_sec = S.cast<ThreadLocalVariables>();
      if (tlv_sec == nullptr) {
        continue;
      }

      auto thunks = tlv_sec->thunks();

      for (auto i = 0; i < thunks.size(); ++i) {
        Thunk T = thunks[i];
        const auto initialContentSize = (uint32_t)(T.offset >> 32);
        if (!initial_content.empty() &&
            initial_content.size() == initialContentSize)
        {
          LIEF_DEBUG("fixing thunk #{}. offset: {:#06x} -> {:#06x}", i, T.offset,
                     T.key >> 32);
          T.offset = T.key >> 32;
          T.key = 0;
          tlv_sec->set(i, T);
        }
      }
    }
  }

  return ok();
}

ok_error_t Parser::undo_reloc_bindings(uintptr_t base_address) {
  if (!config_.fix_from_memory) {
    return ok();
  }
  for (std::unique_ptr<Binary>& bin : binaries_) {
    for (Relocation& reloc : bin->relocations()) {
      if ([[maybe_unused]] const auto* fixup = reloc.cast<RelocationFixup>()) {
        // TODO(romain): add support for this fixup
      } else if ([[maybe_unused]] const auto* dyld_reloc =
                     reloc.cast<RelocationDyld>())
      {
        span<const uint8_t> content =
            bin->get_content_from_virtual_address(reloc.address(),
                                                  sizeof(uintptr_t));
        if (content.empty() || content.size() != sizeof(uintptr_t)) {
          LIEF_WARN("Failed to access relocation data at {:#x}", reloc.address());
          continue;
        }
        const auto value = *reinterpret_cast<const uintptr_t*>(content.data());
        bin->patch_address(reloc.address(),
                           value - base_address + bin->imagebase(),
                           sizeof(uintptr_t));
      }
    }
    if (const DyldInfo* info = bin->dyld_info()) {
      for (const DyldBindingInfo& bindinfo : info->bindings()) {
        if (bindinfo.binding_class() == DyldBindingInfo::CLASS::STANDARD) {
          bin->patch_address(bindinfo.address(), 0, sizeof(uintptr_t));
        }
      }
    }
  }
  return ok();
}

} // namespace LIEF::MachO
