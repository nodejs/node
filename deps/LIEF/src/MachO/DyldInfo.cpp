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
#include "logging.hpp"
#include "frozen.hpp"
#include "LIEF/iostream.hpp"

#include "LIEF/Abstract/Binary.hpp"

#include "LIEF/BinaryStream/SpanStream.hpp"

#include "LIEF/Visitor.hpp"

#include "LIEF/MachO/Binary.hpp"
#include "LIEF/MachO/DyldInfo.hpp"
#include "LIEF/MachO/DyldBindingInfo.hpp"
#include "LIEF/MachO/ExportInfo.hpp"
#include "LIEF/MachO/RelocationDyld.hpp"
#include "LIEF/MachO/SegmentCommand.hpp"
#include "LIEF/MachO/Symbol.hpp"
#include "LIEF/MachO/DylibCommand.hpp"

#include "MachO/exports_trie.hpp"
#include "MachO/Structures.hpp"

#include "Object.tcc"

namespace LIEF {
namespace MachO {

struct ThreadedBindData {
  std::string symbol_name;
  int64_t addend          = 0;
  int64_t library_ordinal = 0;
  uint8_t symbol_flags    = 0;
  uint8_t type            = 0;
};

DyldInfo::DyldInfo() = default;
DyldInfo::~DyldInfo() = default;

DyldInfo& DyldInfo::operator=(DyldInfo other) {
  swap(other);
  return *this;
}

DyldInfo::DyldInfo(const DyldInfo& copy) :
  LoadCommand::LoadCommand{copy},
  rebase_{copy.rebase_},
  rebase_opcodes_{copy.rebase_opcodes_},
  bind_{copy.bind_},
  bind_opcodes_{copy.bind_opcodes_},
  weak_bind_{copy.weak_bind_},
  weak_bind_opcodes_{copy.weak_bind_opcodes_},
  lazy_bind_{copy.lazy_bind_},
  lazy_bind_opcodes_{copy.lazy_bind_opcodes_},
  export_{copy.export_},
  export_trie_{copy.export_trie_}
{}

DyldInfo::DyldInfo(const details::dyld_info_command& dyld_info_cmd) :
  LoadCommand::LoadCommand{LoadCommand::TYPE(dyld_info_cmd.cmd), dyld_info_cmd.cmdsize},
  rebase_{dyld_info_cmd.rebase_off, dyld_info_cmd.rebase_size},
  bind_{dyld_info_cmd.bind_off, dyld_info_cmd.bind_size},
  weak_bind_{dyld_info_cmd.weak_bind_off, dyld_info_cmd.weak_bind_size},
  lazy_bind_{dyld_info_cmd.lazy_bind_off, dyld_info_cmd.lazy_bind_size},
  export_{dyld_info_cmd.export_off, dyld_info_cmd.export_size}
{}


void DyldInfo::swap(DyldInfo& other) noexcept {
  LoadCommand::swap(other);

  std::swap(rebase_,             other.rebase_);
  std::swap(rebase_opcodes_,     other.rebase_opcodes_);

  std::swap(bind_,               other.bind_);
  std::swap(bind_opcodes_,       other.bind_opcodes_);

  std::swap(weak_bind_,          other.weak_bind_);
  std::swap(weak_bind_opcodes_,  other.weak_bind_opcodes_);

  std::swap(lazy_bind_,          other.lazy_bind_);
  std::swap(lazy_bind_opcodes_,  other.lazy_bind_opcodes_);

  std::swap(export_,             other.export_);
  std::swap(export_trie_,        other.export_trie_);

  std::swap(export_info_,        other.export_info_);
  std::swap(binding_info_,       other.binding_info_);

  std::swap(binary_,             other.binary_);
}

void DyldInfo::rebase_opcodes(buffer_t raw) {
  if (raw.size() > rebase_opcodes_.size()) {
    LIEF_WARN("Can't update rebase opcodes. The provided data is larger than the original ones");
    return;
  }
  std::move(std::begin(raw), std::end(raw), rebase_opcodes_.data());
}


std::string DyldInfo::show_rebases_opcodes() const {
  static constexpr char tab[] = "    ";

  if (binary_ == nullptr) {
    LIEF_WARN("Can't print rebase opcode");
    return "";
  }

  size_t pint_v = static_cast<LIEF::Binary*>(binary_)->header().is_64() ? sizeof(uint64_t) : sizeof(uint32_t);
  std::ostringstream output;

  bool     done = false;
  uint8_t  type = 0;
  uint32_t segment_index = 0;
  uint64_t segment_offset = 0;
  uint32_t count = 0;
  uint32_t skip = 0;
  SpanStream rebase_stream = rebase_opcodes();

  Binary::it_segments segments = binary_->segments();

  while (!done && rebase_stream.pos() < rebase_stream.size()) {
    auto val = rebase_stream.read<uint8_t>();
    if (!val) {
      break;
    }
    uint8_t imm = *val & IMMEDIATE_MASK;
    auto opcode = REBASE_OPCODES(*val & OPCODE_MASK);

    switch(opcode) {
      case REBASE_OPCODES::DONE:
        {
          output << "[" << to_string(opcode) << "]" << '\n';
          done = true;
          break;
        }

      case REBASE_OPCODES::SET_TYPE_IMM:
        {
          type = imm;
          output << "[" << to_string(opcode) << "] ";
          output << "Type: " << to_string(REBASE_TYPE(type));
          output << '\n';
          break;
        }

      case REBASE_OPCODES::SET_SEGMENT_AND_OFFSET_ULEB:
        {
          if (auto val = rebase_stream.read_uleb128()) {
            segment_offset = *val;
          } else {
            LIEF_ERR("Can't read REBASE_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB segment offset");
            break;
          }

          segment_index = imm;

          output << "[" << to_string(opcode) << "] ";
          output << "Segment Index := " << std::dec << segment_index << " (" << segments[segment_index].name() << ") ";
          output << "Segment Offset := " << std::hex << std::showbase << segment_offset;
          output << '\n';

          break;
        }

      case REBASE_OPCODES::ADD_ADDR_ULEB:
        {
          uint64_t val = 0;
          if (auto res = rebase_stream.read_uleb128()) {
            val = *res;
          } else {
            LIEF_ERR("Can't read REBASE_OPCODE_ADD_ADDR_ULEB segment offset");
            break;
          }

          segment_offset += val;

          output << "[" << to_string(opcode) << "] ";
          output << "Segment Offset += " << std::hex << std::showbase << val << " (" << segment_offset << ")";
          output << '\n';
          break;
        }

      case REBASE_OPCODES::ADD_ADDR_IMM_SCALED:
        {
          segment_offset += (imm * pint_v);

          output << "[" << to_string(opcode) << "]" ;
          output << "Segment Offset += " << std::hex << std::showbase << (imm * pint_v) << " (" << segment_offset << ")";
          output << '\n';
          break;
        }

      case REBASE_OPCODES::DO_REBASE_IMM_TIMES:
        {
          output << "[" << to_string(opcode) << "]" << '\n';
          output << tab << "for i in range(" << std::dec << static_cast<uint32_t>(imm) << "):" << '\n';
          for (size_t i = 0; i < imm; ++i) {
            output << tab << tab;
            output << "rebase(";
            output << to_string(REBASE_TYPE(type));
            output << ", ";
            output << segments[segment_index].name();
            output << ", ";
            output << std::hex << std::showbase << segment_offset;
            output << ")" << '\n';

            segment_offset += pint_v;

            output << tab << tab;
            output << "Segment Offset += " << std::hex << std::showbase << pint_v << " (" << segment_offset << ")";
            output << '\n' << '\n';
          }
          output << '\n';
          break;
        }
      case REBASE_OPCODES::DO_REBASE_ULEB_TIMES:
        {

          if (auto res = rebase_stream.read_uleb128()) {
            count = *res;
          } else {
            LIEF_ERR("Can't read REBASE_OPCODE_DO_REBASE_ULEB_TIMES count");
            break;
          }

          output << "[" << to_string(opcode) << "]" << '\n';

          output << tab << "for i in range(" << std::dec << static_cast<uint32_t>(count) << "):" << '\n';
          for (size_t i = 0; i < count; ++i) {
            output << tab << tab;
            output << "rebase(";
            output << to_string(REBASE_TYPE(type));
            output << ", ";
            output << segments[segment_index].name();
            output << ", ";
            output << std::hex << std::showbase << segment_offset;
            output << ")" << '\n';

            segment_offset += pint_v;

            output << tab << tab;
            output << "Segment Offset += " << std::hex << std::showbase << pint_v << " (" << segment_offset << ")";
            output << '\n' << '\n';
          }

          output << '\n';
          break;
        }

      case REBASE_OPCODES::DO_REBASE_ADD_ADDR_ULEB:
        {

          output << "[" << to_string(opcode) << "]" << '\n';

          output << tab;
          output << "rebase(";
          output << to_string(REBASE_TYPE(type));
          output << ", ";
          output << segments[segment_index].name();
          output << ", ";
          output << std::hex << std::showbase << segment_offset;
          output << ")" << '\n';

          uint64_t val = 0;
          if (auto res = rebase_stream.read_uleb128()) {
            val = *res;
          } else {
            LIEF_ERR("Can't read REBASE_OPCODE_DO_REBASE_ADD_ADDR_ULEB value");
            break;
          }
          segment_offset += val + pint_v;


          output << tab;
          output << "Segment Offset += " << std::hex << std::showbase << (val + pint_v) << " (" << segment_offset << ")";
          output << '\n';

          break;
        }

      case REBASE_OPCODES::DO_REBASE_ULEB_TIMES_SKIPPING_ULEB:
        {

          output << "[" << to_string(static_cast<REBASE_OPCODES>(opcode)) << "]" << '\n';

          // Count
          if (auto res = rebase_stream.read_uleb128()) {
            count = *res;
          } else {
            LIEF_ERR("Can't read REBASE_OPCODE_DO_REBASE_ULEB_TIMES_SKIPPING_ULEB count");
            break;
          }

          // Skip
          if (auto res = rebase_stream.read_uleb128()) {
            skip = *res;
          } else {
            LIEF_ERR("Can't read REBASE_OPCODE_DO_REBASE_ULEB_TIMES_SKIPPING_ULEB skip");
            break;
          }

          output << tab << "for i in range(" << std::dec << static_cast<uint32_t>(count) << "):" << '\n';
          for (size_t i = 0; i < count; ++i) {
            output << tab << tab;
            output << "rebase(";
            output << to_string(REBASE_TYPE(type));
            output << ", ";
            output << segments[segment_index].name();
            output << ", ";
            output << std::hex << std::showbase << segment_offset;
            output << ")" << '\n';

            segment_offset += skip + pint_v;

            output << tab << tab;
            output << "Segment Offset += " << std::hex << std::showbase << skip << " + " << pint_v << " (" << segment_offset << ")";
            output << '\n' << '\n';
          }

          break;
        }

      default:
        {
          output << "[UNSUPPORTED OPCODE - " << std::showbase << std::hex << static_cast<uint32_t>(opcode) << "]" << '\n';
          break;
        }
    }
  }

  return output.str();
}

void DyldInfo::bind_opcodes(buffer_t raw) {
  if (raw.size() > bind_opcodes_.size()) {
    LIEF_WARN("Can't update bind opcodes. The provided data is larger than the original ones");
    return;
  }
  std::move(std::begin(raw), std::end(raw), bind_opcodes_.data());
}


std::string DyldInfo::show_bind_opcodes() const {
  std::ostringstream output;
  show_bindings(output, bind_opcodes(), /* is_lazy = */ false);
  return output.str();
}

void DyldInfo::show_bindings(std::ostream& output, span<const uint8_t> bind_opcodes, bool is_lazy) const {

  if (binary_ == nullptr) {
    LIEF_WARN("Can't print bind opcodes");
    return;
  }

  size_t pint_v = static_cast<LIEF::Binary*>(binary_)->header().is_64() ?
                  sizeof(uint64_t) :
                  sizeof(uint32_t);

  uint8_t     type = is_lazy ? static_cast<uint8_t>(DyldBindingInfo::TYPE::POINTER) : 0;
  uint8_t     segment_idx = 0;
  uint64_t    segment_offset = 0;
  std::string symbol_name;
  int64_t     library_ordinal = 0;

  int64_t     addend = 0;
  uint32_t    count = 0;
  uint32_t    skip = 0;

  bool        is_weak_import = false;
  bool        done = false;

  size_t ordinal_table_size = 0;
  bool use_threaded_rebase_bind = false;
  uint8_t symbol_flags = 0;
  std::vector<ThreadedBindData> ordinal_table;

  Binary::it_segments segments = binary_->segments();
  Binary::it_libraries libraries = binary_->libraries();

  const std::string tab = "    ";

  SpanStream bind_stream = bind_opcodes;

  while (!done && bind_stream.pos() < bind_stream.size()) {
    auto val = bind_stream.read<uint8_t>();
    if (!val) {
      break;
    }
    uint8_t imm = *val & IMMEDIATE_MASK;
    auto opcode = BIND_OPCODES(*val & OPCODE_MASK);

    switch (opcode) {
      case BIND_OPCODES::DONE:
        {
          output << "[" << to_string(opcode) << "]" << '\n';
          if (!is_lazy) {
            done = true;
          }
          break;
        }

      case BIND_OPCODES::SET_DYLIB_ORDINAL_IMM:
        {
          output << "[" << to_string(opcode) << "]" << '\n';

          library_ordinal = imm;

          output << tab << "Library Ordinal := " << std::dec << static_cast<uint32_t>(imm) << '\n';
          break;
        }

      case BIND_OPCODES::SET_DYLIB_ORDINAL_ULEB:
        {
          output << "[" << to_string(opcode) << "]" << '\n';

          if (auto res = bind_stream.read_uleb128()) {
            library_ordinal = *res;
          } else {
            LIEF_ERR("Can't read BIND_OPCODE_SET_DYLIB_ORDINAL_ULEB library ordinal value");
            break;
          }

          output << tab << "Library Ordinal := " << std::dec << library_ordinal << '\n';

          break;
        }

      case BIND_OPCODES::SET_DYLIB_SPECIAL_IMM:
        {

          output << "[" << to_string(opcode) << "]" << '\n';
          // the special ordinals are negative numbers
          if (imm == 0) {
            library_ordinal = 0;
          } else {
            int8_t sign_extended = static_cast<int8_t>(OPCODE_MASK) | imm;
            library_ordinal = sign_extended;
          }

          output << tab << "Library Ordinal := " << std::dec << library_ordinal << '\n';
          break;
        }

      case BIND_OPCODES::SET_SYMBOL_TRAILING_FLAGS_IMM:
        {

          output << "[" << to_string(opcode) << "]" << '\n';

          if (auto res = bind_stream.read_string()) {
            symbol_name = std::move(*res);
          } else {
            LIEF_ERR("Can't read BIND_OPCODE_SET_SYMBOL_TRAILING_FLAGS_IMM symbol's name");
            break;
          }
          symbol_flags = imm;

          is_weak_import = (imm & BIND_SYMBOL_FLAGS::WEAK_IMPORT) != 0;

          output << tab << "Symbol name := " << symbol_name << '\n';
          output << tab << "Is Weak ? " << std::boolalpha << is_weak_import << '\n';
          break;
        }

      case BIND_OPCODES::SET_TYPE_IMM:
        {
          output << "[" << to_string(opcode) << "]" << '\n';

          type = imm;

          output << tab << "Type := " << to_string(DyldBindingInfo::TYPE(type)) << '\n';

          break;
        }

      case BIND_OPCODES::SET_ADDEND_SLEB:
        {
          output << "[" << to_string(opcode) << "]" << '\n';

          if (auto res = bind_stream.read_sleb128()) {
            addend = *res;
          } else {
            LIEF_ERR("Can't read BIND_OPCODE_SET_ADDEND_SLEB addend");
            break;
          }

          output << tab << "Addend := " << std::dec << addend << '\n';
          break;
        }

      case BIND_OPCODES::SET_SEGMENT_AND_OFFSET_ULEB:
        {

          output << "[" << to_string(opcode) << "]" << '\n';
          segment_idx  = imm;

          if (auto res = bind_stream.read_uleb128()) {
            segment_offset = *res;
          } else {
            LIEF_ERR("Can't read BIND_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB segment offset");
            break;
          }

          output << tab << "Segment := " << segments[segment_idx].name() << '\n';
          output << tab << "Segment Offset := " << std::hex << std::showbase << segment_offset << '\n';

          break;
        }

      case BIND_OPCODES::ADD_ADDR_ULEB:
        {

          output << "[" << to_string(opcode) << "]" << '\n';

          uint64_t val = 0;
          if (auto res = bind_stream.read_uleb128()) {
            val = *res;
          } else {
            LIEF_ERR("Can't read BIND_OPCODE_ADD_ADDR_ULEB val");
            break;
          }
          segment_offset += val;

          output << tab << "Segment Offset += " << std::hex << std::showbase << val << " (" << segment_offset << ")" << '\n';
          break;
        }

      case BIND_OPCODES::DO_BIND:
        {
          if (!use_threaded_rebase_bind) {
            output << "[" << to_string(opcode) << "]" << '\n';

            output << tab;
            output << "bind(";
            output << to_string(DyldBindingInfo::TYPE(type));
            output << ", ";
            output << segments[segment_idx].name();
            output << ", ";
            output << std::hex << std::showbase << segment_offset;
            output << ", ";
            output << symbol_name;
            output << ", library_ordinal=";
            output << (library_ordinal > 0 ? libraries[library_ordinal - 1].name() : std::to_string(library_ordinal));
            output << ", addend=";
            output << std::dec << addend;
            output << ", is_weak_import=";
            output << std::boolalpha << is_weak_import;
            output << ")" << '\n';

            segment_offset += pint_v;

            output << tab << "Segment Offset += " << std::hex << std::showbase << pint_v << " (" << segment_offset << ")" << '\n';
          } else {
            ordinal_table.push_back(ThreadedBindData{symbol_name, addend, library_ordinal, symbol_flags, type});
          }
          break;
        }

      case BIND_OPCODES::DO_BIND_ADD_ADDR_ULEB:
        {

          output << "[" << to_string(opcode) << "]" << '\n';

          output << tab;
          output << "bind(";
          output << to_string(DyldBindingInfo::TYPE(type));
          output << ", ";
          output << segments[segment_idx].name();
          output << ", ";
          output << std::hex << std::showbase << segment_offset;
          output << ", ";
          output << symbol_name;
          output << ", library_ordinal=";
          output << (library_ordinal > 0 ? libraries[library_ordinal - 1].name() : std::to_string(library_ordinal));
          output << ", addend=";
          output << std::dec << addend;
          output << ", is_weak_import=";
          output << std::boolalpha << is_weak_import;
          output << ")" << '\n';

          uint64_t v = 0;
          if (auto res = bind_stream.read_uleb128()) {
            v = *res;
          } else {
            LIEF_ERR("Can't read BIND_OPCODE_DO_BIND_ADD_ADDR_ULEB val");
            break;
          }
          segment_offset += v + pint_v;

          output << tab << "Segment Offset += " << std::hex << std::showbase << v + pint_v << " (" << segment_offset << ")" << '\n';
          break;
        }

      case BIND_OPCODES::DO_BIND_ADD_ADDR_IMM_SCALED:
        {

          output << "[" << to_string(opcode) << "]" << '\n';

          output << tab;
          output << "bind(";
          output << to_string(DyldBindingInfo::TYPE(type));
          output << ", ";
          output << segments[segment_idx].name();
          output << ", ";
          output << std::hex << std::showbase << segment_offset;
          output << ", ";
          output << symbol_name;
          output << ", library_ordinal=";
          output << (library_ordinal > 0 ? libraries[library_ordinal - 1].name() : std::to_string(library_ordinal));
          output << ", addend=";
          output << std::dec << addend;
          output << ", is_weak_import=";
          output << std::boolalpha << is_weak_import;
          output << ")" << '\n';

          segment_offset += imm * pint_v + pint_v;

          output << tab << "Segment Offset += " << std::hex << std::showbase << (imm * pint_v + pint_v) << " (" << segment_offset << ")" << '\n';

          break;
        }

      case BIND_OPCODES::DO_BIND_ULEB_TIMES_SKIPPING_ULEB:
        {

          output << "[" << to_string(opcode) << "]" << '\n';
          // Count
          if (auto res = bind_stream.read_uleb128()) {
            count = *res;
          } else {
            LIEF_ERR("Can't read BIND_OPCODE_DO_BIND_ULEB_TIMES_SKIPPING_ULEB count");
            break;
          }

          // Skip
          if (auto res = bind_stream.read_uleb128()) {
            skip = *res;
          } else {
            LIEF_ERR("Can't read BIND_OPCODE_DO_BIND_ULEB_TIMES_SKIPPING_ULEB skip");
            break;
          }

          output << tab << "for i in range(" << std::dec << static_cast<uint32_t>(count) << "):" << '\n';
          for (size_t i = 0; i < count; ++i) {
            output << tab << tab;
            output << "bind(";
            output << to_string(DyldBindingInfo::TYPE(type));
            output << ", ";
            output << segments[segment_idx].name();
            output << ", ";
            output << std::hex << std::showbase << segment_offset;
            output << ", ";
            output << symbol_name;
            output << ", library_ordinal=";
            output << (library_ordinal > 0 ? libraries[library_ordinal - 1].name() : std::to_string(library_ordinal));
            output << ", addend=";
            output << std::dec << addend;
            output << ", is_weak_import=";
            output << std::boolalpha << is_weak_import;
            output << ")" << '\n';


            segment_offset += skip + pint_v;

            output << "Segment Offset += " << std::hex << std::showbase << skip << " + " << pint_v << " (" << segment_offset << ")";

            output << '\n' << '\n';
          }
          break;
        }
      case BIND_OPCODES::THREADED:
        {
          const auto subopcode = static_cast<BIND_SUBOPCODE_THREADED>(imm);
          output << std::string("[") + to_string(BIND_OPCODES::THREADED) + "]\n";
          switch (subopcode) {
            case BIND_SUBOPCODE_THREADED::APPLY:
              {
                output << tab << std::string("[") + to_string(subopcode) + "]\n";
                uint64_t delta = 0;
                const SegmentCommand& current_segment = segments[segment_idx];
                do {
                  const uint64_t address = current_segment.virtual_address() + segment_offset;
                  span<const uint8_t> content = current_segment.content();
                  if (segment_offset >= content.size() ||
                      (segment_offset + sizeof(uint64_t)) >= content.size())
                  {
                    LIEF_WARN("Bad segment offset (0x{:x})", segment_offset);
                    break;
                  }
                  auto value = *reinterpret_cast<const uint64_t*>(content.data() + segment_offset);
                  bool is_rebase = (value & (static_cast<uint64_t>(1) << 62)) == 0;

                  if (is_rebase) {
                    output << tab << tab << fmt::format("rebase({}, {}, 0x{:x})\n",
                        "THREADED_REBASE", current_segment.name(), segment_offset);
                  } else {
                    uint16_t ordinal = value & 0xFFFF;
                    if (ordinal >= ordinal_table_size || ordinal >= ordinal_table.size()) {
                      LIEF_WARN("bind ordinal ({:d}) is out of range (max={:d}) for disk pointer 0x{:04x} in "
                                "segment '{}' (segment offset: 0x{:04x})", ordinal, ordinal_table_size, value,
                                current_segment.name(), segment_offset);
                      break;
                    }
                    if (address < current_segment.virtual_address() ||
                        address >= (current_segment.virtual_address() + current_segment.virtual_size())) {
                      LIEF_WARN("Bad binding address");
                      break;
                    }
                    const ThreadedBindData& th_bind_data = ordinal_table[ordinal];
                    const int64_t library_ordinal = th_bind_data.library_ordinal;
                    output << tab << tab << fmt::format("threaded_bind({}/{}, 0x{:x}, {}, {}, library_ordinal={}, "
                                                        "addend={}, is_weak_import={})\n",
                        "THREADED_BIND", to_string(DyldBindingInfo::TYPE(th_bind_data.type)), segment_offset,
                        current_segment.name(), th_bind_data.symbol_name,
                        library_ordinal > 0 ? libraries[library_ordinal - 1].name() : std::to_string(library_ordinal),
                        th_bind_data.addend, th_bind_data.symbol_flags);
                  }
                  // The delta is bits [51..61]
                  // And bit 62 is to tell us if we are a rebase (0) or bind (1)
                  value &= ~(1ull << 62);
                  delta = (value & 0x3FF8000000000000) >> 51;
                  segment_offset += delta * sizeof(pint_v);
                  output << tab << tab << fmt::format("Segment Offset += 0x{:x} (0x{:x})\n", delta * sizeof(pint_v), segment_offset);
                } while (delta != 0);
                break;
              }
            case BIND_SUBOPCODE_THREADED::SET_BIND_ORDINAL_TABLE_SIZE_ULEB:
              {
                output << tab << std::string("[") + to_string(subopcode) + "]\n";
                if (auto res = bind_stream.read_uleb128()) {
                  count = *res;
                } else {
                  LIEF_ERR("Can't read BIND_SUBOPCODE_THREADED_SET_BIND_ORDINAL_TABLE_SIZE_ULEB count");
                  break;
                }
                ordinal_table_size = count + 1; // the +1 comes from: 'ld64 wrote the wrong value here and we need to offset by 1 for now.'
                use_threaded_rebase_bind = true;
                ordinal_table.reserve(ordinal_table_size);
                output << tab << tab << fmt::format("Ordinal table size := {:d}\n", count);
                break;
              }
          }
          break;
        }

      default:
        {
          LIEF_ERR("Unsupported opcode: 0x{:x}", static_cast<uint32_t>(opcode));
          output << fmt::format("[UNKNOWN OP 0x{:x}]\n", static_cast<uint32_t>(opcode));
          break;
        }
      }
  }
}

// Weak Binding
// ============

void DyldInfo::weak_bind_opcodes(buffer_t raw) {
  if (raw.size() > weak_bind_opcodes_.size()) {
    LIEF_WARN("Can't update weak bind opcodes. The provided data is larger than the original ones");
    return;
  }
  std::move(std::begin(raw), std::end(raw), weak_bind_opcodes_.data());
}


std::string DyldInfo::show_weak_bind_opcodes() const {
  std::ostringstream output;
  show_bindings(output, weak_bind_opcodes(), /* is_lazy = */ false);
  return output.str();
}

// Lazy Binding
// ============
void DyldInfo::lazy_bind_opcodes(buffer_t raw) {
  if (raw.size() > lazy_bind_opcodes_.size()) {
    LIEF_WARN("Can't update lazy bind opcodes. The provided data is larger than the original ones");
    return;
  }
  std::move(std::begin(raw), std::end(raw), lazy_bind_opcodes_.data());
}

std::string DyldInfo::show_lazy_bind_opcodes() const {
  std::ostringstream output;
  show_bindings(output, lazy_bind_opcodes(), true);
  return output.str();
}

// Export Info
// ===========
std::string DyldInfo::show_export_trie() const {
  if (binary_ == nullptr) {
    LIEF_WARN("Can't print bind opcodes");
    return "";
  }

  std::ostringstream output;
  span<const uint8_t> buffer = export_trie();

  SpanStream stream = buffer;
  show_trie(output, "", stream, 0, buffer.size(), "");

  return output.str();
}

void DyldInfo::show_trie(std::ostream& output, std::string output_prefix, BinaryStream& stream,
                         uint64_t start, uint64_t end, const std::string& prefix) const
{
  MachO::show_trie(output, std::move(output_prefix), stream, start, end, prefix);
}

void DyldInfo::export_trie(buffer_t raw) {
  if (raw.size() > export_trie_.size()) {
    LIEF_WARN("Can't update the export trie. The provided data is larger than the original ones");
    return;
  }
  std::move(std::begin(raw), std::end(raw), export_trie_.data());
}

void DyldInfo::accept(Visitor& visitor) const {
  visitor.visit(*this);
}

bool operator==(uint8_t lhs, DyldInfo::REBASE_OPCODES rhs) {
  return lhs == static_cast<uint8_t>(rhs);
}

bool operator!=(uint8_t lhs, DyldInfo::REBASE_OPCODES rhs) {
  return lhs != static_cast<uint8_t>(rhs);
}

DyldInfo& DyldInfo::update_rebase_info(vector_iostream& stream) {
  auto cmp = [] (const RelocationDyld* lhs, const RelocationDyld* rhs) {
    return *lhs < *rhs;
  };

  // In recent version of dyld, relocations are melt with bindings
  if (binding_encoding_version_ != BINDING_ENCODING_VERSION::V1) {
    return *this;
  }

  std::set<RelocationDyld*, decltype(cmp)> rebases(cmp);
  Binary::relocations_t relocations = binary_->relocations_list();
  for (Relocation* r : relocations) {
    if (r->origin() == Relocation::ORIGIN::DYLDINFO) {
      rebases.insert(r->as<RelocationDyld>());
    }
  }


  uint64_t current_segment_start = 0;
  uint64_t current_segment_end = 0;
  uint32_t current_segment_index = 0;
  uint8_t type = 0;
  auto address = static_cast<uint64_t>(-1);
  std::vector<details::rebase_instruction> output;

  for (RelocationDyld* rebase : rebases) {
    if (type != rebase->type()) {
      output.emplace_back(static_cast<uint8_t>(REBASE_OPCODES::SET_TYPE_IMM), rebase->type());
      type = rebase->type();
    }

    if (address != rebase->address()) {
      if (rebase->address() < current_segment_start || rebase->address() >= current_segment_end) {
        SegmentCommand* segment = rebase->segment();
        if (segment == nullptr) {
          LIEF_ERR("No segment associated with the RebaseInfo. Can't update!");
          return *this;
        }
        size_t index = segment->index();

        current_segment_start = segment->virtual_address();
        current_segment_end   = segment->virtual_address() + segment->virtual_size();
        current_segment_index = index;

        output.emplace_back(static_cast<uint8_t>(REBASE_OPCODES::SET_SEGMENT_AND_OFFSET_ULEB), current_segment_index, rebase->address() - current_segment_start);
      } else {
        output.emplace_back(static_cast<uint8_t>(REBASE_OPCODES::ADD_ADDR_ULEB), rebase->address() - address);
      }
      address = rebase->address();
    }

    output.emplace_back(static_cast<uint8_t>(REBASE_OPCODES::DO_REBASE_ULEB_TIMES), 1);
    address += binary_->pointer_size();

    if (address >= current_segment_end) {
      address = 0;
    }
  }
  output.emplace_back(static_cast<uint8_t>(REBASE_OPCODES::DONE), 0);


  // ===========================================
  // 1. First optimization
  // Compress packed runs of pointers
  // Based on ld64-274.2/src/ld/LinkEdit.hpp:239
  // ===========================================
  auto dst = std::begin(output);
  for (auto it = std::begin(output); it->opcode != REBASE_OPCODES::DONE; ++it) {
    if (it->opcode == REBASE_OPCODES::DO_REBASE_ULEB_TIMES && it->op1 == 1) {
      *dst = *it++;

      while (it->opcode == REBASE_OPCODES::DO_REBASE_ULEB_TIMES) {
        dst->op1 += it->op1;
        ++it;
      }

      --it;
      ++dst;
    } else {
      *dst++ = *it;
    }
  }
  dst->opcode = static_cast<uint8_t>(REBASE_OPCODES::DONE);

  // ===========================================
  // 2. Second optimization
  // Combine rebase/add pairs
  // Base on ld64-274.2/src/ld/LinkEdit.hpp:257
  // ===========================================
  dst = std::begin(output);
  for (auto it = std::begin(output); it->opcode != REBASE_OPCODES::DONE; ++it) {

    if ((it->opcode == REBASE_OPCODES::DO_REBASE_ULEB_TIMES)
        && it->op1 == 1
        && it[1].opcode == REBASE_OPCODES::ADD_ADDR_ULEB) {
      dst->opcode = static_cast<uint8_t>(REBASE_OPCODES::DO_REBASE_ADD_ADDR_ULEB);
      dst->op1 = it[1].op1;
      ++it;
      ++dst;
    } else {
      *dst++ = *it;
    }
  }
  dst->opcode = static_cast<uint8_t>(REBASE_OPCODES::DONE);

  // ===========================================
  // 3. Third optimization
  // Base on ld64-274.2/src/ld/LinkEdit.hpp:274
  // ===========================================
  dst = std::begin(output);
  for (auto it = std::begin(output); it->opcode != REBASE_OPCODES::DONE; ++it) {
    uint64_t delta = it->op1;
    if ((it->opcode == REBASE_OPCODES::DO_REBASE_ADD_ADDR_ULEB)
        && (it[1].opcode == REBASE_OPCODES::DO_REBASE_ADD_ADDR_ULEB)
        && (it[2].opcode == REBASE_OPCODES::DO_REBASE_ADD_ADDR_ULEB)
        && (it[1].op1 == delta)
        && (it[2].op1 == delta) ) {

      dst->opcode = static_cast<uint8_t>(REBASE_OPCODES::DO_REBASE_ULEB_TIMES_SKIPPING_ULEB);
      dst->op1 = 1;
      dst->op2 = delta;
      ++it;
      while (it->opcode == REBASE_OPCODES::DO_REBASE_ADD_ADDR_ULEB && it->op1 == delta) {
        dst->op1++;
        ++it;
      }
      --it;
      ++dst;
    } else {
      *dst++ = *it;
    }
  }
  dst->opcode = static_cast<uint8_t>(REBASE_OPCODES::DONE);

  // ===========================================
  // 4. Fourth optimization
  // Use immediate encodings
  // Base on ld64-274.2/src/ld/LinkEdit.hpp:303
  // ===========================================
  const size_t pint_size = binary_->pointer_size();
  for (auto it = std::begin(output); it->opcode != REBASE_OPCODES::DONE; ++it) {

    if (it->opcode == REBASE_OPCODES::ADD_ADDR_ULEB && it->op1 < (15 * pint_size) && (it->op1 % pint_size) == 0) {
      it->opcode = static_cast<uint8_t>(REBASE_OPCODES::ADD_ADDR_IMM_SCALED);
      it->op1 = it->op1 / pint_size;
    } else if ( (it->opcode == REBASE_OPCODES::DO_REBASE_ULEB_TIMES) && (it->op1 < 15) ) {
      it->opcode = static_cast<uint8_t>(REBASE_OPCODES::DO_REBASE_IMM_TIMES);
    }
  }

  vector_iostream raw_output;
  bool done = false;
  for (auto it = std::begin(output); !done && it != std::end(output); ++it) {
    const details::rebase_instruction& inst = *it;

    switch (static_cast<REBASE_OPCODES>(inst.opcode)) {
      case REBASE_OPCODES::DONE:
        {
          done = true;
          break;
        }

      case REBASE_OPCODES::SET_TYPE_IMM:
        {
          raw_output.write<uint8_t>(uint8_t(REBASE_OPCODES::SET_TYPE_IMM) | inst.op1);
          break;
        }

      case REBASE_OPCODES::SET_SEGMENT_AND_OFFSET_ULEB:
        {
          raw_output
            .write<uint8_t>(uint8_t(REBASE_OPCODES::SET_SEGMENT_AND_OFFSET_ULEB) | inst.op1)
            .write_uleb128(inst.op2);

          break;
        }

      case REBASE_OPCODES::ADD_ADDR_ULEB:
        {
          raw_output
            .write<uint8_t>(uint8_t(REBASE_OPCODES::ADD_ADDR_ULEB))
            .write_uleb128(inst.op1);

          break;
        }

      case REBASE_OPCODES::ADD_ADDR_IMM_SCALED:
        {
          raw_output
            .write<uint8_t>(uint8_t(REBASE_OPCODES::ADD_ADDR_IMM_SCALED) | inst.op1);

          break;
        }

      case REBASE_OPCODES::DO_REBASE_IMM_TIMES:
        {
          raw_output
            .write<uint8_t>(uint8_t(REBASE_OPCODES::DO_REBASE_IMM_TIMES) | inst.op1);

          break;
        }

      case REBASE_OPCODES::DO_REBASE_ULEB_TIMES:
        {
          raw_output
            .write<uint8_t>(uint8_t(REBASE_OPCODES::DO_REBASE_ULEB_TIMES))
            .write_uleb128(inst.op1);

          break;
        }

      case REBASE_OPCODES::DO_REBASE_ADD_ADDR_ULEB:
        {
          raw_output
            .write<uint8_t>(uint8_t(REBASE_OPCODES::DO_REBASE_ADD_ADDR_ULEB))
            .write_uleb128(inst.op1);

          break;
        }

      case REBASE_OPCODES::DO_REBASE_ULEB_TIMES_SKIPPING_ULEB:
        {
          raw_output
            .write<uint8_t>(uint8_t(REBASE_OPCODES::DO_REBASE_ULEB_TIMES_SKIPPING_ULEB))
            .write_uleb128(inst.op1)
            .write_uleb128(inst.op2);

          break;
        }

      default:
        {
          LIEF_ERR("Unknown opcode: 0x{:x}", static_cast<uint32_t>(inst.opcode));
        }
    }

  }
  raw_output.align(pint_size);
  if (raw_output.size() > rebase_opcodes_.size()) {
    LIEF_INFO("New rebase opcodes are larger than the original ones: 0x{:06x} -> 0x{:06x}",
              rebase_opcodes_.size(), raw_output.size());
  }
  stream.write(std::move(raw_output.raw()));
  return *this;
}

DyldInfo& DyldInfo::update_binding_info(vector_iostream& stream, details::dyld_info_command& cmd) {
  auto cmp = [] (const DyldBindingInfo* lhs, const DyldBindingInfo* rhs) {
    if (lhs->library_ordinal() != rhs->library_ordinal()) {
      return lhs->library_ordinal() < rhs->library_ordinal();
    }

    if (lhs->has_symbol() && rhs->has_symbol()) {
      if (lhs->symbol()->name() != rhs->symbol()->name()) {
        return lhs->symbol()->name() < rhs->symbol()->name();
      }
    } else {
      LIEF_ERR("No symbol in LHS/RHS");
    }

    if (lhs->binding_type() != rhs->binding_type()) {
      return lhs->binding_type() < rhs->binding_type();
    }

    return lhs->address() < rhs->address();

  };

  auto cmp_weak_binding = [] (const DyldBindingInfo* lhs, const DyldBindingInfo* rhs) {
    if (lhs->has_symbol() && rhs->has_symbol()) {
      if (lhs->symbol()->name() != rhs->symbol()->name()) {
        return lhs->symbol()->name() < rhs->symbol()->name();
      }
    } else {
      LIEF_ERR("No symbol in LHS/RHS");
    }

    if (lhs->binding_type() != rhs->binding_type()) {
      return lhs->binding_type() < rhs->binding_type();
    }

    return lhs->address() < rhs->address();

  };

  auto cmp_lazy_binding = [] (const DyldBindingInfo* lhs, const DyldBindingInfo* rhs) {
    return lhs->address() < rhs->address();
  };


  DyldInfo::bind_container_t standard_binds(cmp);
  DyldInfo::bind_container_t weak_binds(cmp_weak_binding);
  DyldInfo::bind_container_t lazy_binds(cmp_lazy_binding);

  for (const std::unique_ptr<DyldBindingInfo>& binfo : binding_info_) {
    switch (binfo->binding_class()) {
      case DyldBindingInfo::CLASS::THREADED:
      case DyldBindingInfo::CLASS::STANDARD:
        {
          standard_binds.insert(binfo.get());
          break;
        }

      case DyldBindingInfo::CLASS::WEAK:
        {
          weak_binds.insert(binfo.get());
          break;
        }

      case DyldBindingInfo::CLASS::LAZY:
        {
          lazy_binds.insert(binfo.get());
          break;
        }
    }
  }

  if (!standard_binds.empty()) {
    cmd.bind_off = stream.size();
    {
      update_standard_bindings(standard_binds, stream);
    }
    cmd.bind_size = stream.size() - cmd.bind_off;

    // LIEF_DEBUG("LC_DYLD_INFO.bind_off : 0x{:06x} -> 0x{:06x}",
    //            this->bind().first, cmd.bind_off);
    // LIEF_DEBUG("LC_DYLD_INFO.bind_off : 0x{:06x} -> 0x{:06x}",
    //            this->bind().second, cmd.bind_size);
  }
  if (!weak_binds.empty()) {
    cmd.weak_bind_off = stream.size();
    {
      update_weak_bindings(weak_binds, stream);
    }
    cmd.weak_bind_size = stream.size() - cmd.weak_bind_off;
  }
  if (!lazy_binds.empty()) {
    cmd.lazy_bind_off = stream.size();
    {
      update_lazy_bindings(lazy_binds, stream);
    }
    cmd.lazy_bind_size = stream.size() - cmd.lazy_bind_off;
  }
  return *this;
}

bool operator==(uint8_t lhs, DyldInfo::BIND_OPCODES rhs) {
  return lhs == static_cast<uint8_t>(rhs);
}

bool operator!=(uint8_t lhs, DyldInfo::BIND_OPCODES rhs) {
  return lhs != static_cast<uint8_t>(rhs);
}

DyldInfo& DyldInfo::update_weak_bindings(const DyldInfo::bind_container_t& bindings, vector_iostream& stream) {
  std::vector<details::binding_instruction> instructions;

  uint64_t current_segment_start = 0;
  uint64_t current_segment_end = 0;
  uint32_t current_segment_index = 0;

  uint8_t type = 0;
  auto address = static_cast<uint64_t>(-1);
  std::string symbol_name;
  int64_t addend = 0;
  const size_t pint_size = binary_->pointer_size();


  for (DyldBindingInfo* info : bindings) {
    Symbol* sym = info->symbol();
    if (sym != nullptr) {
      if (sym->name() != symbol_name) {
        uint64_t flag = info->is_non_weak_definition() ? BIND_SYMBOL_FLAGS::NON_WEAK_DEFINITION : 0;
        instructions.emplace_back(uint8_t(BIND_OPCODES::SET_SYMBOL_TRAILING_FLAGS_IMM), flag, 0, sym->name());
        symbol_name = sym->name();
      }
    } else {
      LIEF_ERR("No symbol associated with the binding info");
    }

    if (info->binding_type() != DyldBindingInfo::TYPE(type)) {
      type = static_cast<uint8_t>(info->binding_type());
      instructions.emplace_back(static_cast<uint8_t>(BIND_OPCODES::SET_TYPE_IMM), type);
    }

    if (info->address() != address) {
      if (info->address() < current_segment_start || current_segment_end <= info->address()) {
        SegmentCommand* segment = info->segment();
        if (segment == nullptr) {
          LIEF_ERR("No segment associated the weak binding information. Can't update");
          return *this;
        }

        size_t index = segment->index();

        current_segment_start = segment->virtual_address();
        current_segment_end   = segment->virtual_address() + segment->virtual_size();
        current_segment_index = index;

        instructions.emplace_back(static_cast<uint8_t>(BIND_OPCODES::SET_SEGMENT_AND_OFFSET_ULEB),
            current_segment_index, info->address() - current_segment_start);

      } else {
        instructions.emplace_back(static_cast<uint8_t>(BIND_OPCODES::ADD_ADDR_ULEB), info->address() - address);
      }
      address = info->address();
    }

    if (addend != info->addend()) {
      instructions.emplace_back(static_cast<uint8_t>(BIND_OPCODES::SET_ADDEND_SLEB), info->addend());
      addend = info->addend();
    }

    instructions.emplace_back(static_cast<uint8_t>(BIND_OPCODES::DO_BIND), 0);
    address += binary_->pointer_size();
  }

  instructions.emplace_back(static_cast<uint8_t>(BIND_OPCODES::DONE), 0);


  // ===========================================
  // 1. First optimization
  // combine bind/add pairs
  // Based on ld64-274.2/src/ld/LinkEdit.hpp:469
  // ===========================================
  auto dst = std::begin(instructions);
  for (auto it = std::begin(instructions); it->opcode != BIND_OPCODES::DONE; ++it) {
    if (it->opcode == BIND_OPCODES::DO_BIND && it[1].opcode == BIND_OPCODES::ADD_ADDR_ULEB) {
      dst->opcode = static_cast<uint8_t>(BIND_OPCODES::DO_BIND_ADD_ADDR_ULEB);
      dst->op1 = it[1].op1;
      ++it;
      ++dst;
    } else {
      *dst++ = *it;
    }
  }
  dst->opcode = static_cast<uint8_t>(REBASE_OPCODES::DONE);

  // ===========================================
  // 2. Second optimization
  // Based on ld64-274.2/src/ld/LinkEdit.hpp:485
  // ===========================================
  dst = std::begin(instructions);
  for (auto it = std::begin(instructions); it->opcode != BIND_OPCODES::DONE; ++it) {
    uint64_t delta = it->op1;
    if (it->opcode == BIND_OPCODES::DO_BIND_ADD_ADDR_ULEB &&
        it[1].opcode == BIND_OPCODES::DO_BIND_ADD_ADDR_ULEB &&
        it[1].op1 == delta) {
      dst->opcode = static_cast<uint8_t>(BIND_OPCODES::DO_BIND_ULEB_TIMES_SKIPPING_ULEB);
      dst->op1 = 1;
      dst->op2 = delta;
      ++it;
      while (it->opcode == BIND_OPCODES::DO_BIND_ADD_ADDR_ULEB && it->op1 == delta) {
        dst->op1++;
        ++it;
      }
      --it;
      ++dst;
    } else {
      *dst++ = *it;
    }
  }
  dst->opcode = static_cast<uint8_t>(REBASE_OPCODES::DONE);


  // ===========================================
  // 3. Third optimization
  // Use immediate encodings
  // Based on ld64-274.2/src/ld/LinkEdit.hpp:512
  // ===========================================
  for (auto it = std::begin(instructions); it->opcode != BIND_OPCODES::DONE; ++it) {
    if (it->opcode == BIND_OPCODES::DO_BIND_ADD_ADDR_ULEB &&
        it->op1 < (15 * pint_size) &&
        (it->op1 % pint_size) == 0) {
      it->opcode = static_cast<uint8_t>(BIND_OPCODES::DO_BIND_ADD_ADDR_IMM_SCALED);
      it->op1 = it->op1 / pint_size;
    } else if (it->opcode == BIND_OPCODES::SET_DYLIB_ORDINAL_ULEB && it->op1 <= 15) {
      it->opcode = static_cast<uint8_t>(BIND_OPCODES::SET_DYLIB_ORDINAL_IMM);
    }
  }
  dst->opcode = static_cast<uint8_t>(REBASE_OPCODES::DONE);

  bool done = false;
  vector_iostream raw_output;
  for (auto it = std::begin(instructions); !done && it != std::end(instructions); ++it) {
    const details::binding_instruction& inst = *it;
    switch (static_cast<BIND_OPCODES>(inst.opcode)) {
      case BIND_OPCODES::DONE:
        {
          done = true;
          break;
        }

      case BIND_OPCODES::SET_DYLIB_ORDINAL_IMM:
        {
          raw_output
            .write<uint8_t>(uint8_t(BIND_OPCODES::SET_DYLIB_ORDINAL_IMM) | inst.op1);
          break;
        }

      case BIND_OPCODES::SET_DYLIB_ORDINAL_ULEB:
        {
          raw_output
            .write<uint8_t>(static_cast<uint8_t>(BIND_OPCODES::SET_DYLIB_ORDINAL_ULEB))
            .write_uleb128(inst.op1);
          break;
        }

      case BIND_OPCODES::SET_DYLIB_SPECIAL_IMM:
        {
          raw_output
            .write<uint8_t>(uint8_t(BIND_OPCODES::SET_DYLIB_SPECIAL_IMM) | (inst.op1 & IMMEDIATE_MASK));
          break;
        }

      case BIND_OPCODES::SET_SYMBOL_TRAILING_FLAGS_IMM:
        {
          raw_output
            .write<uint8_t>(uint8_t(BIND_OPCODES::SET_SYMBOL_TRAILING_FLAGS_IMM) | inst.op1)
            .write(inst.name);
          break;
        }

      case BIND_OPCODES::SET_TYPE_IMM:
        {
          raw_output
            .write<uint8_t>(uint8_t(BIND_OPCODES::SET_TYPE_IMM) | inst.op1);
          break;
        }

      case BIND_OPCODES::SET_ADDEND_SLEB:
        {
          raw_output
            .write<uint8_t>(uint8_t(BIND_OPCODES::SET_ADDEND_SLEB))
            .write_sleb128(inst.op1);
          break;
        }

      case BIND_OPCODES::SET_SEGMENT_AND_OFFSET_ULEB:
        {
          raw_output
            .write<uint8_t>(uint8_t(BIND_OPCODES::SET_SEGMENT_AND_OFFSET_ULEB) | inst.op1)
            .write_uleb128(inst.op2);
          break;
        }

      case BIND_OPCODES::ADD_ADDR_ULEB:
        {
          raw_output
            .write<uint8_t>(uint8_t(BIND_OPCODES::ADD_ADDR_ULEB))
            .write_uleb128(inst.op1);
          break;
        }

      case BIND_OPCODES::DO_BIND:
        {
          raw_output
            .write<uint8_t>(uint8_t(BIND_OPCODES::DO_BIND));
          break;
        }

      case BIND_OPCODES::DO_BIND_ADD_ADDR_ULEB:
        {
          raw_output
              .write<uint8_t>(uint8_t(BIND_OPCODES::DO_BIND_ADD_ADDR_ULEB))
            .write_uleb128(inst.op1);
          break;
        }

      case BIND_OPCODES::DO_BIND_ADD_ADDR_IMM_SCALED:
        {
          raw_output
            .write<uint8_t>(uint8_t(BIND_OPCODES::DO_BIND_ADD_ADDR_IMM_SCALED) | inst.op1);
          break;
        }

      case BIND_OPCODES::DO_BIND_ULEB_TIMES_SKIPPING_ULEB:
        {
          raw_output
            .write<uint8_t>(uint8_t(BIND_OPCODES::DO_BIND_ULEB_TIMES_SKIPPING_ULEB))
            .write_uleb128(inst.op1)
            .write_uleb128(inst.op2);
          break;
        }

      default:
        {
          LIEF_WARN("Opcode {} ({:d}) is not processed for weak bindings",
              to_string(static_cast<BIND_OPCODES>(inst.opcode)), inst.opcode);
          break;
        }
    }
  }
  raw_output.align(pint_size);
  if (raw_output.size() > weak_bind_opcodes_.size()) {
    LIEF_INFO("New WEAK bind opcodes are larger than the original ones: 0x{:06x} -> 0x{:06x}",
              weak_bind_opcodes_.size(), raw_output.size());
  }
  stream.write(std::move(raw_output.raw()));
  return *this;
}

DyldInfo& DyldInfo::update_lazy_bindings(const DyldInfo::bind_container_t& bindings, vector_iostream& stream) {

  vector_iostream raw_output;
  for (DyldBindingInfo* info : bindings) {
    SegmentCommand* segment = info->segment();
    if (segment == nullptr) {
      LIEF_ERR("No segment associated with the lazy binding info. Can't update");
      return *this;
    }
    size_t index = segment->index();

    uint64_t current_segment_start = segment->virtual_address();
    uint32_t current_segment_index = index;

    raw_output
      .write<uint8_t>(uint8_t(BIND_OPCODES::SET_SEGMENT_AND_OFFSET_ULEB) | current_segment_index)
      .write_uleb128(info->address() - current_segment_start);

    if (info->library_ordinal() <= 0) {
      raw_output.write<uint8_t>(
        uint8_t(BIND_OPCODES::SET_DYLIB_SPECIAL_IMM) | (info->library_ordinal() & IMMEDIATE_MASK)
      );
    } else if (info->library_ordinal() <= 15) {
      raw_output.write<uint8_t>(
        uint8_t(BIND_OPCODES::SET_DYLIB_ORDINAL_IMM) | info->library_ordinal()
      );
    } else {
      raw_output
        .write<uint8_t>(uint8_t(BIND_OPCODES::SET_DYLIB_ORDINAL_ULEB))
        .write_uleb128(info->library_ordinal());
    }

    uint64_t flags = info->is_weak_import() ? BIND_SYMBOL_FLAGS::WEAK_IMPORT : 0;
    flags |= info->is_non_weak_definition() ? BIND_SYMBOL_FLAGS::NON_WEAK_DEFINITION : 0;
    if (!info->has_symbol()) {
      LIEF_ERR("Missing symbol. Can't update");
      return *this;
    }
    raw_output
      .write<uint8_t>(uint8_t(BIND_OPCODES::SET_SYMBOL_TRAILING_FLAGS_IMM) | flags)
      .write(info->symbol()->name());

    raw_output
      .write<uint8_t>(static_cast<uint8_t>(BIND_OPCODES::DO_BIND))
      .write<uint8_t>(static_cast<uint8_t>(BIND_OPCODES::DONE));
  }

  raw_output.align(binary_->pointer_size());

  LIEF_DEBUG("size: 0x{:x} vs 0x{:x}", raw_output.size(), lazy_bind_opcodes_.size());

  if (raw_output.size() > lazy_bind_opcodes_.size()) {
    LIEF_INFO("New LAZY bind opcodes are larger than the original ones: 0x{:06x} -> 0x{:06x}",
              lazy_bind_opcodes_.size(), raw_output.size());
  }
  stream.write(std::move(raw_output.raw()));
  return *this;
}

DyldInfo& DyldInfo::update_standard_bindings(const DyldInfo::bind_container_t& bindings, vector_iostream& stream) {
  switch (binding_encoding_version_) {
    case BINDING_ENCODING_VERSION::V1:
      {
        update_standard_bindings_v1(bindings, stream);
        break;
      }

    case BINDING_ENCODING_VERSION::V2:
      {
        std::vector<RelocationDyld*> rebases;
        Binary::relocations_t relocations = binary_->relocations_list();
        rebases.reserve(relocations.size());
        for (Relocation* r : relocations) {
          if (r->origin() == Relocation::ORIGIN::DYLDINFO) {
            rebases.push_back(r->as<RelocationDyld>());
          }
        }
        LIEF_DEBUG("Bindings V2: #{} relocations", rebases.size());
        update_standard_bindings_v2(bindings, std::move(rebases), stream);
        break;
      }

    case BINDING_ENCODING_VERSION::UNKNOWN:
    default:
      {
        LIEF_WARN("Unsupported version");
        break;
      }
  }
  return *this;
}



DyldInfo& DyldInfo::update_standard_bindings_v1(const DyldInfo::bind_container_t& bindings, vector_iostream& stream) {
  // This function updates the standard bindings opcodes (i.e. not lazy and not weak)
  // The following code is mainly inspired from LinkEdit.hpp: BindingInfoAtom<A>::encodeV1()

  std::vector<details::binding_instruction> instructions;

  uint64_t current_segment_start = 0;
  uint64_t current_segment_end = 0;
  uint32_t current_segment_index = 0;
  uint8_t type = 0;
  auto address = static_cast<uint64_t>(-1);
  int32_t ordinal = 0x80000000;
  std::string symbol_name;
  int64_t addend = 0;
  const size_t pint_size = binary_->pointer_size();

  for (DyldBindingInfo* info : bindings) {
    if (info->library_ordinal() != ordinal) {
      if (info->library_ordinal() <= 0) {
        instructions.emplace_back(uint8_t(BIND_OPCODES::SET_DYLIB_SPECIAL_IMM), info->library_ordinal());
      } else {
        instructions.emplace_back(uint8_t(BIND_OPCODES::SET_DYLIB_ORDINAL_ULEB), info->library_ordinal());
      }
      ordinal = info->library_ordinal();
    }

    if (!info->has_symbol()) {
      LIEF_ERR("Missing symbol for updating v1 binding.");
      return *this;
    }
    if (info->symbol()->name() != symbol_name) {
      uint64_t flag = info->is_weak_import() ? BIND_SYMBOL_FLAGS::WEAK_IMPORT : 0;
      symbol_name = info->symbol()->name();
      instructions.emplace_back(uint8_t(BIND_OPCODES::SET_SYMBOL_TRAILING_FLAGS_IMM), flag, 0, symbol_name);
    }

    if (info->binding_type() != DyldBindingInfo::TYPE(type)) {
      type = static_cast<uint8_t>(info->binding_type());
      instructions.emplace_back(uint8_t(BIND_OPCODES::SET_TYPE_IMM), type);
    }

    if (info->address() != address) {
      if (info->address() < current_segment_start || info->address() >= current_segment_end) {
        SegmentCommand* segment = info->segment();
        if (segment == nullptr) {
          LIEF_ERR("Can't find the segment. Can't update binding v1");
          return *this;
        }
        size_t index = segment->index();

        current_segment_start = segment->virtual_address();
        current_segment_end   = segment->virtual_address() + segment->virtual_size();
        current_segment_index = index;

        instructions.emplace_back(uint8_t(BIND_OPCODES::SET_SEGMENT_AND_OFFSET_ULEB),
            current_segment_index, info->address() - current_segment_start);

      } else {
        instructions.emplace_back(uint8_t(BIND_OPCODES::ADD_ADDR_ULEB), info->address() - address);
      }
      address = info->address();
    }

    if (addend != info->addend()) {
      instructions.emplace_back(uint8_t(BIND_OPCODES::SET_ADDEND_SLEB), info->addend());
      addend = info->addend();
    }

    instructions.emplace_back(uint8_t(BIND_OPCODES::DO_BIND), 0);
    address += binary_->pointer_size();
  }

  instructions.emplace_back(uint8_t(BIND_OPCODES::DONE), 0);


  // ===========================================
  // 1. First optimization
  // combine bind/add pairs
  // Based on ld64-274.2/src/ld/LinkEdit.hpp:469
  // ===========================================
  auto dst = std::begin(instructions);
  for (auto it = std::begin(instructions); it->opcode != BIND_OPCODES::DONE; ++it) {
    if (it->opcode == BIND_OPCODES::DO_BIND && it[1].opcode == BIND_OPCODES::ADD_ADDR_ULEB) {
      dst->opcode = static_cast<uint8_t>(BIND_OPCODES::DO_BIND_ADD_ADDR_ULEB);
      dst->op1 = it[1].op1;
      ++it;
      ++dst;
    } else {
      *dst++ = *it;
    }
  }
  dst->opcode = static_cast<uint8_t>(REBASE_OPCODES::DONE);

  // ===========================================
  // 2. Second optimization
  // Based on ld64-274.2/src/ld/LinkEdit.hpp:485
  // ===========================================
  dst = std::begin(instructions);
  for (auto it = std::begin(instructions); it->opcode != BIND_OPCODES::DONE; ++it) {
    uint64_t delta = it->op1;
    if (it->opcode == BIND_OPCODES::DO_BIND_ADD_ADDR_ULEB &&
        it[1].opcode == BIND_OPCODES::DO_BIND_ADD_ADDR_ULEB &&
        it[1].op1 == delta) {
      dst->opcode = static_cast<uint8_t>(BIND_OPCODES::DO_BIND_ULEB_TIMES_SKIPPING_ULEB);
      dst->op1 = 1;
      dst->op2 = delta;
      ++it;
      while (it->opcode == BIND_OPCODES::DO_BIND_ADD_ADDR_ULEB && it->op1 == delta) {
        dst->op1++;
        ++it;
      }
      --it;
      ++dst;
    } else {
      *dst++ = *it;
    }
  }
  dst->opcode = static_cast<uint8_t>(REBASE_OPCODES::DONE);


  // ===========================================
  // 3. Third optimization
  // Use immediate encodings
  // Based on ld64-274.2/src/ld/LinkEdit.hpp:512
  // ===========================================
  for (auto it = std::begin(instructions); it->opcode != BIND_OPCODES::DONE; ++it) {
    if (it->opcode == BIND_OPCODES::DO_BIND_ADD_ADDR_ULEB &&
        it->op1 < (15 * pint_size) &&
        (it->op1 % pint_size) == 0) {
      it->opcode = static_cast<uint8_t>(BIND_OPCODES::DO_BIND_ADD_ADDR_IMM_SCALED);
      it->op1 = it->op1 / pint_size;
    } else if (it->opcode == BIND_OPCODES::SET_DYLIB_ORDINAL_ULEB && it->op1 <= 15) {
      it->opcode = static_cast<uint8_t>(BIND_OPCODES::SET_DYLIB_ORDINAL_IMM);
    }
  }
  dst->opcode = static_cast<uint8_t>(REBASE_OPCODES::DONE);

  bool done = false;
  vector_iostream raw_output;
  for (auto it = std::begin(instructions); !done && it != std::end(instructions); ++it) {
    const details::binding_instruction& inst = *it;
    switch (static_cast<BIND_OPCODES>(inst.opcode)) {
      case BIND_OPCODES::DONE:
        {
          done = true;
          break;
        }

      case BIND_OPCODES::SET_DYLIB_ORDINAL_IMM:
        {
          raw_output
            .write<uint8_t>(uint8_t(BIND_OPCODES::SET_DYLIB_ORDINAL_IMM) | inst.op1);
          break;
        }

      case BIND_OPCODES::SET_DYLIB_ORDINAL_ULEB:
        {
          raw_output
            .write<uint8_t>(uint8_t(BIND_OPCODES::SET_DYLIB_ORDINAL_ULEB))
            .write_uleb128(inst.op1);
          break;
        }

      case BIND_OPCODES::SET_DYLIB_SPECIAL_IMM:
        {
          raw_output
            .write<uint8_t>(uint8_t(BIND_OPCODES::SET_DYLIB_SPECIAL_IMM) | (inst.op1 & IMMEDIATE_MASK));
          break;
        }

      case BIND_OPCODES::SET_SYMBOL_TRAILING_FLAGS_IMM:
        {
          raw_output
            .write<uint8_t>(uint8_t(BIND_OPCODES::SET_SYMBOL_TRAILING_FLAGS_IMM) | inst.op1)
            .write(inst.name);
          break;
        }

      case BIND_OPCODES::SET_TYPE_IMM:
        {
          raw_output
            .write<uint8_t>(uint8_t(BIND_OPCODES::SET_TYPE_IMM) | inst.op1);
          break;
        }

      case BIND_OPCODES::SET_ADDEND_SLEB:
        {
          raw_output
            .write<uint8_t>(uint8_t(BIND_OPCODES::SET_ADDEND_SLEB))
            .write_sleb128(inst.op1);
          break;
        }

      case BIND_OPCODES::SET_SEGMENT_AND_OFFSET_ULEB:
        {
          raw_output
            .write<uint8_t>(uint8_t(BIND_OPCODES::SET_SEGMENT_AND_OFFSET_ULEB) | inst.op1)
            .write_uleb128(inst.op2);
          break;
        }

      case BIND_OPCODES::ADD_ADDR_ULEB:
        {
          raw_output
            .write<uint8_t>(uint8_t(BIND_OPCODES::ADD_ADDR_ULEB))
            .write_uleb128(inst.op1);
          break;
        }

      case BIND_OPCODES::DO_BIND:
        {
          raw_output
            .write<uint8_t>(uint8_t(BIND_OPCODES::DO_BIND));
          break;
        }

      case BIND_OPCODES::DO_BIND_ADD_ADDR_ULEB:
        {
          raw_output
            .write<uint8_t>(uint8_t(BIND_OPCODES::DO_BIND_ADD_ADDR_ULEB))
            .write_uleb128(inst.op1);
          break;
        }

      case BIND_OPCODES::DO_BIND_ADD_ADDR_IMM_SCALED:
        {
          raw_output
            .write<uint8_t>(uint8_t(BIND_OPCODES::DO_BIND_ADD_ADDR_IMM_SCALED) | inst.op1);
          break;
        }

      case BIND_OPCODES::DO_BIND_ULEB_TIMES_SKIPPING_ULEB:
        {
          raw_output
            .write<uint8_t>(uint8_t(BIND_OPCODES::DO_BIND_ULEB_TIMES_SKIPPING_ULEB))
            .write_uleb128(inst.op1)
            .write_uleb128(inst.op2);
          break;
        }

      default:
        {
          LIEF_WARN("Opcode {} ({:d}) is not processed for weak bindings",
              to_string(static_cast<BIND_OPCODES>(inst.opcode)), inst.opcode);
          break;
        }
    }
  }
  raw_output.align(pint_size);
  if (raw_output.size() > bind_opcodes_.size()) {
    LIEF_INFO("New REGULAR bind opcodes are larger than the original ones: 0x{:06x} -> 0x{:06x}",
              bind_opcodes_.size(), raw_output.size());
  }
  stream.write(std::move(raw_output.raw()));
  return *this;
}


DyldInfo& DyldInfo::update_standard_bindings_v2(const DyldInfo::bind_container_t& bindings_set,
                                                std::vector<RelocationDyld*> rebases, vector_iostream& stream) {
  // v2 encoding as defined in Linkedit.hpp - BindingInfoAtom<A>::encodeV2()
  // This encoding uses THREADED opcodes.
  std::vector<DyldBindingInfo*> bindings = {std::begin(bindings_set), std::end(bindings_set)};

  std::vector<details::binding_instruction> instructions;
  uint64_t current_segment_start = 0;
  uint64_t current_segment_end   = 0;
  uint64_t current_segment_index = 0;
  uint8_t type = 0;
  auto address = static_cast<uint64_t>(-1);
  int32_t ordinal = 0x80000000;
  std::string symbol_name;
  int64_t addend = 0;
  auto num_bindings = static_cast<uint64_t>(-1);
  const size_t pint_size = binary_->pointer_size();

  for (DyldBindingInfo* info : bindings) {
    bool made_changes = false;
    const int32_t lib_ordinal = info->library_ordinal();
    if (ordinal != lib_ordinal) {
      if (lib_ordinal <= 0) {
        instructions.emplace_back(uint8_t(BIND_OPCODES::SET_DYLIB_SPECIAL_IMM), lib_ordinal);
      }
      else if (lib_ordinal <= 15) {
        instructions.emplace_back(uint8_t(BIND_OPCODES::SET_DYLIB_ORDINAL_IMM), lib_ordinal);
      }
      else {
        instructions.emplace_back(uint8_t(BIND_OPCODES::SET_DYLIB_ORDINAL_ULEB), lib_ordinal);
      }
      ordinal = lib_ordinal;
      made_changes = true;
    }
    if (!info->has_symbol()) {
      LIEF_ERR("Missing symbol for updating bindings v2");
      return *this;
    }
    if (symbol_name != info->symbol()->name()) {
      uint64_t flag = info->is_weak_import() ? BIND_SYMBOL_FLAGS::WEAK_IMPORT : 0;
      symbol_name = info->symbol()->name();
      instructions.emplace_back(uint8_t(BIND_OPCODES::SET_SYMBOL_TRAILING_FLAGS_IMM), flag, 0, symbol_name);
      made_changes = true;
    }

    if (info->binding_type() != DyldBindingInfo::TYPE(type)) {
      if (info->binding_type() != DyldBindingInfo::TYPE::POINTER) {
        LIEF_ERR("Unsupported bind type with linked list opcodes");
        return *this;
      }
      type = static_cast<uint8_t>(info->binding_type());
      instructions.emplace_back(uint8_t(BIND_OPCODES::SET_TYPE_IMM), type);
      made_changes = true;
    }

    if (address != info->address()) {
      address = info->address();
      SegmentCommand* segment = info->segment();
      if (segment == nullptr) {
        LIEF_ERR("Can't find the segment associated with the binding info. Can't udpate binding v2");
        return *this;
      }
      size_t index = segment->index();
      current_segment_start = segment->virtual_address();
      current_segment_end   = segment->virtual_address() + segment->virtual_size();
      current_segment_index = index;
      made_changes = true;
    }

    if (addend != info->addend()) {
      addend = info->addend();
      instructions.emplace_back(uint8_t(BIND_OPCODES::SET_ADDEND_SLEB), addend);
      made_changes = true;
    }

    if (made_changes) {
      ++num_bindings;
      instructions.emplace_back(uint8_t(BIND_OPCODES::DO_BIND), 0);
    }
  }

  if (num_bindings > std::numeric_limits<uint16_t>::max() && num_bindings != static_cast<uint64_t>(-1)) {
    LIEF_ERR("Too many binds ({:d}). The limit being 65536");
    return *this;
  }

  std::vector<uint64_t> threaded_rebase_bind_indices;
  threaded_rebase_bind_indices.reserve(bindings.size() + rebases.size());

  for (int64_t i = 0, e = rebases.size(); i != e; ++i) {
    threaded_rebase_bind_indices.push_back(-i);
  }

  for (int64_t i = 0, e = bindings.size(); i != e; ++i) {
    threaded_rebase_bind_indices.push_back(i + 1);
  }

  std::sort(std::begin(threaded_rebase_bind_indices), std::end(threaded_rebase_bind_indices),
      [&bindings, &rebases] (int64_t index_a, int64_t index_b) {
        if (index_a == index_b) {
          return false;
        }
        uint64_t address_a = index_a <= 0 ? rebases[-index_a]->address() : bindings[index_a - 1]->address();
        uint64_t address_b = index_b <= 0 ? rebases[-index_b]->address() : bindings[index_b - 1]->address();
        return address_a < address_b;
      });

  current_segment_start = 0;
  current_segment_end   = 0;
  current_segment_index = 0;

  uint64_t prev_page_index = 0;

  for (int64_t entry_index : threaded_rebase_bind_indices) {
    RelocationDyld* rebase = nullptr;
    DyldBindingInfo* bind = nullptr;

    uint64_t address = 0;
    SegmentCommand* segment = nullptr;
    if (entry_index <= 0) {
      rebase = rebases[-entry_index];
      address = rebase->address();
      segment = rebase->segment();
    } else {
      bind = bindings[entry_index - 1];
      address = bind->address();
      segment = bind->segment();
    }

    if (segment == nullptr) {
      LIEF_ERR("Missing segment while update binding v2");
      return *this;
    }
    if (address % 8 != 0) {
      LIEF_WARN("Address not aligned!");
    }

    bool new_segment = false;
    if (address < current_segment_start || address >= current_segment_end) {
      size_t index = segment->index();
      current_segment_start = segment->virtual_address();
      current_segment_end   = segment->virtual_address() + segment->virtual_size();
      current_segment_index = index;
      new_segment = true;
    }
    uint64_t page_index = (address - current_segment_start) / 4096;
    if (new_segment || page_index != prev_page_index) {
      instructions.emplace_back(uint8_t(BIND_OPCODES::SET_SEGMENT_AND_OFFSET_ULEB),
                                current_segment_index, address - current_segment_start);
      instructions.emplace_back(uint8_t(BIND_OPCODES::THREADED) |
                                uint8_t(BIND_SUBOPCODE_THREADED::APPLY),
                                0);
    }
    prev_page_index = page_index;
  }
  instructions.emplace_back(static_cast<uint8_t>(BIND_OPCODES::DONE), 0);
  vector_iostream raw_output;
  raw_output.write<uint8_t>(static_cast<uint8_t>(BIND_OPCODES::THREADED) |
                            static_cast<uint8_t>(BIND_SUBOPCODE_THREADED::SET_BIND_ORDINAL_TABLE_SIZE_ULEB))
            .write_uleb128(num_bindings + 1);

  bool done = false;
  for (auto it = std::begin(instructions); !done && it != std::end(instructions); ++it) {
    const details::binding_instruction& inst = *it;
    switch(static_cast<BIND_OPCODES>(it->opcode)) {
      case BIND_OPCODES::DONE:
        {
          done = true;
          break;
        }

      case BIND_OPCODES::SET_DYLIB_ORDINAL_IMM:
        {
          raw_output
            .write<uint8_t>(uint8_t(BIND_OPCODES::SET_DYLIB_ORDINAL_IMM) | inst.op1);
          break;
        }

      case BIND_OPCODES::SET_DYLIB_ORDINAL_ULEB:
        {
          raw_output
            .write<uint8_t>(uint8_t(BIND_OPCODES::SET_DYLIB_ORDINAL_ULEB))
            .write_uleb128(inst.op1);
          break;
        }

      case BIND_OPCODES::SET_DYLIB_SPECIAL_IMM:
        {
          raw_output
            .write<uint8_t>(uint8_t(BIND_OPCODES::SET_DYLIB_SPECIAL_IMM) | (inst.op1 & IMMEDIATE_MASK));
          break;
        }

      case BIND_OPCODES::SET_SYMBOL_TRAILING_FLAGS_IMM:
        {
          raw_output
            .write<uint8_t>(uint8_t(BIND_OPCODES::SET_SYMBOL_TRAILING_FLAGS_IMM) | inst.op1)
            .write(inst.name);
          break;
        }

      case BIND_OPCODES::SET_TYPE_IMM:
        {
          raw_output
            .write<uint8_t>(uint8_t(BIND_OPCODES::SET_TYPE_IMM) | inst.op1);
          break;
        }

      case BIND_OPCODES::SET_ADDEND_SLEB:
        {
          raw_output
            .write<uint8_t>(uint8_t(BIND_OPCODES::SET_ADDEND_SLEB))
            .write_sleb128(inst.op1);
          break;
        }

      case BIND_OPCODES::SET_SEGMENT_AND_OFFSET_ULEB:
        {
          raw_output
            .write<uint8_t>(uint8_t(BIND_OPCODES::SET_SEGMENT_AND_OFFSET_ULEB) | inst.op1)
            .write_uleb128(inst.op2);
          break;
        }

      case BIND_OPCODES::ADD_ADDR_ULEB:
        {
          raw_output
            .write<uint8_t>(static_cast<uint8_t>(BIND_OPCODES::ADD_ADDR_ULEB))
            .write_uleb128(inst.op1);
          break;
        }

      case BIND_OPCODES::DO_BIND:
        {
          raw_output
            .write<uint8_t>(static_cast<uint8_t>(BIND_OPCODES::DO_BIND));
          break;
        }

      case BIND_OPCODES::DO_BIND_ADD_ADDR_ULEB:
        {
          raw_output
            .write<uint8_t>(static_cast<uint8_t>(BIND_OPCODES::DO_BIND_ADD_ADDR_ULEB))
            .write_uleb128(inst.op1);
          break;
        }

      case BIND_OPCODES::DO_BIND_ADD_ADDR_IMM_SCALED:
        {
          raw_output
            .write<uint8_t>(uint8_t(BIND_OPCODES::DO_BIND_ADD_ADDR_IMM_SCALED) | inst.op1);
          break;
        }

      case BIND_OPCODES::DO_BIND_ULEB_TIMES_SKIPPING_ULEB:
        {
          raw_output
            .write<uint8_t>(uint8_t(BIND_OPCODES::DO_BIND_ULEB_TIMES_SKIPPING_ULEB))
            .write_uleb128(inst.op1)
            .write_uleb128(inst.op2);
          break;
        }

      case BIND_OPCODES::THREADED_SET_BIND_ORDINAL_TABLE_SIZE_ULEB:
        {
          raw_output
            .write<uint8_t>(static_cast<uint8_t>(BIND_OPCODES::THREADED_SET_BIND_ORDINAL_TABLE_SIZE_ULEB))
            .write_uleb128(inst.op1);
          break;
        }

      case BIND_OPCODES::THREADED_APPLY:
        {
          raw_output
            .write(static_cast<uint8_t>(BIND_OPCODES::THREADED_APPLY));
          break;
        }

      default:
        {
          LIEF_ERR("Unsupported opcode");
          done = true;
          break;
        }
    }
  }

  raw_output.write<uint8_t>(static_cast<uint8_t>(BIND_OPCODES::DONE));
  raw_output.align(pint_size);

  if (raw_output.size() > bind_opcodes_.size()) {
    LIEF_INFO("New REGULAR V2 bind opcodes are larger than the original ones: 0x{:06x} -> 0x{:06x}",
              bind_opcodes_.size(), raw_output.size());
  }
  stream.write(std::move(raw_output.raw()));
  return *this;
}


DyldInfo& DyldInfo::update_export_trie(vector_iostream& stream) {
  std::vector<uint8_t> raw_output = create_trie(export_info_, binary_->pointer_size());
  if (raw_output.size() > export_trie_.size()) {
    LIEF_INFO("New EXPORTS TRIE is larger than the original one: 0x{:06x} -> 0x{:06x}",
              export_trie_.size(), raw_output.size());
  }
  stream.write(std::move(raw_output));
  return *this;
}


void DyldInfo::add(std::unique_ptr<ExportInfo> info) {
  export_info_.push_back(std::move(info));
}

std::ostream& DyldInfo::print(std::ostream& os) const {
  LoadCommand::print(os) << '\n';

  os << fmt::format("{:11}: {:10} {:10}", "Type", "Offset", "Size") << '\n'
     << fmt::format("{:11}: {:10} {:10}", "Rebase", std::get<0>(rebase()), std::get<1>(rebase())) << '\n'
     << fmt::format("{:11}: {:10} {:10}", "Bind", std::get<0>(bind()), std::get<1>(bind())) << '\n'
     << fmt::format("{:11}: {:10} {:10}", "Weak bind", std::get<0>(weak_bind()), std::get<1>(weak_bind())) << '\n'
     << fmt::format("{:11}: {:10} {:10}", "Lazy bind", std::get<0>(lazy_bind()), std::get<1>(lazy_bind())) << '\n'
     << fmt::format("{:11}: {:10} {:10}", "Export", std::get<0>(export_info()), std::get<1>(export_info())) << '\n';

  it_const_binding_info bindings = this->bindings();
  if (!bindings.empty()) {
    os << fmt::format("Binding Info (#{})", bindings.size()) << '\n';
    for (const BindingInfo& info : bindings) {
      os << info << '\n';
    }
  }

  it_const_export_info exports = this->exports();
  if (!exports.empty()) {
    os << fmt::format("Export Info (#{})", exports.size()) << '\n';
    for (const ExportInfo& info : exports) {
      os << info << '\n';
    }
  }

  return os;
}


const char* to_string(DyldInfo::REBASE_TYPE e) {
  #define ENTRY(X) std::pair(DyldInfo::REBASE_TYPE::X, #X)
  STRING_MAP enums2str {
    ENTRY(POINTER),
    ENTRY(TEXT_ABSOLUTE32),
    ENTRY(TEXT_PCREL32),
    ENTRY(THREADED),
  };
  #undef ENTRY
  if (auto it = enums2str.find(e); it != enums2str.end()) {
    return it->second;
  }
  return "UNKNOWN";
}

const char* to_string(DyldInfo::REBASE_OPCODES e) {
  #define ENTRY(X) std::pair(DyldInfo::REBASE_OPCODES::X, #X)
  STRING_MAP enums2str {
    ENTRY(DONE),
    ENTRY(SET_TYPE_IMM),
    ENTRY(SET_SEGMENT_AND_OFFSET_ULEB),
    ENTRY(ADD_ADDR_ULEB),
    ENTRY(ADD_ADDR_IMM_SCALED),
    ENTRY(DO_REBASE_IMM_TIMES),
    ENTRY(DO_REBASE_ULEB_TIMES),
    ENTRY(DO_REBASE_ADD_ADDR_ULEB),
    ENTRY(DO_REBASE_ULEB_TIMES_SKIPPING_ULEB),
  };
  #undef ENTRY
  if (auto it = enums2str.find(e); it != enums2str.end()) {
    return it->second;
  }
  return "UNKNOWN";
}

const char* to_string(DyldInfo::BIND_OPCODES e) {
  #define ENTRY(X) std::pair(DyldInfo::BIND_OPCODES::X, #X)
  STRING_MAP enums2str {
    ENTRY(DONE),
    ENTRY(SET_DYLIB_ORDINAL_IMM),
    ENTRY(SET_DYLIB_ORDINAL_ULEB),
    ENTRY(SET_DYLIB_SPECIAL_IMM),
    ENTRY(SET_SYMBOL_TRAILING_FLAGS_IMM),
    ENTRY(SET_TYPE_IMM),
    ENTRY(SET_ADDEND_SLEB),
    ENTRY(SET_SEGMENT_AND_OFFSET_ULEB),
    ENTRY(ADD_ADDR_ULEB),
    ENTRY(DO_BIND),
    ENTRY(DO_BIND_ADD_ADDR_ULEB),
    ENTRY(DO_BIND_ADD_ADDR_IMM_SCALED),
    ENTRY(DO_BIND_ULEB_TIMES_SKIPPING_ULEB),
    ENTRY(THREADED),
    ENTRY(THREADED_APPLY),
    ENTRY(THREADED_SET_BIND_ORDINAL_TABLE_SIZE_ULEB),
  };
  #undef ENTRY
  if (auto it = enums2str.find(e); it != enums2str.end()) {
    return it->second;
  }
  return "UNKNOWN";
}

const char* to_string(DyldInfo::BIND_SUBOPCODE_THREADED e) {
  #define ENTRY(X) std::pair(DyldInfo::BIND_SUBOPCODE_THREADED::X, #X)
  STRING_MAP enums2str {
    ENTRY(SET_BIND_ORDINAL_TABLE_SIZE_ULEB),
    ENTRY(APPLY),
  };
  #undef ENTRY
  if (auto it = enums2str.find(e); it != enums2str.end()) {
    return it->second;
  }
  return "UNKNOWN";
}


}
}
