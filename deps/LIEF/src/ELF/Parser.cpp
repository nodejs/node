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
#include <memory>
#include <iterator>
#include <algorithm>

#include "logging.hpp"

#include "LIEF/BinaryStream/VectorStream.hpp"

#include "LIEF/ELF/utils.hpp"
#include "LIEF/ELF/Parser.hpp"
#include "LIEF/ELF/Binary.hpp"
#include "LIEF/ELF/SymbolVersion.hpp"
#include "LIEF/ELF/Segment.hpp"
#include "LIEF/ELF/Section.hpp"
#include "LIEF/ELF/Symbol.hpp"
#include "LIEF/ELF/Note.hpp"
#include "LIEF/ELF/SysvHash.hpp"

#include "ELF/DataHandler/Handler.hpp"

#include "Parser.tcc"

namespace LIEF {
namespace ELF {

struct Target {
  Header::CLASS clazz = Header::CLASS::NONE;
  ARCH arch = ARCH::NONE;
};

Parser::Parser() = default;
Parser::~Parser() = default;

Parser::Parser(const std::vector<uint8_t>& data, ParserConfig conf) :
  stream_{std::make_unique<VectorStream>(data)},
  binary_{new Binary{}},
  config_{std::move(conf)}
{}

Parser::Parser(std::unique_ptr<BinaryStream> stream, ParserConfig conf) :
  stream_{std::move(stream)},
  binary_{new Binary{}},
  config_{std::move(conf)}
{}

Parser::Parser(const std::string& file, ParserConfig conf) :
  binary_{new Binary{}},
  config_{std::move(conf)}
{
  if (auto s = VectorStream::from_file(file)) {
    stream_ = std::make_unique<VectorStream>(std::move(*s));
  }
}

Header::ELF_DATA determine_elf_endianess(ARCH machine) {
  switch (machine) {
    /* Architectures that are known to be big-endian only */
    case ARCH::H8_300:
    case ARCH::SPARC:
    case ARCH::SPARCV9:
    case ARCH::S390:
    case ARCH::M68K:
    case ARCH::OPENRISC:
      return Header::ELF_DATA::MSB;

    /* Architectures that are known to be little-endian only */
    case ARCH::HEXAGON:
    case ARCH::ALPHA:
    case ARCH::ALTERA_NIOS2:
    case ARCH::CRIS:
    case ARCH::I386: // x86
    case ARCH::X86_64:
    case ARCH::LOONGARCH:
      return Header::ELF_DATA::LSB;

    default:
      return Header::ELF_DATA::NONE;
  }
}

/*
 * Get the endianess of the current architecture
 */
constexpr Header::ELF_DATA get_endianess() {
  #ifdef __BYTE_ORDER__
    #if defined(__ORDER_LITTLE_ENDIAN__) && (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
      return Header::ELF_DATA::LSB;
    #elif defined(__ORDER_BIG_ENDIAN__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
      return Header::ELF_DATA::MSB;
    #endif
  #endif
  /* If there are no __BYTE_ORDER__ we take the (arbitrary) decision that we are
   * on a little endian architecture.
   */
  return Header::ELF_DATA::LSB;
}

constexpr Header::ELF_DATA invert_endianess(Header::ELF_DATA endian) {
  if (endian == Header::ELF_DATA::MSB) {
    return Header::ELF_DATA::LSB;
  }
  if (endian == Header::ELF_DATA::LSB) {
    return Header::ELF_DATA::MSB;
  }
  return Header::ELF_DATA::NONE;
}

Header::ELF_DATA determine_elf_endianess(BinaryStream& stream) {
  static constexpr auto BOTH_ENDIANESS = {
    ARCH::AARCH64, ARCH::ARM,  ARCH::SH,  ARCH::XTENSA,
    ARCH::ARC,     ARCH::MIPS, ARCH::PPC, ARCH::PPC64,
    ARCH::IA_64,
  };
  Header::ELF_DATA from_ei_data   = Header::ELF_DATA::NONE;
  /* ELF_DATA from_e_machine = ELF_DATA::ELFDATANONE; */

  // First, check EI_CLASS
  if (auto res = stream.peek<Header::identity_t>()) {
    auto ident = *res;
    uint32_t ei_data = ident[Header::ELI_DATA];
    const auto data = static_cast<Header::ELF_DATA>(ei_data);
    if (data == Header::ELF_DATA::LSB || data == Header::ELF_DATA::MSB) {
      from_ei_data = data;
    }
  }

  // Try to determine the size based on Elf_Ehdr.e_machine
  //
  // typedef struct {
  //     unsigned char e_ident[EI_NIDENT]; | +0x00
  //     uint16_t      e_type;             | +0x10
  //     uint16_t      e_machine;          | +0x12 <------ THIS
  //     uint32_t      e_version;          |
  //     ....
  // } ElfN_Ehdr;
  constexpr size_t e_machine_off = offsetof(details::Elf32_Ehdr, e_machine);
  {
    // Read Machine type with both endianess
    ARCH machine      = ARCH::NONE; // e_machine value without endian swap enabled
    ARCH machine_swap = ARCH::NONE; // e_machine value with endian swap enabled
    if (auto res = stream.peek<uint16_t>(e_machine_off)) {
      machine = static_cast<ARCH>(*res);
    }

    {
      ToggleEndianness swapped(stream);
      if (auto res = swapped->peek<uint16_t>(e_machine_off)) {
        machine_swap = static_cast<ARCH>(*res);
      }
    }

    LIEF_DEBUG("Machine      '{}' (0x{:x})", to_string(machine), (int)machine);
    LIEF_DEBUG("Machine Swap '{}' (0x{:x})", to_string(machine_swap), (int)machine);

    const Header::ELF_DATA endian      = determine_elf_endianess(machine);
    const Header::ELF_DATA endian_swap = determine_elf_endianess(machine_swap);

    LIEF_DEBUG("Endian: {}",        to_string(endian));
    LIEF_DEBUG("Endian (swap): {}", to_string(endian_swap));

    if (endian != Header::ELF_DATA::NONE) {
      return endian;
    }

    if (endian_swap != Header::ELF_DATA::NONE) {
      return endian_swap;
    }

    {
      auto it = std::find(BOTH_ENDIANESS.begin(), BOTH_ENDIANESS.end(),
                          machine);
      if (it != BOTH_ENDIANESS.end()) {
        return get_endianess();
      }
    }

    {
      auto it = std::find(BOTH_ENDIANESS.begin(), BOTH_ENDIANESS.end(),
                          machine_swap);
      if (it != BOTH_ENDIANESS.end()) {
        return invert_endianess(get_endianess());
      }
    }
  }
  return from_ei_data;
}

bool Parser::should_swap() const {
  const Header::ELF_DATA binary_endian  = determine_elf_endianess(*stream_);
  const Header::ELF_DATA current_endian = get_endianess();
  LIEF_DEBUG("LIEF Endianness:   '{}'", to_string(current_endian));
  LIEF_DEBUG("Binary Endianness: '{}'", to_string(binary_endian));
  if (binary_endian  != Header::ELF_DATA::NONE &&
      current_endian != Header::ELF_DATA::NONE)
  {
    return binary_endian != current_endian;
  }
  return false;
}

Target determine_elf_target(BinaryStream& stream) {
  auto from_ei_class  = Header::CLASS::NONE;
  auto from_e_machine = Header::CLASS::NONE;
  auto file_type = Header::FILE_TYPE::NONE;

  // First, check EI_CLASS
  if (auto res = stream.peek<Header::identity_t>()) {
    auto ident = *res;
    uint32_t ei_class = ident[Header::ELI_CLASS];
    const auto typed = Header::CLASS(ei_class);
    if (typed == Header::CLASS::ELF32 || typed == Header::CLASS::ELF64) {
      from_ei_class = typed;
    }
  }

  constexpr size_t e_type_off = offsetof(details::Elf32_Ehdr, e_type);
  if (auto res = stream.peek<uint16_t>(e_type_off)) {
    file_type = Header::FILE_TYPE(*res);
  }

  // Try to determine the size based on Elf_Ehdr.e_machine
  //
  // typedef struct {
  //     unsigned char e_ident[EI_NIDENT]; | +0x00
  //     uint16_t      e_type;             | +0x10
  //     uint16_t      e_machine;          | +0x12 <------ THIS
  //     uint32_t      e_version;          |
  //     ....
  // } ElfN_Ehdr;
  constexpr size_t e_machine_off = offsetof(details::Elf32_Ehdr, e_machine);
  auto machine = ARCH::NONE;
  if (auto res = stream.peek<uint16_t>(e_machine_off)) {
    machine = (ARCH)*res;
    switch (machine) {
      case ARCH::AARCH64:
      case ARCH::X86_64:
      case ARCH::PPC64:
      case ARCH::SPARCV9:
        {
          from_e_machine = Header::CLASS::ELF64;
          break;
        }
      case ARCH::I386:
      case ARCH::ARM:
      case ARCH::PPC:
        {
          from_e_machine = Header::CLASS::ELF32;
          break;
        }
      default:
        {
          from_e_machine = Header::CLASS::NONE;
          break;
        }
    }
  }
  if (from_e_machine != Header::CLASS::NONE &&
      from_ei_class != Header::CLASS::NONE)
  {
    if (from_e_machine == from_ei_class) {
      return {from_ei_class, machine};
    }

    if (file_type == Header::FILE_TYPE::REL) {
      return {from_ei_class, machine};
    }

    if (machine == ARCH::X86_64 && from_ei_class == Header::CLASS::ELF32) {
      LIEF_DEBUG("ELF32-X86_64: x32");
      return {from_ei_class, machine};
    }

    if (machine == ARCH::AARCH64 && from_ei_class == Header::CLASS::ELF32) {
      LIEF_DEBUG("ELF32-arm64: arm64ilp32");
      return {from_ei_class, machine};
    }

    LIEF_WARN("ELF class from machine type ('{}') does not match ELF class from "
              "e_ident ('{}'). The binary has been likely modified.",
              to_string(from_e_machine), to_string(from_ei_class));
    // Make the priority on Elf_Ehdr.e_machine as it is
    // this value that is used by the kernel.
    return {from_e_machine, machine};
  }
  if (from_e_machine != Header::CLASS::NONE) {
    return {from_e_machine, machine};
  }
  return {from_ei_class, machine};
}


ok_error_t Parser::init() {
  if (stream_ == nullptr) {
    LIEF_ERR("Stream not properly initialized");
    return make_error_code(lief_errors::parsing_error);
  }

  binary_->should_swap_ = should_swap();
  binary_->original_size_ = stream_->size();
  binary_->pagesize_ = config_.page_size;

  auto res = DataHandler::Handler::from_stream(stream_);
  if (!res) {
    LIEF_ERR("The provided stream is not supported by the ELF DataHandler");
    return make_error_code(lief_errors::not_supported);
  }

  binary_->datahandler_ = std::move(*res);

  auto res_ident = stream_->peek<Header::identity_t>();
  if (!res_ident) {
    LIEF_ERR("Can't read ELF identity. Nothing to parse");
    return make_error_code(res_ident.error());
  }

  LIEF_DEBUG("Should swap: {}", should_swap());
  stream_->set_endian_swap(should_swap());
  Target elf_target = determine_elf_target(*stream_);
  binary_->type_ = elf_target.clazz;

  switch (elf_target.clazz) {
    case Header::CLASS::ELF32:
      {
        if (elf_target.arch == ARCH::X86_64) {
          return parse_binary<details::ELF32_x32>();
        }

        if (elf_target.arch == ARCH::AARCH64) {
          return parse_binary<details::ELF32_arm64>();
        }

        return parse_binary<details::ELF32>();
      }
    case Header::CLASS::ELF64: return parse_binary<details::ELF64>();
    case Header::CLASS::NONE:
      {
        LIEF_ERR("Can't determine the ELF class ({})",
                  static_cast<size_t>(binary_->type_));
        return make_error_code(lief_errors::corrupted);
      }
  }

  return ok();
}

std::unique_ptr<Binary> Parser::parse(const std::string& filename,
                                      const ParserConfig& conf) {
  if (!is_elf(filename)) {
    return nullptr;
  }

  Parser parser{filename, conf};
  parser.init();
  return std::move(parser.binary_);
}

std::unique_ptr<Binary> Parser::parse(const std::vector<uint8_t>& data,
                                      const ParserConfig& conf) {
  if (!is_elf(data)) {
    return nullptr;
  }

  Parser parser{data, conf};
  parser.init();
  return std::move(parser.binary_);
}

std::unique_ptr<Binary> Parser::parse(std::unique_ptr<BinaryStream> stream,
                                      const ParserConfig& conf) {
  if (stream == nullptr) {
    return nullptr;
  }

  if (!is_elf(*stream)) {
    return nullptr;
  }

  Parser parser{std::move(stream), conf};
  parser.init();
  return std::move(parser.binary_);
}


ok_error_t Parser::parse_symbol_version(uint64_t symbol_version_offset) {
  LIEF_DEBUG("== Parsing symbol version ==");
  LIEF_DEBUG("Symbol version offset: 0x{:x}", symbol_version_offset);

  const auto nb_entries = static_cast<uint32_t>(binary_->dynamic_symbols_.size());

  stream_->setpos(symbol_version_offset);
  for (size_t i = 0; i < nb_entries; ++i) {
    auto val = stream_->read<uint16_t>();
    if (!val) {
      break;
    }
    binary_->symbol_version_table_.emplace_back(std::make_unique<SymbolVersion>(*val));
  }
  return ok();
}


result<uint64_t> Parser::get_dynamic_string_table_from_segments(BinaryStream* stream) const {
  const ARCH arch = binary_->header().machine_type();
  if (const DynamicEntry* dt_str = binary_->get(DynamicEntry::TAG::STRTAB)) {
    return binary_->virtual_address_to_offset(dt_str->value());
  }

  if (stream != nullptr) {
    size_t count = 0;
    ScopedStream scope(*stream);
    while (*scope) {
      if (++count > Parser::NB_MAX_DYNAMIC_ENTRIES) {
        break;
      }
      if (binary_->type_ == Header::CLASS::ELF32) {
        auto dt = scope->read<details::Elf32_Dyn>();
        if (!dt) {
          break;
        }
        if (DynamicEntry::from_value(dt->d_tag, arch) == DynamicEntry::TAG::STRTAB) {
          return binary_->virtual_address_to_offset(dt->d_un.d_val);
        }
      } else {
        auto dt = scope->read<details::Elf64_Dyn>();
        if (!dt) {
          break;
        }

        if (DynamicEntry::from_value(dt->d_tag, arch) == DynamicEntry::TAG::STRTAB) {
          return binary_->virtual_address_to_offset(dt->d_un.d_val);
        }
      }
    }
  }

  Segment* dyn_segment = binary_->get(Segment::TYPE::DYNAMIC);
  if (dyn_segment == nullptr) {
    return 0;
  }

  const uint64_t offset = dyn_segment->file_offset();
  const uint64_t size   = dyn_segment->physical_size();

  stream_->setpos(offset);

  if (binary_->type_ == Header::CLASS::ELF32) {
    size_t nb_entries = size / sizeof(details::Elf32_Dyn);

    for (size_t i = 0; i < nb_entries; ++i) {
      auto res = stream_->read<details::Elf32_Dyn>();
      if (!res) {
        LIEF_ERR("Can't read dynamic entry #{}", i);
        return 0;
      }
      auto dt = *res;

      if (DynamicEntry::from_value(dt.d_tag, arch) == DynamicEntry::TAG::STRTAB) {
        return binary_->virtual_address_to_offset(dt.d_un.d_val);
      }
    }

  } else {
    size_t nb_entries = size / sizeof(details::Elf64_Dyn);
    for (size_t i = 0; i < nb_entries; ++i) {
      auto res = stream_->read<details::Elf64_Dyn>();
      if (!res) {
        LIEF_ERR("Can't read dynamic entry #{}", i);
        return 0;
      }
      const auto dt = *res;

      if (DynamicEntry::from_value(dt.d_tag, arch) == DynamicEntry::TAG::STRTAB) {
        return binary_->virtual_address_to_offset(dt.d_un.d_val);
      }
    }
  }
  return 0;
}

uint64_t Parser::get_dynamic_string_table_from_sections() const {
  // Find Dynamic string section
  auto it_dynamic_string_section = std::find_if(
      std::begin(binary_->sections_), std::end(binary_->sections_),
      [] (const std::unique_ptr<Section>& section) {
        return section->name() == ".dynstr" &&
               section->type() == Section::TYPE::STRTAB;
      });


  if (it_dynamic_string_section == std::end(binary_->sections_)) {
    return 0;
  }
  return (*it_dynamic_string_section)->file_offset();
}

uint64_t Parser::get_dynamic_string_table(BinaryStream* stream) const {
  if (auto res = get_dynamic_string_table_from_segments(stream)) {
    return *res;
  }
  return get_dynamic_string_table_from_sections();
}


void Parser::link_symbol_version() {
  if (binary_->dynamic_symbols_.size() == binary_->symbol_version_table_.size()) {
    for (size_t i = 0; i < binary_->dynamic_symbols_.size(); ++i) {
      binary_->dynamic_symbols_[i]->symbol_version_ = binary_->symbol_version_table_[i].get();
    }
  }
}

ok_error_t Parser::parse_symbol_sysv_hash(uint64_t offset) {
  LIEF_DEBUG("== Parse SYSV hash table ==");
  auto sysvhash = std::make_unique<SysvHash>();

  stream_->setpos(offset);

  auto res_nbucket = stream_->read<uint32_t>();
  if (!res_nbucket) {
    LIEF_ERR("Can't read the number of buckets");
    return make_error_code(lief_errors::read_error);
  }

  auto res_nchains = stream_->read<uint32_t>();
  if (!res_nchains) {
    LIEF_ERR("Can't read the number of chains");
    return make_error_code(lief_errors::read_error);
  }

  const auto nbuckets = std::min<uint32_t>(*res_nbucket, Parser::NB_MAX_BUCKETS);
  const auto nchain   = std::min<uint32_t>(*res_nchains, Parser::NB_MAX_CHAINS);

  sysvhash->buckets_.reserve(nbuckets);

  for (size_t i = 0; i < nbuckets; ++i) {
    if (auto bucket = stream_->read<uint32_t>()) {
      sysvhash->buckets_.push_back(*bucket);
    } else {
      LIEF_ERR("Can't read bucket #{}", i);
      break;
    }
  }

  sysvhash->chains_.reserve(nchain);
  for (size_t i = 0; i < nchain; ++i) {
    if (auto chain = stream_->read<uint32_t>()) {
      sysvhash->chains_.push_back(*chain);
    } else {
      LIEF_ERR("Can't read chain #{}", i);
      break;
    }
  }

  binary_->sysv_hash_ = std::move(sysvhash);
  binary_->sizing_info_->hash = stream_->pos() - offset;
  return ok();
}

#if 0
std::unique_ptr<Note> Parser::get_note(uint32_t type, std::string name,
                                       std::vector<uint8_t> desc_bytes)
{
  const E_TYPE ftype = binary_->header().file_type();

  auto conv = Note::convert_type(ftype, type, name);
  if (!conv) {
    LIEF_WARN("Note type: 0x{:x} is not supported for owner: '{}'", type, name);
    return std::make_unique<Note>(std::move(name), Note::TYPE::UNKNOWN, type,
                                  std::move(desc_bytes));
  }

  Note::TYPE ntype = *conv;

  if (ntype != Note::TYPE::GNU_BUILD_ATTRIBUTE_FUNC &&
      ntype != Note::TYPE::GNU_BUILD_ATTRIBUTE_OPEN)
  {
    name = name.c_str();
  }

  const ARCH arch = binary_->header().machine_type();
  const ELF_CLASS cls = binary_->header().identity_class();

  if (cls != ELF_CLASS::ELFCLASS32 && cls != ELF_CLASS::ELFCLASS64) {
    LIEF_WARN("Invalid ELFCLASS");
    return nullptr;
  }

  switch (ntype) {
    case Note::TYPE::CORE_PRSTATUS:
        return std::make_unique<CorePrStatus>(arch, cls, std::move(name), type,
                                              std::move(desc_bytes));
    case Note::TYPE::CORE_PRPSINFO:
        return std::make_unique<CorePrPsInfo>(arch, cls, std::move(name), type,
                                              std::move(desc_bytes));
    case Note::TYPE::CORE_FILE:
        return std::make_unique<CoreFile>(arch, cls, std::move(name), type,
                                          std::move(desc_bytes));
    case Note::TYPE::CORE_AUXV:
        return std::make_unique<CoreAuxv>(arch, cls, std::move(name), type,
                                          std::move(desc_bytes));
    case Note::TYPE::CORE_SIGINFO:
        return std::make_unique<CoreSigInfo>(std::move(name), ntype, type,
                                             std::move(desc_bytes));
    case Note::TYPE::ANDROID_IDENT:
        return std::make_unique<AndroidIdent>(std::move(name), ntype, type,
                                              std::move(desc_bytes));
    case Note::TYPE::GNU_ABI_TAG:
        return std::make_unique<NoteAbi>(std::move(name), ntype, type,
                                         std::move(desc_bytes));

    default:
        return std::make_unique<Note>(std::move(name), ntype, type,
                                      std::move(desc_bytes));
  }
}
#endif

ok_error_t Parser::parse_notes(uint64_t offset, uint64_t size) {
  static constexpr auto ERROR_THRESHOLD = 6;
  LIEF_DEBUG("== Parsing note segment ==");
  stream_->setpos(offset);
  uint64_t last_offset = offset + size;
  size_t error_count = 0;

  if (!*stream_) {
    return make_error_code(lief_errors::read_error);
  }

  while (*stream_ && stream_->pos() < last_offset) {
    const auto current_pos = static_cast<int64_t>(stream_->pos());
    const Section* sec = binary_->section_from_offset(current_pos);
    std::string sec_name = sec != nullptr ? sec->name() : "";

    std::unique_ptr<Note> note = Note::create(
        *stream_, std::move(sec_name),
        binary_->header().file_type(), binary_->header().machine_type(),
        binary_->header().identity_class()
    );

    if (note != nullptr) {
      const auto it_note = std::find_if(
          std::begin(binary_->notes_), std::end(binary_->notes_),
          [&note] (const std::unique_ptr<Note>& n) { return *n == *note; });

      if (it_note == std::end(binary_->notes_)) { // Not already present
        binary_->notes_.push_back(std::move(note));
      }
    } else {
      LIEF_WARN("Note not parsed!");
      ++error_count;
    }

    if (error_count > ERROR_THRESHOLD) {
      LIEF_ERR("Too many errors while trying to parse notes");
      return make_error_code(lief_errors::corrupted);
    }

    if (static_cast<int64_t>(stream_->pos()) <= current_pos) {
      return make_error_code(lief_errors::corrupted);
    }
  }
  return ok();
}


ok_error_t Parser::parse_overlay() {
  const uint64_t last_offset = binary_->eof_offset();

  if (last_offset > stream_->size()) {
    return ok();
  }
  const uint64_t overlay_size = stream_->size() - last_offset;

  if (overlay_size == 0) {
    return ok();
  }

  LIEF_INFO("Overlay detected at 0x{:x} ({} bytes)", last_offset, overlay_size);

  if (!stream_->peek_data(binary_->overlay_, last_offset, overlay_size)) {
    LIEF_WARN("Can't read overlay data");
    return make_error_code(lief_errors::read_error);
  }
  return ok();
}

bool Parser::check_section_in_segment(const Section& section, const Segment& segment) {
  if (section.virtual_address() > 0) {
    const uint64_t seg_vend = segment.virtual_address() + segment.virtual_size();
    return segment.virtual_address() <= section.virtual_address() &&
           section.virtual_address() + section.size() <= seg_vend;
  }

  if (section.file_offset() > 0) {
    const uint64_t seg_end = segment.file_offset() + segment.physical_size();
    return segment.file_offset() <= section.file_offset() &&
           section.file_offset() + section.size() <= seg_end;
  }
  return false;
}

ok_error_t Parser::link_symbol_section(Symbol& sym) {
  const uint16_t sec_idx = sym.section_idx();
  if (sec_idx == Symbol::SECTION_INDEX::ABS ||
      sec_idx == Symbol::SECTION_INDEX::UNDEF) {
    // Nothing to bind
    return ok();
  }

  auto it_section = sections_idx_.find(sec_idx);
  if (it_section == std::end(sections_idx_)) {
    return make_error_code(lief_errors::corrupted);
  }

  sym.section_ = it_section->second;
  return ok();
}


bool Parser::bind_symbol(Relocation& R) {
  if (!config_.parse_dyn_symbols) {
    return false;
  }
  const uint32_t idx = R.info();
  if (idx >= binary_->dynamic_symbols_.size()) {
    LIEF_DEBUG("Index #{} is out of range for reloc: {}", idx, to_string(R));
    return false;
  }

  R.symbol_ = binary_->dynamic_symbols_[idx].get();

  return true;
}

Relocation& Parser::insert_relocation(std::unique_ptr<Relocation> R) {
  R->binary_ = binary_.get();
  binary_->relocations_.push_back(std::move(R));
  return *binary_->relocations_.back();
}

}
}
