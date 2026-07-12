/* Copyright 2021 - 2025 R. Thomas
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
#include <spdlog/fmt/fmt.h>

#include "MachO/Structures.hpp"

#include "LIEF/BinaryStream/SpanStream.hpp"

#include "LIEF/MachO/AtomInfo.hpp"
#include "LIEF/MachO/FatBinary.hpp"
#include "LIEF/MachO/Binary.hpp"
#include "LIEF/MachO/Routine.hpp"
#include "LIEF/MachO/CodeSignature.hpp"
#include "LIEF/MachO/CodeSignatureDir.hpp"
#include "LIEF/MachO/DataInCode.hpp"
#include "LIEF/MachO/DyldChainedFixups.hpp"
#include "LIEF/MachO/DyldExportsTrie.hpp"
#include "LIEF/MachO/DyldInfo.hpp"
#include "LIEF/MachO/DylibCommand.hpp"
#include "LIEF/MachO/DynamicSymbolCommand.hpp"
#include "LIEF/MachO/FunctionStarts.hpp"
#include "LIEF/MachO/FunctionVariants.hpp"
#include "LIEF/MachO/FunctionVariantFixups.hpp"
#include "LIEF/MachO/LinkerOptHint.hpp"
#include "LIEF/MachO/MainCommand.hpp"
#include "LIEF/MachO/RPathCommand.hpp"
#include "LIEF/MachO/Section.hpp"
#include "LIEF/MachO/SegmentCommand.hpp"
#include "LIEF/MachO/SegmentSplitInfo.hpp"
#include "LIEF/MachO/SymbolCommand.hpp"
#include "LIEF/MachO/ThreadCommand.hpp"
#include "LIEF/MachO/TwoLevelHints.hpp"
#include "LIEF/MachO/utils.hpp"
#include "LIEF/utils.hpp"

#include "MachO/ChainedFixup.hpp"

#include "logging.hpp"
#include "Object.tcc"

#include <string>

namespace LIEF::MachO {
template<typename T, typename U>
inline bool greater_than_add_or_overflow(T LHS, T RHS, U B) {
  return (LHS > B) || (RHS > (B - LHS));
}

inline uint64_t rnd64(uint64_t v, uint64_t r) {
  r--;
  v += r;
  v &= ~static_cast<int64_t>(r);
  return v;
}

inline uint64_t rnd(uint64_t v, uint64_t r) {
  return rnd64(v, r);
}

class LayoutChecker {
  public:
  LayoutChecker() = delete;
  LayoutChecker(const Binary& macho) :
    binary(macho)
  {}


  bool check();

  bool error(std::string msg) {
    error_msg = std::move(msg);
    return false;
  }

  template <typename... Args>
  bool error(const char *fmt, const Args &... args) {
    error_msg = fmt::format(fmt, args...);
    return false;
  }

  // Return true if segments overlap
  bool check_overlapping();

  // Mirror MachOAnalyzer::validLoadCommands
  bool check_load_commands();

  // Mirror of MachOAnalyzer::validMain
  bool check_main();

  // Mirror of MachOAnalyzer::validEmbeddedPaths
  bool check_valid_paths();

  // Mirror of Image::validStructureLinkedit (version: dyld-1235.2)
  bool check_linkedit();

  // Mirror of MachOFile::validSegments
  bool check_segments();

  // Mirror Image::validInitializers
  bool check_initializers();

  // Mirror ChainedFixups::valid
  bool check_chained_fixups();

  // Check from PR #1136
  // See: https://github.com/lief-project/LIEF/pull/1136/files#r1882625692
  bool check_section_contiguity();

  // From FunctionVariants::valid()
  bool check_function_variants();

  bool check_linkedit_end();

  size_t ptr_size() const {
    return binary.header().is_32bit() ? sizeof(uint32_t) :
                                        sizeof(uint64_t);
  }

  const std::string& get_error() const {
    return error_msg;
  }

  private:
  std::string error_msg;
  const Binary& binary;
};

bool LayoutChecker::check_initializers() {
  struct range_t {
    uint64_t va_start = 0;
    uint64_t va_end = 0;
    uint64_t filesz = 0;
  };

  class ranges_t : public std::vector<range_t> {
    public:
    using base_t = std::vector<range_t>;
    using base_t::base_t;

    bool contain(uint64_t addr) const {
      return std::find_if(begin(), end(), [addr] (const range_t& R) {
        return R.va_start <= addr && addr < (R.va_start + R.va_end);
      }) != end();
    }
  };

  const size_t ptr_size = this->ptr_size();
  const uint64_t imagebase = binary.imagebase();

  ranges_t ranges;
  ranges.reserve(binary.segments().size());
  for (const SegmentCommand& segment : binary.segments()) {
    if (segment.is(SegmentCommand::VM_PROTECTIONS::EXECUTE)) {
      ranges.push_back({
        segment.virtual_address(), segment.virtual_address() + segment.virtual_size(),
        segment.file_size()
      });
    }
  }

  if (ranges.empty()) {
    return error("no executable segments");
  }

  for (const LoadCommand& cmd : binary.commands()) {
    const auto* routines = cmd.cast<Routine>();
    if (routines == nullptr) {
      continue;
    }

    uint64_t init_addr = routines->init_address();
    if (!ranges.contain(init_addr)) {
      return error("LC_ROUTINE/64 initializer 0x{:016x} is not an offset "
                   "to an executable segment", init_addr);
    }
  }

  for (const Section& section : binary.sections()) {
    const bool contains_init_ptr =
      section.type() == Section::TYPE::MOD_INIT_FUNC_POINTERS ||
      section.type() == Section::TYPE::MOD_TERM_FUNC_POINTERS;

    if (!contains_init_ptr) {
      continue;
    }

    if (section.size() % ptr_size != 0) {
      return error("Section '{}' size (0x{:08x}) is not a multiple of pointer-size",
                   section.name(), section.size());
    }

    if (section.address() % ptr_size != 0) {
      return error("Section '{}' address (0x{:08x}) is not pointer aligned",
                   section.name(), section.address());
    }

    SpanStream stream(section.content());
    while (stream) {
      uint64_t ptr = 0;
      if (ptr_size == sizeof(uint32_t)) {
        auto res = stream.read<uint32_t>();
        if (!res) {
          return error("Can't read pointer at 0x{:04x} in {}",
                       stream.pos(), section.name());
        }

        // As mentioned in the dyld documentation:
        //  > FIXME: as a quick hack, the low 32-bits with either rebase opcodes
        //  > or chained fixups is offset in image
        ptr = (uint32_t)*res;
      } else {
        auto res = stream.read<uint64_t>();
        if (!res) {
          return error("Can't read pointer at 0x{:04x} in {}",
                       stream.pos(), section.name());
        }
        // As mentioned in the dyld documentation:
        //  > FIXME: as a quick hack, the low 32-bits with either rebase opcodes
        //  > or chained fixups is offset in image
        ptr = *res & 0x03FFFFFF;
      }
      if (!ranges.contain(imagebase + ptr)) {
        return error("initializer at 0x{:06x} in {} is not in an executable segment",
                     stream.pos(), section.name());
      }
    }
  }


  for (const Section& section : binary.sections()) {
    if (section.type() != Section::TYPE::INIT_FUNC_OFFSETS) {
      continue;
    }
    if (section.segment()->is(SegmentCommand::VM_PROTECTIONS::WRITE)) {
      return error("initializer offsets section {}/{} must be in read-only segment",
                   section.segment_name(), section.name());
    }

    if (section.size() % 4 != 0) {
      return error("initializer offsets section {}/{} has bad size",
                   section.segment_name(), section.name());
    }

    if (section.address() % 4 != 0) {
      return error("initializer offsets section {}/{} is not 4-byte aligned",
                   section.segment_name(), section.name());
    }

    SpanStream stream(section.content());

    while (stream) {
      auto res = stream.read<uint32_t>();
      if (!res) {
        return error("Can't read pointer at 0x{:04} in {}",
                     stream.pos(), section.name());
      }

      if (!ranges.contain(imagebase + *res)) {
        return error("initializer 0x{:08x} is not an offset to an executable "
                     "segment", *res);
      }
    }

  }
  return true;
}

bool LayoutChecker::check_segments() {
  const size_t sizeof_segment_hdr = binary.header().is_32bit() ?
    sizeof(details::segment_command_32) : sizeof(details::segment_command_64);

  const size_t sizeof_section_hdr = binary.header().is_32bit() ?
    sizeof(details::section_32) : sizeof(details::section_64);

  for (const SegmentCommand& segment : binary.segments()) {
    int32_t section_space = segment.size() - sizeof_segment_hdr;
    if (section_space < 0) {
      return error("load command is too small for LC_SEGMENT/64: {}",
                   segment.name());
    }

    if ((section_space % sizeof_section_hdr) != 0) {
      return error("segment load command size {}: 0x{:04x} will not fit "
                   "whole number of sections", segment.name(), segment.size());
    }

    if ((uint32_t)section_space != segment.numberof_sections() * sizeof_section_hdr) {
      return error("segment {} does not match nsects", segment.name(),
                   segment.numberof_sections());
    }

    if ((segment.file_size() > segment.virtual_size()) &&
        (segment.virtual_size() != 0 || (segment.flags() & (uint32_t)SegmentCommand::FLAGS::NORELOC) != 0))
    {
      return error("in segment {}, filesize exceeds vmsize", segment.name());
    }

    if ((segment.init_protection() & 0xFFFFFFF8) != 0) {
      return error("{} segment permissions has invalid bits set", segment.name());
    }
  }

  if (const SegmentCommand* text_seg = binary.get_segment("__TEXT")) {
    static constexpr auto R_X = (uint32_t)SegmentCommand::VM_PROTECTIONS::READ |
                                (uint32_t)SegmentCommand::VM_PROTECTIONS::EXECUTE;
    if (text_seg->init_protection() != R_X) {
      return error("__TEXT segment must be r-x");
    }
  }

  if (const SegmentCommand* lnk_seg = binary.get_segment("__LINKEDIT")) {
    static constexpr auto RO = (uint32_t)SegmentCommand::VM_PROTECTIONS::READ;
    if (lnk_seg->init_protection() != RO) {
      return error("__LINKEDIT segment must be read only");
    }
  }

  return true;
}

bool LayoutChecker::check_overlapping() {
  for (const SegmentCommand& lhs : binary.segments()) {
    const uint64_t lhs_vm_end   = lhs.virtual_address() + lhs.virtual_size();
    const uint64_t lhs_file_end = lhs.file_offset() + lhs.file_size();
    for (const SegmentCommand& rhs : binary.segments()) {
      if (lhs.index() == rhs.index()) {
        continue;
      }
      const uint64_t rhs_vm_end   = rhs.virtual_address() + rhs.virtual_size();
      const uint64_t rhs_file_end = rhs.file_offset() + rhs.file_size();

      const bool vm_overalp = (rhs.virtual_address() <= lhs.virtual_address() && rhs_vm_end > lhs.virtual_address() && lhs_vm_end > lhs.virtual_address()) ||
                              (rhs.virtual_address() >= lhs.virtual_address()  && rhs.virtual_address() < lhs_vm_end && rhs_vm_end > rhs.virtual_address());
      if (vm_overalp) {
        error(R"delim(
              Segments '{}' and '{}' overlap (virtual addresses):
              [0x{:08x}, 0x{:08x}] [0x{:08x}, 0x{:08x}]
              )delim", lhs.name(), rhs.name(),
              lhs.virtual_address(), lhs_vm_end, rhs.virtual_address(), rhs_vm_end);
        return true;
      }
      const bool file_overlap = (rhs.file_offset() <= lhs.file_offset() && rhs_file_end > lhs.file_offset() && lhs_file_end > lhs.file_offset()) ||
                                (rhs.file_offset() >= lhs.file_offset()  && rhs.file_offset() < lhs_file_end && rhs_file_end > rhs.file_offset());
      if (file_overlap) {
        error(R"delim(
              Segments '{}' and '{}' overlap (file offsets):
              [0x{:08x}, 0x{:08x}] [0x{:08x}, 0x{:08x}]
              )delim", lhs.name(), rhs.name(),
              lhs.file_offset(), lhs_file_end, rhs.file_offset(), rhs_file_end);
        return true;
      }

      if (lhs.index() < rhs.index()) {

        const bool wrong_order = lhs.virtual_address() > rhs.virtual_address() ||
                                 (lhs.file_offset() > rhs.file_offset() && lhs.file_offset() != 0 && rhs.file_offset() != 0);
        if (wrong_order) {
          error(R"delim(
                Segments '{}' and '{}' are wrongly ordered
                )delim", lhs.name(), rhs.name());
          return true;
        }
      }
    }
  }
  return false;
}


// Mirror MachOAnalyzer::validLoadCommands
bool LayoutChecker::check_load_commands() {
  const size_t sizeof_header =
    binary.header().is_32bit() ? sizeof(details::mach_header) :
                                 sizeof(details::mach_header_64);

  const size_t last_cmd_offset = sizeof_header + binary.header().sizeof_cmds();

  if (last_cmd_offset > binary.original_size()) {
    return error("Load commands exceed length of file");
  }

  const auto& commands = binary.commands();
  for (size_t i = 0; i < commands.size(); ++i) {
    const LoadCommand& cmd = commands[i];
    if (cmd.command_offset() > last_cmd_offset ||
        (cmd.command_offset() + cmd.size()) > last_cmd_offset)
    {
      return error("Command #{} ({}) pasts the end of the commands", i,
                   to_string(cmd.command()));
    }

    if (const auto* lib = cmd.cast<DylibCommand>()) {
      if (lib->name_offset() > lib->size()) {
        return error("{}: load command {} name offset ({}) outside its size ({})",
                     lib->name(), to_string(cmd.command()), lib->name_offset(),
                     lib->size());
      }
      span<const uint8_t> raw = cmd.data();
      const auto start = raw.begin() + lib->name_offset();
      auto it = std::find(start, raw.end(), '\0');
      if (it == raw.end()) {
        return error("{}: string extends beyond end of load command", lib->name());
      }
    }

    if (const auto* rpath = cmd.cast<RPathCommand>()) {
      if (rpath->path_offset() > rpath->size()) {
        return error("{}: load command {} name offset ({}) outside its size ({})",
                     rpath->path(), to_string(cmd.command()), rpath->path_offset(),
                     rpath->size());
      }

      span<const uint8_t> raw = cmd.data();
      const auto start = raw.begin() + rpath->path_offset();
      auto it = std::find(start, raw.end(), '\0');
      if (it == raw.end()) {
        return error("{}: string extends beyond end of load command", rpath->path());
      }
    }
  }
  return true;
}

// Mirror of MachOAnalyzer::validMain
bool LayoutChecker::check_main() {
  if (binary.header().file_type() != Header::FILE_TYPE::EXECUTE) {
    return true;
  }

  bool has_main = false;
  bool has_thread = false;

  if (const MainCommand* cmd = binary.main_command()) {
    has_main = true;
    uint64_t start_address = binary.imagebase() + cmd->entrypoint();
    const SegmentCommand* segment = binary.segment_from_virtual_address(start_address);
    if (segment == nullptr) {
      return error("LC_MAIN entryoff is out of range");
    }

    if (!segment->is(SegmentCommand::VM_PROTECTIONS::EXECUTE)) {
      return error("LC_MAIN points to non-executable segment");
    }
  }


  if (const ThreadCommand* cmd = binary.thread_command()) {
    has_thread = true;
    uint64_t start_address = cmd->pc();
    if (start_address == 0) {
      return error("LC_UNIXTHREAD not valid for the current arch");
    }

    const SegmentCommand* segment = binary.segment_from_virtual_address(start_address);
    if (segment == nullptr) {
      return error("LC_MAIN entryoff is out of range");
    }

    if (!segment->is(SegmentCommand::VM_PROTECTIONS::EXECUTE)) {
      return error("LC_MAIN points to non-executable segment");
    }
  }

  if (has_main || has_thread) {
    return true;
  }

  if (!has_main && !has_thread) {
    return error("Missing LC_MAIN or LC_UNIXTHREAD command");
  }

  return error("only one LC_MAIN or LC_UNIXTHREAD is allowed");
}

// Mirror of MachOAnalyzer::validEmbeddedPaths
bool LayoutChecker::check_valid_paths() {
  bool has_install_name = false;
  int dependents_count  = 0;
  for (const LoadCommand& cmd : binary.commands()) {
    switch (cmd.command()) {
      case LoadCommand::TYPE::ID_DYLIB:
        {
          has_install_name = true;
          [[fallthrough]];
        }
      case LoadCommand::TYPE::LOAD_DYLIB:
      case LoadCommand::TYPE::LOAD_WEAK_DYLIB:
      case LoadCommand::TYPE::REEXPORT_DYLIB:
      case LoadCommand::TYPE::LOAD_UPWARD_DYLIB:
        {
          if (!DylibCommand::classof(&cmd)) {
            LIEF_ERR("{} is not associated with a DylibCommand which should be the case",
                     to_string(cmd.command()));
            break;
          }
          auto& dylib = *cmd.as<DylibCommand>();
          if (dylib.command() != LoadCommand::TYPE::ID_DYLIB) {
            ++dependents_count;
          }
          break;
        }
      default: {}
    }
  }

  const Header::FILE_TYPE ftype = binary.header().file_type();
  if (ftype == Header::FILE_TYPE::DYLIB) {
    if (!has_install_name) {
      return error("Missing a LC_ID_DYLIB command for a MH_DYLIB file");
    }
  } else {
    if (has_install_name) {
      return error("LC_ID_DYLIB command found in a non MH_DYLIB file");
    }
  }
  const bool is_dynamic_exe =
    ftype == Header::FILE_TYPE::EXECUTE && binary.has(LoadCommand::TYPE::LOAD_DYLINKER);
  if (dependents_count == 0 && is_dynamic_exe) {
    return error("Missing libraries. It must link with at least one library "
                 "(like libSystem.dylib)");
  }
  return true;
}

// Mirror of Image::validStructureLinkedit (version: dyld-1235.2)
bool LayoutChecker::check_linkedit() {
  struct chunk_t {
    enum class KIND {
      UNKNOWN = 0,
      SYMTAB, SYMTAB_STR,
      DYSYMTAB_LOC_REL, DYSYMTAB_EXT_REL, DYSYMTAB_INDIRECT_SYM,
      DYLD_INFO_REBASES, DYLD_INFO_BIND,
      DYLD_INFO_WEAK_BIND, DYLD_INFO_LAZY_BIND,
      DYLD_INFO_EXPORT,
      SEGMENT_SPLIT_INFO,
      ATOM_INFO,
      FUNCTION_STARTS,
      DATA_IN_CODE,
      CODE_SIGNATURE,
      DYLD_EXPORT_TRIE,
      DYLD_CHAINED_FIXUPS
    };
    static const char* to_string(KIND kind) {
      switch (kind) {
        case KIND::UNKNOWN: return "UNKNOWN";
        case KIND::SYMTAB: return "SYMTAB";
        case KIND::SYMTAB_STR: return "SYMTAB_STR";
        case KIND::DYSYMTAB_LOC_REL: return "DYSYMTAB_LOC_REL";
        case KIND::DYSYMTAB_EXT_REL: return "DYSYMTAB_EXT_REL";
        case KIND::DYSYMTAB_INDIRECT_SYM: return "DYSYMTAB_INDIRECT_SYM";
        case KIND::DYLD_INFO_REBASES: return "DYLD_INFO_REBASES";
        case KIND::DYLD_INFO_BIND: return "DYLD_INFO_BIND";
        case KIND::DYLD_INFO_WEAK_BIND: return "DYLD_INFO_WEAK_BIND";
        case KIND::DYLD_INFO_LAZY_BIND: return "DYLD_INFO_LAZY_BIND";
        case KIND::DYLD_INFO_EXPORT: return "DYLD_INFO_EXPORT";
        case KIND::SEGMENT_SPLIT_INFO: return "SEGMENT_SPLIT_INFO";
        case KIND::ATOM_INFO: return "ATOM_INFO";
        case KIND::FUNCTION_STARTS: return "FUNCTION_STARTS";
        case KIND::DATA_IN_CODE: return "DATA_IN_CODE";
        case KIND::CODE_SIGNATURE: return "CODE_SIGNATURE";
        case KIND::DYLD_EXPORT_TRIE: return "DYLD_EXPORT_TRIE";
        case KIND::DYLD_CHAINED_FIXUPS: return "DYLD_CHAINED_FIXUPS";
      }
      return "UNKNOWN";
    }
    chunk_t() = delete;
    chunk_t(KIND kind, uint32_t align, uint32_t off, size_t size) :
      kind(kind), alignment(align), file_offset(off), size(size)
    {}
    chunk_t(const chunk_t&) = default;
    chunk_t& operator=(const chunk_t&) = default;

    chunk_t(chunk_t&&) = default;
    chunk_t& operator=(chunk_t&&) = default;

    KIND kind = KIND::UNKNOWN;
    uint32_t alignment = 0;
    uint32_t file_offset = 0;
    size_t size = 0;
  };
  using chunks_t = std::vector<chunk_t>;

  chunks_t chunks;
  chunks.reserve(binary.commands().size());

  static const auto sort_chunks = [] (chunks_t& chunks) {
    const size_t count = chunks.size();
    for (size_t i = 0; i < count; ++i) {
      bool done = true;
      for (size_t j = 0; j < count - i - 1; ++j) {
        if (chunks[j].file_offset > chunks[j + 1].file_offset) {
          std::swap(chunks[j], chunks[j + 1]);
          done = false;
        }
      }
      if (done) {
        break;
      }
    }
  };

  bool has_external_relocs = false;
  bool has_local_relocs = false;
  uint32_t sym_count = 0;
  uint32_t ind_sym_count = 0;
  bool has_ind_sym_tab = false;
  const size_t ptr_size = this->ptr_size();

  for (const LoadCommand& cmd : binary.commands()) {
    // LC_SYMTAB
    if (const auto* symtab = cmd.cast<SymbolCommand>()) {
      sym_count = symtab->numberof_symbols();
      if (sym_count > 0x10000000) {
        return error("LC_SYMTAB: symbol table too large ({})", sym_count);
      }
      chunks.emplace_back(chunk_t::KIND::SYMTAB, ptr_size,
                            symtab->symbol_offset(), symtab->symbol_table().size());

      if (symtab->strings_size() > 0) {
        chunks.emplace_back(chunk_t::KIND::SYMTAB_STR, 1,
                            symtab->strings_offset(), symtab->strings_size());
      }
    }

    // LC_DYSYMTAB
    if (const auto* dynsym = cmd.cast<DynamicSymbolCommand>()) {
      has_ind_sym_tab = true;
      has_external_relocs = dynsym->nb_external_relocations() != 0;
      has_local_relocs = dynsym->nb_local_relocations() != 0;
      if (size_t cnt = dynsym->nb_indirect_symbols(); cnt > 0x10000000) {
        return error("LC_DYSYMTAB: indirect symbol table too large ({})", cnt);
      }

      if (size_t idx = dynsym->idx_local_symbol(); idx != 0) {
        return error("LC_DYSYMTAB: indirect symbol table ilocalsym != 0");
      }

      if (dynsym->idx_external_define_symbol() != dynsym->nb_local_symbols()) {
        return error("LC_DYSYMTAB: indirect symbol table ilocalsym != 0");
      }

      if (dynsym->idx_external_define_symbol() != dynsym->nb_local_symbols()) {
        return error("LC_DYSYMTAB: indirect symbol table ilocalsym != 0");
      }

      if (dynsym->idx_undefined_symbol() != (dynsym->idx_external_define_symbol() +
                                             dynsym->nb_external_define_symbols()))
      {
        return error("LC_DYSYMTAB: indirect symbol table "
                     "iundefsym != iextdefsym+nextdefsym");
      }

      ind_sym_count = dynsym->idx_undefined_symbol() + dynsym->nb_undefined_symbols();

      if (dynsym->nb_local_relocations() != 0) {
        chunks.emplace_back(chunk_t::KIND::DYSYMTAB_LOC_REL, ptr_size,
                            dynsym->local_relocation_offset(),
                            dynsym->nb_local_relocations() * sizeof(details::relocation_info));
      }
      if (dynsym->nb_external_relocations() != 0) {
        chunks.emplace_back(chunk_t::KIND::DYSYMTAB_EXT_REL, ptr_size,
                            dynsym->external_relocation_offset(),
                            dynsym->nb_external_relocations() * sizeof(details::relocation_info));
      }
      if (dynsym->nb_indirect_symbols() != 0) {
        chunks.emplace_back(chunk_t::KIND::DYSYMTAB_INDIRECT_SYM, 4,
                            dynsym->indirect_symbol_offset(),
                            dynsym->nb_indirect_symbols() * 4);
      }
    }

    if (const auto* dyldinfo = cmd.cast<DyldInfo>()) {
      if (auto info = dyldinfo->rebase(); info.second != 0) {
        chunks.emplace_back(chunk_t::KIND::DYLD_INFO_REBASES, ptr_size,
                            info.first, info.second);
      }

      if (auto info = dyldinfo->bind(); info.second != 0) {
        chunks.emplace_back(chunk_t::KIND::DYLD_INFO_BIND, ptr_size,
                            info.first, info.second);
      }

      if (auto info = dyldinfo->weak_bind(); info.second != 0) {
        chunks.emplace_back(chunk_t::KIND::DYLD_INFO_WEAK_BIND, ptr_size,
                            info.first, info.second);
      }

      if (auto info = dyldinfo->lazy_bind(); info.second != 0) {
        chunks.emplace_back(chunk_t::KIND::DYLD_INFO_LAZY_BIND, ptr_size,
                            info.first, info.second);
      }

      if (auto info = dyldinfo->export_info(); info.second != 0) {
        chunks.emplace_back(chunk_t::KIND::DYLD_INFO_EXPORT, ptr_size,
                            info.first, info.second);
      }
    }

    if (const auto* split_info = cmd.cast<SegmentSplitInfo>()) {
      if (split_info->data_size() != 0) {
        chunks.emplace_back(chunk_t::KIND::SEGMENT_SPLIT_INFO, ptr_size,
                            split_info->data_offset(), split_info->data_size());
      }
    }

    if (const auto* info = cmd.cast<AtomInfo>()) {
      if (info->data_size() != 0) {
        chunks.emplace_back(chunk_t::KIND::ATOM_INFO, ptr_size,
                            info->data_offset(), info->data_size());
      }
    }

    if (const auto* lnk_cmd = cmd.cast<FunctionStarts>()) {
      if (lnk_cmd->data_size() != 0) {
        chunks.emplace_back(chunk_t::KIND::FUNCTION_STARTS, ptr_size,
                            lnk_cmd->data_offset(), lnk_cmd->data_size());
      }
    }

    if (const auto* lnk_cmd = cmd.cast<DataInCode>()) {
      if (lnk_cmd->data_size() != 0) {
        chunks.emplace_back(chunk_t::KIND::DATA_IN_CODE, ptr_size,
                            lnk_cmd->data_offset(), lnk_cmd->data_size());
      }
    }

    if (const auto* lnk_cmd = cmd.cast<CodeSignature>()) {
      if (lnk_cmd->data_size() != 0) {
        chunks.emplace_back(chunk_t::KIND::CODE_SIGNATURE, ptr_size,
                            lnk_cmd->data_offset(), lnk_cmd->data_size());
      }
    }

    if (const auto* lnk_cmd = cmd.cast<DyldExportsTrie>()) {
      if (lnk_cmd->data_size() != 0) {
        chunks.emplace_back(chunk_t::KIND::DYLD_EXPORT_TRIE, ptr_size,
                            lnk_cmd->data_offset(), lnk_cmd->data_size());
      }
    }

    if (const auto* lnk_cmd = cmd.cast<DyldChainedFixups>()) {
      if (lnk_cmd->data_size() != 0) {
        chunks.emplace_back(chunk_t::KIND::DYLD_CHAINED_FIXUPS, ptr_size,
                            lnk_cmd->data_offset(), lnk_cmd->data_size());
      }
    }
  }

  if (has_ind_sym_tab && sym_count != ind_sym_count) {
    return error("symbol count from symbol table and dynamic symbol table differ.");
  }

  const bool has_dyld_info = binary.has(LoadCommand::TYPE::DYLD_INFO_ONLY);
  const bool has_dyld_chained_fixups = binary.has(LoadCommand::TYPE::DYLD_CHAINED_FIXUPS);

  if (has_dyld_info && has_dyld_chained_fixups) {
    return error("Contains LC_DYLD_INFO and LC_DYLD_CHAINED_FIXUPS.");
  }

  if (has_dyld_chained_fixups) {
    if (has_local_relocs) {
      return error("Contains LC_DYLD_CHAINED_FIXUPS and local relocations.");
    }

    if (has_external_relocs) {
      return error("Contains LC_DYLD_CHAINED_FIXUPS and external relocations.");
    }
  }

  uint64_t linkedit_file_offset_start = 0;
  uint64_t linkedit_file_offset_end = 0;

  if (binary.header().file_type() == Header::FILE_TYPE::OBJECT ||
      binary.header().file_type() == Header::FILE_TYPE::PRELOAD)
  {
    for (const Section& section : binary.sections()) {
      const bool is_zero_fill = section.type() == Section::TYPE::ZEROFILL ||
                                section.type() == Section::TYPE::THREAD_LOCAL_ZEROFILL;
      if (is_zero_fill) {
        continue;
      }

      const uint64_t section_end = section.offset() + section.size();
      if (section_end > linkedit_file_offset_start) {
        linkedit_file_offset_start = section_end;
      }
      linkedit_file_offset_end = binary.original_size();
      if (linkedit_file_offset_start == 0) {
        if (const auto* cmd = binary.get(LoadCommand::TYPE::SYMTAB)->cast<SymbolCommand>()) {
          linkedit_file_offset_start = cmd->symbol_offset();
        }
      }
    }
  }
  else {
    if (const SegmentCommand* segment = binary.get_segment("__LINKEDIT")) {
      linkedit_file_offset_start = segment->file_offset();
      linkedit_file_offset_end = segment->file_offset() + segment->file_size();
    }
    if (linkedit_file_offset_start == 0 || linkedit_file_offset_end == 0) {
      return error("bad or unknown fileoffset/size for LINKEDIT");
    }
  }

  if (chunks.empty()) {
    if (binary.header().file_type() == Header::FILE_TYPE::OBJECT) {
      return true;
    }
    return error("Missing __LINKEDIT data");
  }

  sort_chunks(chunks);

  uint64_t prev_end = linkedit_file_offset_start;
  const char* prev_name = "start of __LINKEDIT";
  for (size_t i = 0; i < chunks.size(); ++i) {
    const chunk_t& chunk = chunks[i];
    if (chunk.file_offset < prev_end) {
      return error("LINKEDIT overlap of {} and {}",
                   prev_name, chunk_t::to_string(chunk.kind));
    }

    if (greater_than_add_or_overflow<uint64_t>(
          chunk.file_offset, chunk.size, linkedit_file_offset_end))
    {
      return error("LINKEDIT content '{}' extends beyond end of segment",
                   chunk_t::to_string(chunk.kind));
    }

    if (chunk.file_offset & (chunk.alignment - 1)) {
      // In the source code of dyld this is enforced only the provided
      // policy ask to, but we might want to enforce it by default
      return error("mis-aligned LINKEDIT content: {}", chunk_t::to_string(chunk.kind));
    }

    prev_end = chunk.file_offset + chunk.size;
    prev_name = chunk_t::to_string(chunk.kind);
  }

  return true;
}


bool LayoutChecker::check_linkedit_end() {
  if (binary.header().file_type() == Header::FILE_TYPE::OBJECT) {
    return true;
  }

  const SegmentCommand* linkedit = binary.get_segment("__LINKEDIT");
  if (linkedit == nullptr) {
    return error("Missing __LINKEDIT segment");
  }

  if (linkedit->file_offset() + linkedit->file_size() != binary.original_size()) {
    return error("__LINKEDIT segment does not wrap the end of the binary");
  }
  return true;
}


bool LayoutChecker::check_section_contiguity() {
  if (binary.header().file_type() == Header::FILE_TYPE::OBJECT) {
    // Skip this check for object file
    return true;
  }

  // LIEF allocates space for new/extended sections between the last load command
  // and the first section in the first segment.
  // We are not willing to change the distance between the end of the `__text` section
  // and start of `__DATA` segment, keeping `__text` section in a "fixed" position.
  // Due to above there might happen a "reverse" alignment gap between `__text` section
  // and a section that was allocated in front of it.
  auto is_gap_reversed = [](const Section* LHS, const Section* RHS) {
    return align_down(RHS->offset() - LHS->size(), LHS->alignment());
  };

  for (const SegmentCommand& segment : binary.segments()) {
    std::vector<const Section*> sections_vec;
    auto sections = segment.sections();
    if (sections.empty()) {
      continue;
    }

    sections_vec.reserve(segment.sections().size());
    for (const Section& sec : sections) {
      const Section::TYPE ty = sec.type();
      if (ty == Section::TYPE::ZEROFILL ||
          ty == Section::TYPE::THREAD_LOCAL_ZEROFILL ||
          ty == Section::TYPE::GB_ZEROFILL)
      {
        continue;
      }
      sections_vec.push_back(&sec);
    }

    if (sections_vec.empty()) {
      return true;
    }

    std::sort(sections_vec.begin(), sections_vec.end(),
              [] (const Section* LHS, const Section* RHS) {
                return LHS->offset() < RHS->offset();
              });

    uint64_t next_expected_offset = sections_vec[0]->offset();
    for (auto it = sections_vec.begin(); it != sections_vec.end(); ++it) {
      const Section* section = *it;
      const uint32_t alignment = 1 << section->alignment();
      if ((section->offset() % alignment) != 0) {
        return error("section '{}' offset ({:#06x}) is misaligned (align={:#06x})",
                     section->name(), section->virtual_address(),
                     alignment);
      }

      next_expected_offset = align(next_expected_offset, alignment);

      if (section->offset() != next_expected_offset) {
        auto message = fmt::format("section '{}' is not at the expected offset: {:#06x} "
                                   "(expected={:#06x}, align={:#06x})",
                                   section->name(), section->offset(),
                                   next_expected_offset, alignment);
        if (it != sections_vec.begin() && is_gap_reversed(*std::prev(it), *it) && section->name() == "__text") {
          LIEF_WARN("Permitting section gap which could be caused by LIEF add_section/extend_section: {}", message);
          next_expected_offset = section->offset();
        } else {
          return error(message);
        }
      }
      next_expected_offset += section->size();
    }
  }
  return true;
}

bool LayoutChecker::check() {
  if (binary.header().magic() == MACHO_TYPES::NEURAL_MODEL) {
    return true;
  }

  if (check_overlapping()) {
    return false;
  }

  if (!check_valid_paths()) {
    return false;
  }

  if (!check_linkedit()) {
    return false;
  }

  if (!check_segments()) {
    return false;
  }

  if (!check_load_commands()) {
    return false;
  }

  if (!check_main()) {
    return false;
  }

  if (!check_initializers()) {
    return false;
  }

  if (!check_chained_fixups()) {
    return false;
  }

  if (!check_function_variants()) {
    return false;
  }

  if (!check_section_contiguity()) {
    return false;
  }

  if (!check_linkedit_end()) {
    return false;
  }



  // The following checks are only relevant for non-object files
  if (binary.header().file_type() == Header::FILE_TYPE::OBJECT) {
    return true;
  }

  const SegmentCommand* linkedit = binary.get_segment("__LINKEDIT");
  if (linkedit == nullptr) {
    return error("Missing __LINKEDIT segment");
  }

  const bool is64 = static_cast<const LIEF::Binary&>(binary).header().is_64();
  uint64_t offset = linkedit->file_offset();

  if (const DyldInfo* dyld_info = binary.dyld_info()) {
    if (dyld_info->rebase().first != 0) {
      if (dyld_info->rebase().first != offset) {
        return error(R"delim(
        __LINKEDIT does not start with LC_DYLD_INFO.rebase:
          Expecting offset: 0x{:x} while it is 0x{:x}
        )delim", offset, dyld_info->rebase().first);
      }
    }

    else if (dyld_info->bind().first != 0) {
      if (dyld_info->bind().first != offset) {
        return error(R"delim(
        __LINKEDIT does not start with LC_DYLD_INFO.bind:
          Expecting offset: 0x{:x} while it is 0x{:x}
        )delim", offset, dyld_info->bind().first);
      }
    }

    else if (dyld_info->export_info().first != 0) {
      if (dyld_info->export_info().first != offset &&
          dyld_info->weak_bind().first   != 0      &&
          dyld_info->lazy_bind().first   != 0         )
      {
        return error(R"delim(
                     LC_DYLD_INFO.exports out of place:
                     Expecting offset: 0x{:x} while it is 0x{:x}
                     )delim", offset, dyld_info->export_info().first);
      }
    }

    // Update Offset to end of dyld_info->contents
    if (dyld_info->export_info().second != 0) {
      offset = dyld_info->export_info().first + dyld_info->export_info().second;
    }

    else if (dyld_info->lazy_bind().second != 0) {
      offset = dyld_info->lazy_bind().first + dyld_info->lazy_bind().second;
    }

    else if (dyld_info->weak_bind().second != 0) {
      offset = dyld_info->weak_bind().first + dyld_info->weak_bind().second;
    }

    else if (dyld_info->bind().second != 0) {
      offset = dyld_info->bind().first + dyld_info->bind().second;
    }

    else if (dyld_info->rebase().second != 0) {
      offset = dyld_info->rebase().first + dyld_info->rebase().second;
    }
  }


  if (const DyldChainedFixups* fixups = binary.dyld_chained_fixups()) {
    if (fixups->data_offset() != 0) {
      if (fixups->data_offset() != offset) {
        return error(R"delim(
        __LINKEDIT does not start with LC_DYLD_CHAINED_FIXUPS:
          Expecting offset: 0x{:x} while it is 0x{:x}
        )delim", offset, fixups->data_offset());
      }
      offset += fixups->data_size();
    }
  }

  if (const DyldExportsTrie* exports = binary.dyld_exports_trie()) {
    if (exports->data_offset() != 0) {
      if (exports->data_offset() != offset) {
        return error(R"delim(
        LC_DYLD_EXPORTS_TRIE out of place in __LINKEDIT:
          Expecting offset: 0x{:x} while it is 0x{:x}
        )delim", offset, exports->data_offset());
      }
    }
    offset += exports->data_size();
  }

  if (const FunctionVariants* variants = binary.function_variants()) {
    if (variants->data_offset() != 0) {
      if (variants->data_offset() != offset) {
        return error(R"delim(
        LC_FUNCTION_VARIANTS out of place in __LINKEDIT:
          Expecting offset: 0x{:x} while it is 0x{:x}
        )delim", offset, variants->data_offset());
      }
    }
    offset += variants->data_size();
  }

  if (const FunctionVariantFixups* fixups = binary.function_variant_fixups()) {
    if (fixups->data_offset() != 0) {
      if (fixups->data_offset() != offset) {
        return error(R"delim(
        LC_FUNCTION_VARIANT_FIXUPS out of place in __LINKEDIT:
          Expecting offset: 0x{:x} while it is 0x{:x}
        )delim", offset, fixups->data_offset());
      }
    }
    offset += fixups->data_size();
  }

  const DynamicSymbolCommand* dyst = binary.dynamic_symbol_command();
  if (dyst == nullptr) {
    return error("LC_DYSYMTAB not found");
  }

  if (dyst->nb_local_relocations() != 0) {
    if (dyst->local_relocation_offset() != offset) {
      return error(R"delim(
      LC_DYSYMTAB local relocations out of place:
        Expecting offset: 0x{:x} while it is 0x{:x}
      )delim", offset, dyst->local_relocation_offset());
    }
    offset += dyst->nb_local_relocations() * sizeof(details::relocation_info);
  }

  // Check consistency of Segment Split Info command
  if (const SegmentSplitInfo* spi = binary.segment_split_info()) {
    if (spi->data_offset() != 0 && spi->data_offset() != offset) {
      return error(R"delim(
      LC_SEGMENT_SPLIT_INFO out of place:
        Expecting offset: 0x{:x} while it is 0x{:x}
      )delim", offset, spi->data_offset());
    }
    offset += spi->data_size();
  }

  // Check consistency of Function starts
  if (const FunctionStarts* fs = binary.function_starts()) {
    if (fs->data_offset() != 0 && fs->data_offset() != offset) {
      return error(R"delim(
      LC_FUNCTION_STARTS out of place:
        Expecting offset: 0x{:x} while it is 0x{:x}
      )delim", offset, fs->data_offset());
    }
    offset += fs->data_size();
  }

  // Check consistency of Data in Code
  if (const DataInCode* dic = binary.data_in_code()) {
    if (dic->data_offset() != 0 && dic->data_offset() != offset) {
      return error(R"delim(
      LC_DATA_IN_CODE out of place:
        Expecting offset: 0x{:x} while it is 0x{:x}
      )delim", offset, dic->data_offset());
    }
    offset += dic->data_size();
  }

  if (const AtomInfo* info = binary.atom_info()) {
    if (info->data_offset() != 0 && info->data_offset() != offset) {
      return error(R"delim(
      LC_ATOM_INFO out of place:
        Expecting offset: 0x{:x} while it is 0x{:x}
      )delim", offset, info->data_offset());
    }
    offset += info->data_size();
  }

  if (const CodeSignatureDir* cs = binary.code_signature_dir()) {
    if (cs->data_offset() != 0 && cs->data_offset() != offset) {
      return error(R"delim(
      LC_DYLIB_CODE_SIGN_DRS out of place:
        Expecting offset: 0x{:x} while it is 0x{:x}
      )delim", offset, cs->data_offset());
    }
    offset += cs->data_size();
  }

  if (const LinkerOptHint* opt = binary.linker_opt_hint()) {
    if (opt->data_offset() != 0 && opt->data_offset() != offset) {
      return error(R"delim(
      LC_LINKER_OPTIMIZATION_HINT out of place:
        Expecting offset: 0x{:x} while it is 0x{:x}
      )delim", offset, opt->data_offset());
    }
    offset += opt->data_size();
  }

  const SymbolCommand* st = binary.symbol_command();
  if (st == nullptr) {
    return error("LC_SYMTAB not found!");
  }

  if (st->numberof_symbols() != 0) {
    // Check offset
    if (st->symbol_offset() != offset) {
      return error(R"delim(
      LC_SYMTAB.nlist out of place:
        Expecting offset: 0x{:x} while it is 0x{:x}
      )delim", offset, st->symbol_offset());
    }
    offset += st->numberof_symbols() * (is64 ? sizeof(details::nlist_64) : sizeof(details::nlist_32));
  }

  size_t isym = 0;

  if (dyst->nb_local_symbols() != 0) {
    if (isym != dyst->idx_local_symbol()) {
      return error(R"delim(
      LC_DYSYMTAB.nlocalsym out of place:
        Expecting index: {} while it is {}
      )delim", isym, dyst->idx_local_symbol());
    }
    isym += dyst->nb_local_symbols();
  }


  if (dyst->nb_external_define_symbols() != 0) {
    if (isym != dyst->idx_external_define_symbol()) {
      return error(R"delim(
      LC_DYSYMTAB.iextdefsym out of place:
        Expecting index: {} while it is {}
      )delim", isym, dyst->idx_external_define_symbol());
    }
    isym += dyst->nb_external_define_symbols();
  }

  if (dyst->nb_undefined_symbols() != 0) {
    if (isym != dyst->idx_undefined_symbol()) {
      return error(R"delim(
      LC_DYSYMTAB.nundefsym out of place:
        Expecting index: {} while it is {}
      )delim", isym, dyst->idx_undefined_symbol());
    }
    isym += dyst->nb_undefined_symbols();
  }


  if (const TwoLevelHints* two = binary.two_level_hints()) {
    if (two->offset() != 0 && two->offset() != offset) {
      return error(R"delim(
      LC_TWOLEVEL_HINTS out of place:
        Expecting offset: 0x{:x} while it is 0x{:x}
      )delim", offset, two->offset());
    }
    offset += two->hints().size() * sizeof(details::twolevel_hint);
  }


  if (dyst->nb_external_relocations() != 0) {
    if (dyst->external_relocation_offset() != offset) {
      return error(R"delim(
      LC_DYSYMTAB.extrel out of place:
        Expecting offset: 0x{:x} while it is 0x{:x}
      )delim", offset, dyst->external_relocation_offset());
    }

    offset += dyst->nb_external_relocations() * sizeof(details::relocation_info);
  }


  if (dyst->nb_indirect_symbols() != 0) {
    if (dyst->indirect_symbol_offset() != offset) {
      return error(R"delim(
      LC_DYSYMTAB.nindirect out of place:
        Expecting offset: 0x{:x} while it is 0x{:x}
      )delim", offset, dyst->indirect_symbol_offset());
    }

    offset += dyst->nb_indirect_symbols() * sizeof(uint32_t);
  }

  uint64_t rounded_offset = offset;
  uint64_t input_indirectsym_pad = 0;
  if (is64 && (dyst->nb_indirect_symbols() % 2) != 0) {
    rounded_offset = rnd(offset, 8);
  }

  if (dyst->toc_offset() != 0) {
    if (dyst->toc_offset() != offset && dyst->toc_offset() != rounded_offset) {
      return error(R"delim(
      LC_DYSYMTAB.toc out of place:
        Expecting offsets: 0x{:x} or 0x{:x} while it is 0x{:x}
      )delim", offset, rounded_offset, dyst->toc_offset());
    }
    if (dyst->toc_offset() == offset) {
      offset        += dyst->nb_toc() * sizeof(details::dylib_table_of_contents);
      rounded_offset = offset;
    }
    else if (dyst->toc_offset() == rounded_offset) {
      input_indirectsym_pad = rounded_offset - offset;

      rounded_offset += dyst->nb_toc() * sizeof(details::dylib_table_of_contents);
      offset          = rounded_offset;
    }
  }


  if (dyst->nb_module_table() != 0) {
    if (dyst->module_table_offset() != offset && dyst->module_table_offset() != rounded_offset) {
      return error(R"delim(
      LC_DYSYMTAB.modtab out of place:
        Expecting offsets: 0x{:x} or 0x{:x} while it is 0x{:x}
      )delim", offset, rounded_offset, dyst->module_table_offset());
    }

    if (is64) {
      if (dyst->module_table_offset() == offset) {
        offset        += dyst->nb_module_table() * sizeof(details::dylib_module_64);
        rounded_offset = offset;
      }
      else if (dyst->module_table_offset() == rounded_offset) {
        input_indirectsym_pad = rounded_offset - offset;
        rounded_offset += dyst->nb_module_table() * sizeof(details::dylib_module_64);
        offset         = rounded_offset;
      }
    } else {
      offset        += dyst->nb_module_table() * sizeof(details::dylib_module_32);
      rounded_offset = offset;
    }
  }


  if (dyst->nb_external_reference_symbols() != 0) {
    if (dyst->external_reference_symbol_offset() != offset && dyst->external_reference_symbol_offset() != rounded_offset) {
      return error(R"delim(
      LC_DYSYMTAB.extrefsym out of place:
        Expecting offsets: 0x{:x} or 0x{:x} while it is 0x{:x}
      )delim", offset, rounded_offset, dyst->external_reference_symbol_offset());
    }

    if (dyst->external_reference_symbol_offset() == offset) {
      offset        += dyst->nb_external_reference_symbols() * sizeof(details::dylib_reference);
      rounded_offset = offset;
    }
    else if (dyst->external_reference_symbol_offset() == rounded_offset) {
      input_indirectsym_pad = rounded_offset - offset;
      rounded_offset += dyst->nb_external_reference_symbols() * sizeof(details::dylib_reference);
      offset         = rounded_offset;
    }
  }


  if (st->strings_size() != 0) {
    if (st->strings_offset() != offset && st->strings_offset() != rounded_offset) {
      return error(R"delim(
      LC_SYMTAB.strings out of place:
        Expecting offsets: 0x{:x} or 0x{:x} while it is 0x{:x}
      )delim", offset, rounded_offset, st->strings_offset());
    }


    if (st->strings_offset() == offset) {
      offset        += st->strings_size();
      rounded_offset = offset;
    }
    else if (st->strings_offset() == rounded_offset) {
      input_indirectsym_pad = rounded_offset - offset;
      rounded_offset += st->strings_size();
      offset         = rounded_offset;
    }
  }

  if (const CodeSignature* cs = binary.code_signature()) {
    rounded_offset = align(rounded_offset, 16);
    if (cs->data_offset() != rounded_offset) {
      return error(R"delim(
      LC_CODE_SIGNATURE out of place:
        Expecting offsets: 0x{:x} while it is 0x{:x}
      )delim", offset, cs->data_offset());
    }
    rounded_offset += cs->data_size();
    offset = rounded_offset;
  }

  LIEF_DEBUG("input_indirectsym_pad: {:x}", input_indirectsym_pad);
  const uint64_t object_size = linkedit->file_offset() + linkedit->file_size();
  if (offset != object_size && rounded_offset != object_size) {
    return error(R"delim(
    __LINKEDIT.end (0x{:x}) does not match 0x{:x} nor 0x{:x}
    )delim", object_size, offset, rounded_offset);
  }
  return true;
}

bool LayoutChecker::check_chained_fixups() {
  static constexpr auto DYLD_CHAINED_PTR_START_NONE  = 0xFFFF;
  static constexpr auto DYLD_CHAINED_PTR_START_MULTI = 0x8000;
  const DyldChainedFixups* fixups = binary.dyld_chained_fixups();
  if (fixups == nullptr) {
    return true;
  }

  if (fixups->fixups_version() != 0) {
    return error("chained fixups, unknown header version ({})",
                 fixups->fixups_version());
  }

  if (fixups->starts_offset() >= fixups->data_size()) {
    return error("chained fixups, starts_offset exceeds LC_DYLD_CHAINED_FIXUPS size");
  }

  if (fixups->imports_offset() >= fixups->data_size()) {
    return error("chained fixups, starts_offset exceeds LC_DYLD_CHAINED_FIXUPS size");
  }

  uint32_t fmt_entry_size = 0;
  switch (fixups->imports_format()) {
    case DYLD_CHAINED_FORMAT::IMPORT:
      fmt_entry_size = sizeof(details::dyld_chained_import);
      break;

    case DYLD_CHAINED_FORMAT::IMPORT_ADDEND:
      fmt_entry_size = sizeof(details::dyld_chained_import_addend);
      break;

    case DYLD_CHAINED_FORMAT::IMPORT_ADDEND64:
      fmt_entry_size = sizeof(details::dyld_chained_import_addend64);
      break;
    default:
      return error("chained fixups, unknown imports_format: {}",
                   (int)fixups->imports_format());
  }

  if (greater_than_add_or_overflow<uint32_t>(fixups->imports_offset(),
        fmt_entry_size * fixups->imports_count(), fixups->symbols_offset()))
  {
    return error("chained fixups, imports array overlaps symbols");
  }

  if (fixups->symbols_format() != 0) {
    return error("chained fixups, symbols_format unknown ({})",
                 fixups->symbols_format());
  }

  const auto starts_info = fixups->chained_starts_in_segments();
  const size_t nb_segments = binary.segments().size();
  const SegmentCommand& last_seg = binary.segments()[nb_segments - 1];

  if (starts_info.size() != nb_segments) {
    if (starts_info.size() > binary.segments().size()) {
      return error("chained fixups, seg_count exceeds number of segments");
    }
    if (last_seg.name() != "__CTF") {
      return error("chained fixups, seg_count does not match number of segments");
    }
  }
  const uint64_t end_of_start = fixups->imports_offset();
  uint32_t max_valid_pointer_seen = 0;
  size_t idx = 0;
  result<DYLD_CHAINED_PTR_FORMAT> ptr_format_for_all = make_error_code(lief_errors::not_found);

  for (const DyldChainedFixups::chained_starts_in_segment& seg_start : starts_info) {
    if (seg_start.offset == 0) {
      continue;
    }

    if (seg_start.size > end_of_start - seg_start.offset) {
      return error("chained fixups, dyld_chained_starts_in_segment for segment "
                   "#{} overruns imports table", idx);
    }

    if (seg_start.page_size != 0x1000 && seg_start.page_size != 0x4000) {
      return error("chained fixups, page_size not 4KB or 16KB in segment "
                   "#{} (page_size: 0x{:04x})", idx, seg_start.page_size);
    }

    if ((int)seg_start.pointer_format > (int)DYLD_CHAINED_PTR_FORMAT::PTR_ARM64E_SHARED_CACHE) {
      return error("chained fixups, unknown pointer_format in segment #{}", idx);
    }

    if (!ptr_format_for_all) {
      ptr_format_for_all = seg_start.pointer_format;
    }

    if (seg_start.pointer_format != ptr_format_for_all) {
      return error("chained fixups, pointer_format not same for all segments {} and {}",
                   to_string(seg_start.pointer_format), to_string(*ptr_format_for_all));
    }

    if (seg_start.max_valid_pointer != 0) {
      if (max_valid_pointer_seen == 0) {
        max_valid_pointer_seen = seg_start.max_valid_pointer;
      }
      else if (max_valid_pointer_seen != seg_start.max_valid_pointer) {
        return error("chained fixups, different max_valid_pointer values seen in different segments");
      }
    }
    if (seg_start.page_start.size() * sizeof(uint16_t) > seg_start.size) {
      return error("chained fixups, page_start array overflows size");
    }

    [[maybe_unused]] uint32_t max_overflow_idx =
        (seg_start.size - sizeof(details::dyld_chained_starts_in_segment)) / sizeof(uint16_t);

    for (size_t page_idx = 0; page_idx < seg_start.page_count(); ++page_idx) {
      uint16_t offset_in_page = seg_start.page_start[page_idx];
      if (offset_in_page == DYLD_CHAINED_PTR_START_NONE) {
        continue;
      }

      if ((offset_in_page & DYLD_CHAINED_PTR_START_MULTI) == 0) {
        if (offset_in_page > seg_start.page_size) {
          return error("chained fixups, in segment #{} page_start[{}]=0x{:04x} exceeds page size",
                       idx, page_idx, offset_in_page);
        }
      }
      else {
        // TODO(romain): to implement
      }
    }
  }

  return true;
}


bool LayoutChecker::check_function_variants() {
  using KIND = FunctionVariants::RuntimeTable::KIND;
  const FunctionVariants* variants = binary.function_variants();
  if (variants == nullptr) {
    return true;
  }

  for (const FunctionVariants::RuntimeTable& entry : variants->runtime_table()) {
    switch (entry.kind()) {
      case KIND::ARM64:
      case KIND::PER_PROCESS:
      case KIND::SYSTEM_WIDE:
      case KIND::X86_64:
        break;
      default:
        return error("unknown FunctionVariants::RuntimeTable::KIND ({})",
            (uint32_t)entry.kind());
    }

    // Check that the last entry is "default"
    auto entries = entry.entries();
    if (entries.empty()) {
      return error("Missing entries in FunctionVariants::RuntimeTable (offset=0x{:06x})", entry.offset());
    }

    const FunctionVariants::RuntimeTableEntry& last = entries[entries.size() - 1];
    if (last.flag_bit_nums()[0] != 0) {
      return error("last entry in FunctionVariants::RuntimeTable entries is not 'default'");
    }
  }
  return true;
}

bool check_layout(const FatBinary& fat, std::string* error) {
  bool is_ok = true;
  for (Binary& bin : fat) {
    LayoutChecker checker(bin);
    if (!checker.check()) {
      is_ok = false;
      if (error) { *error += checker.get_error() + '\n'; }
    }
  }
  return is_ok;
}


bool check_layout(const Binary& binary, std::string* error) {
  LayoutChecker checker(binary);
  if (!checker.check()) {
    if (error) { *error += checker.get_error() + '\n'; }
    return false;
  }
  return true;
}


}
