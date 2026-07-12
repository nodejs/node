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
#include "spdlog/fmt/fmt.h"
#include "LIEF/MachO/DyldChainedFixups.hpp"
#include "LIEF/MachO/ChainedBindingInfo.hpp"
#include "LIEF/MachO/hash.hpp"
#include "LIEF/MachO/SegmentCommand.hpp"
#include "LIEF/MachO/DylibCommand.hpp"
#include "LIEF/MachO/RelocationFixup.hpp"
#include "LIEF/MachO/Symbol.hpp"

#include "MachO/Structures.hpp"
#include "MachO/ChainedFixup.hpp"
#include "MachO/ChainedBindingInfoList.hpp"

namespace LIEF {
namespace MachO {
DyldChainedFixups::~DyldChainedFixups() = default;
DyldChainedFixups::DyldChainedFixups() = default;

DyldChainedFixups& DyldChainedFixups::operator=(const DyldChainedFixups& other) {
  if (&other != this) {
    data_offset_ = other.data_offset_;
    data_size_ = other.data_size_;
  }
  return *this;
}

DyldChainedFixups::DyldChainedFixups(const DyldChainedFixups& other) :
  LoadCommand::LoadCommand(other),
  data_offset_{other.data_offset_},
  data_size_{other.data_size_}
{}

DyldChainedFixups::DyldChainedFixups(const details::linkedit_data_command& cmd) :
  LoadCommand::LoadCommand{LoadCommand::TYPE(cmd.cmd), cmd.cmdsize},
  data_offset_{cmd.dataoff},
  data_size_{cmd.datasize}
{}


void DyldChainedFixups::update_with(const details::dyld_chained_fixups_header& header) {
  fixups_version_  = header.fixups_version;
  starts_offset_   = header.starts_offset;
  imports_offset_  = header.imports_offset;
  symbols_offset_  = header.symbols_offset;
  imports_count_   = header.imports_count;
  symbols_format_  = header.symbols_format;
  imports_format_  = static_cast<DYLD_CHAINED_FORMAT>(header.imports_format);
}

void DyldChainedFixups::accept(Visitor& visitor) const {
  visitor.visit(*this);
}

std::ostream& DyldChainedFixups::print(std::ostream& os) const {
  auto segments = chained_starts_in_segments();

  os << "chained fixups header\n";
  os << fmt::format("fixups_version = {}\n", fixups_version());
  os << fmt::format("starts_offset  = {}\n", starts_offset());
  os << fmt::format("imports_offset = {}\n", imports_offset());
  os << fmt::format("symbols_offset = {}\n", symbols_offset());
  os << fmt::format("imports_count  = {}\n", imports_count());
  os << fmt::format("imports_format = {}\n", to_string(imports_format()));
  os << fmt::format("symbols_format = {}\n", symbols_format());
  os << "chained start in image\n";
  os << fmt::format("seg_count = {}\n", segments.size());

  for (size_t i = 0; i < segments.size(); ++i) {
    const chained_starts_in_segment& info = segments[i];
    static constexpr char PREFIX[] = "  ";
    os << fmt::format("{}seg_offset[{}] = {:03d} ({})\n", PREFIX, i,  info.offset, info.segment.name());
  }

  for (size_t i = 0; i < segments.size(); ++i) {
    const chained_starts_in_segment& info = segments[i];
    if (info.offset == 0) {
      continue;
    }
    os << fmt::format("chained starts in segment {} ({})\n", i, info.segment.name());
    os << info << "\n";

    for (const Relocation& reloc : info.segment.relocations()) {
      if (const auto* r = reloc.cast<RelocationFixup>()) {
        os << fmt::format("[RELOC] 0x{:08x}: 0x{:08x}\n", r->address(), r->target());
      }
    }
    os << "\n";
  }

  // Print chained imports
  os << "chained imports\n";
  for (const ChainedBindingInfo& bind : bindings()) {
    std::string segment_name;
    std::string libname;
    std::string symbol;
    if (const auto* segment = bind.segment()) {
      segment_name = segment->name();
    }

    if (const auto* lib = bind.library()) {
      libname = lib->name();
      auto pos = libname.rfind('/');
      if (pos != std::string::npos) {
        libname = libname.substr(pos + 1);
      }
    }

    if (const auto* sym = bind.symbol()) {
      symbol = sym->name();
    }

    os << fmt::format("{}0x{:08x}: {} ({}) addend: 0x{:x}\n",
                      segment_name, bind.address(), symbol, libname, bind.sign_extended_addend());


  }
  return os;
}

DyldChainedFixups::chained_starts_in_segment::chained_starts_in_segment(uint32_t offset, const details::dyld_chained_starts_in_segment& info,
                                                                        SegmentCommand& segment) :
  offset{offset},
  size{info.size},
  page_size{info.page_size},
  segment_offset{info.segment_offset},
  max_valid_pointer{info.max_valid_pointer},
  pointer_format{static_cast<DYLD_CHAINED_PTR_FORMAT>(info.pointer_format)},
  segment{segment}
{}

std::ostream& operator<<(std::ostream& os, const DyldChainedFixups::chained_starts_in_segment& info) {
  os << fmt::format("size              = {}\n",     info.size);
  os << fmt::format("page_size         = 0x{:x}\n", info.page_size);
  os << fmt::format("pointer_format    = {}\n",     to_string(info.pointer_format));
  os << fmt::format("segment_offset    = 0x{:x}\n", info.segment_offset);
  os << fmt::format("max_valid_pointer = 0x{:x}\n", info.max_valid_pointer);
  os << fmt::format("page_count        = {}\n",     info.page_count());

  for (size_t i = 0; i < info.page_count(); ++i) {
    os << fmt::format("  page_start[{}] = {}\n", i, info.page_start[i]);
  }
  return os;
}

}
}
