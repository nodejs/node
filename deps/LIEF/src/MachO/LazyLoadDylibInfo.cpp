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
#include <algorithm>
#include <limits>

#include "spdlog/fmt/fmt.h"

#include "LIEF/BinaryStream/BinaryStream.hpp"
#include "LIEF/BinaryStream/SpanStream.hpp"
#include "LIEF/iostream.hpp"
#include "LIEF/utils.hpp"

#include "LIEF/MachO/LazyLoadDylibInfo.hpp"
#include "LIEF/MachO/SegmentCommand.hpp"
#include "LIEF/MachO/ChainedPointerAnalysis.hpp"

#include "MachO/Structures.hpp"

#include "logging.hpp"


namespace LIEF::MachO {

LazyLoadDylibInfo::LazyLoadDylibInfo() :
  LoadCommand::LoadCommand{LoadCommand::TYPE::LAZY_LOAD_DYLIB_INFO,
                           sizeof(details::linkedit_data_command)} {}

LazyLoadDylibInfo::LazyLoadDylibInfo(const details::linkedit_data_command& cmd) :
  LoadCommand::LoadCommand{LoadCommand::TYPE(cmd.cmd), cmd.cmdsize},
  data_offset_{cmd.dataoff},
  data_size_{cmd.datasize} {}


ok_error_t LazyLoadDylibInfo::walk_fixups(uint64_t chain_va, SegmentCommand& seg) {
  const uint64_t seg_va = seg.virtual_address();
  span<uint8_t> seg_content = seg.content();

  SpanStream chain_stream(seg_content);
  assert(chain_va >= seg_va);
  chain_stream.setpos(chain_va - seg_va);

  const auto ptr_fmt = DYLD_CHAINED_PTR_FORMAT(pointer_format());

  ChainedPointerAnalysis::walk_chain(
      chain_stream, ptr_fmt,
      [&](uint64_t offset, const ChainedPointerAnalysis::union_pointer_t& ptr) {
        auto ordinal = ptr.ordinal();
        if (!ordinal) {
          return 0;
        }
        std::string symbol = *ordinal < symbols_.size() ? symbols_[*ordinal] : "";
        fixups_.emplace_back(seg_va + offset, *ordinal, std::move(symbol),
                             ptr.is_auth());
        return 0;
      }
  );
  return ok();
}

ok_error_t LazyLoadDylibInfo::parse_payload(BinaryStream& stream) {
  auto load_path_offset = stream.read<uint32_t>();
  if (!load_path_offset) {
    LIEF_DEBUG("Failed to read LazyLoadDylibInfo.loadPathOffset");
    return make_error_code(load_path_offset.error());
  }

  auto flag_image_offset = stream.read<uint32_t>();
  if (!flag_image_offset) {
    LIEF_DEBUG("Failed to read LazyLoadDylibInfo.flagImageOffset");
    return make_error_code(flag_image_offset.error());
  }

  auto flags = stream.read<uint16_t>();
  if (!flags) {
    LIEF_DEBUG("Failed to read LazyLoadDylibInfo.flags");
    return make_error_code(flags.error());
  }

  auto pointer_format = stream.read<uint16_t>();
  if (!pointer_format) {
    LIEF_DEBUG("Failed to read LazyLoadDylibInfo.pointerFormat");
    return make_error_code(pointer_format.error());
  }

  auto chain_start_image_offset = stream.read<uint32_t>();
  if (!chain_start_image_offset) {
    LIEF_DEBUG("Failed to read LazyLoadDylibInfo.chainStartImageOffset");
    return make_error_code(chain_start_image_offset.error());
  }

  auto symbols_count = stream.read<uint32_t>();
  if (!symbols_count) {
    LIEF_DEBUG("Failed to read LazyLoadDylibInfo.symbolsCount");
    return make_error_code(symbols_count.error());
  }

  auto symbol_string_array_offset = stream.read<uint32_t>();
  if (!symbol_string_array_offset) {
    LIEF_DEBUG("Failed to read LazyLoadDylibInfo.symbolStringArrayOffset");
    return make_error_code(symbol_string_array_offset.error());
  }

  flag_image_offset_ = *flag_image_offset;
  flags_ = *flags;
  pointer_format_ = *pointer_format;
  chain_start_image_offset_ = *chain_start_image_offset;

  if (auto path = stream.peek_string_at(*load_path_offset)) {
    load_path_ = std::move(*path);
  } else {
    LIEF_DEBUG("Failed to read LazyLoadDylibInfo's load path at offset {:#x}",
               *load_path_offset);
  }

  const size_t max_symbols = stream.size() / sizeof(uint32_t);
  symbols_.reserve(std::min<size_t>(*symbols_count, max_symbols));

  for (size_t i = 0; i < *symbols_count; ++i) {
    const uint64_t off = *symbol_string_array_offset + i * sizeof(uint32_t);
    auto symbol_offset = stream.peek<uint32_t>(off);
    if (!symbol_offset) {
      LIEF_DEBUG("Failed to read LazyLoadDylibInfo.symbolStringOffsets[{}]", i);
      break;
    }

    if (auto symbol = stream.peek_string_at(*symbol_offset)) {
      symbols_.push_back(std::move(*symbol));
    } else {
      LIEF_DEBUG("Failed to read LazyLoadDylibInfo's symbol #{} at offset {:#x}",
                 i, *symbol_offset);
    }
  }

  return ok();
}

ok_error_t LazyLoadDylibInfo::serialize(vector_iostream& ios) const {
  // Mirror mach_o::LazyLoadDylibWriter.
  // [0]                    header (24 bytes, the 7 LazyLoadDylibLinkEdit fields)
  // [24]                   symbol string-offset array (4 * symbolsCount)
  // [24 + 4*symbolsCount]  load path string (NUL-terminated)
  // [...]                  the symbol strings (each NUL-terminated)
  static constexpr uint32_t HEADER_SIZE = 24;
  const uint64_t symbols_count = symbols_.size();
  const uint64_t symbol_string_array_offset = HEADER_SIZE;
  const uint64_t load_path_offset = HEADER_SIZE + symbols_count * sizeof(uint32_t);

  std::vector<uint64_t> symbol_offsets;
  symbol_offsets.reserve(symbols().size());
  uint64_t pos = load_path_offset + load_path_.size() + 1;
  for (const std::string& symbol : symbols()) {
    symbol_offsets.push_back(pos);
    pos += symbol.size() + 1;
  }

  // LINKEDIT content must be pointer-size aligned
  const auto total_size = align_up<uint64_t>(pos, sizeof(uint64_t));
  if (total_size > std::numeric_limits<uint32_t>::max()) {
    LIEF_ERR("LazyLoadDylibInfo payload is too large ({} bytes): its 32-bit "
             "offsets cannot be encoded",
             total_size);
    return make_error_code(lief_errors::build_error);
  }

  // header
  (ios)
      .write<uint32_t>((uint32_t)load_path_offset)
      .write<uint32_t>(flag_image_offset_)
      .write<uint16_t>(flags_)
      .write<uint16_t>(pointer_format_)
      .write<uint32_t>(chain_start_image_offset_)
      .write<uint32_t>((uint32_t)symbols_count)
      .write<uint32_t>((uint32_t)symbol_string_array_offset);

  // symbol string offsets array
  for (uint64_t offset : symbol_offsets) {
    ios.write<uint32_t>((uint32_t)offset);
  }

  ios.write(load_path_);
  for (const std::string& symbol : symbols_) {
    ios.write(symbol);
  }
  return ok();
}

std::string LazyLoadDylibInfo::Fixup::to_string() const {
  return fmt::format("{:#010x}: {} (ordinal={}{})", address(), symbol(), ordinal(),
                     is_auth() ? ", auth" : "");
}

std::ostream& LazyLoadDylibInfo::print(std::ostream& os) const {
  LoadCommand::print(os) << '\n';
  os << fmt::format("Load path: {}\n", load_path());
  os << fmt::format("Flags: {:#06x} (may be missing: {})\n", flags(),
                    may_be_missing());
  os << fmt::format("Pointer format: {:#06x}\n", pointer_format());
  os << fmt::format("Flag image offset: {:#010x}\n", flag_image_offset());
  os << fmt::format("Chain start image offset: {:#010x}\n",
                    chain_start_image_offset());
  os << fmt::format("Symbols ({}):\n", symbols().size());
  for (const std::string& symbol : symbols()) {
    os << fmt::format("  {}\n", symbol);
  }
  os << fmt::format("Fixups ({}):\n", fixups_.size());
  for (const Fixup& fixup : fixups_) {
    os << fmt::format("  {}\n", fixup.to_string());
  }
  return os;
}

}
