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
#include "LIEF/ELF/Binary.hpp"
#include "LIEF/ELF/Header.hpp"
#include "LIEF/ELF/Segment.hpp"
#include "LIEF/ELF/Section.hpp"
#include "LIEF/ELF/Symbol.hpp"
#include "LIEF/ELF/DynamicEntry.hpp"
#include "LIEF/ELF/Note.hpp"
#include "LIEF/ELF/utils.hpp"
#include "LIEF/ELF/SysvHash.hpp"
#include "ELF/Structures.hpp"

#include <spdlog/fmt/fmt.h>
#include <algorithm>


namespace LIEF::ELF {

class LayoutChecker {
  public:
  static constexpr uint32_t DEFAULT_PAGE_SIZE = 0x1000;

  LayoutChecker() = delete;
  LayoutChecker(const Binary& bin) :
    elf(bin),
    filesz(elf.original_size()) {}

  bool check_header();
  bool check_segments();
  bool check_sections();
  bool check_dynamic();
  bool check_notes();
  bool check_tls();
  bool check_mips_rld_map();

  bool check() {
    if (!check_header()) {
      return false;
    }
    if (!check_segments()) {
      return false;
    }
    if (!check_sections()) {
      return false;
    }
    if (!check_dynamic()) {
      return false;
    }
    if (!check_mips_rld_map()) {
      return false;
    }
    if (!check_notes()) {
      return false;
    }
    if (!check_tls()) {
      return false;
    }
    return true;
  }

  bool error(std::string msg) {
    error_msg = std::move(msg);
    return false;
  }

  template<typename... Args>
  bool error(const char* fmt, const Args&... args) {
    error_msg = fmt::format(fmt::runtime(fmt), args...);
    return false;
  }

  const std::string& get_error() const {
    return error_msg;
  }

  bool is64() const {
    return elf.header().identity_class() == Header::CLASS::ELF64;
  }

  private:
  std::string error_msg;
  const Binary& elf;
  uint64_t filesz;
};

bool LayoutChecker::check_header() {
  const Header& hdr = elf.header();

  const uint32_t ehdr_size =
      is64() ? sizeof(details::Elf64_Ehdr) : sizeof(details::Elf32_Ehdr);
  if (filesz < ehdr_size) {
    return error("File size ({:#x}) is too small for ELF header ({:#x})", filesz,
                 ehdr_size);
  }

  if (hdr.identity_version() != Header::VERSION::CURRENT) {
    return error("Invalid ELF version: {}", (int)hdr.identity_version());
  }

  if (hdr.identity_class() == Header::CLASS::NONE) {
    return error("Invalid ELF class");
  }

  if (hdr.file_type() == Header::FILE_TYPE::NONE) {
    return error("Invalid ELF type (ET_NONE)");
  }

  const uint32_t phdr_size =
      is64() ? sizeof(details::Elf64_Phdr) : sizeof(details::Elf32_Phdr);
  const uint32_t phnum = hdr.numberof_segments();
  if (phnum > 0) {
    const uint64_t phoff = hdr.program_headers_offset();
    const uint64_t pht_size = (uint64_t)phnum * phdr_size;

    if (phoff + pht_size > filesz) {
      return error(
          "Program header table (offset: {:#x}, size: {:#x}) is beyond file size",
          phoff, pht_size
      );
    }

    // Required by Android
    if (phoff % elf.ptr_size() != 0) {
      return error("phoff offset ({:#010x}) misaligned", phoff);
    }

    if (phnum > (std::numeric_limits<uint16_t>::max() / phdr_size)) {
      return error("Too many program headers: {}", phnum);
    }
  }

  const uint32_t shdr_size =
      is64() ? sizeof(details::Elf64_Shdr) : sizeof(details::Elf32_Shdr);
  const uint32_t shnum = hdr.numberof_sections();
  if (shnum > 0) {
    const uint64_t shoff = hdr.section_headers_offset();
    const uint64_t sht_size = (uint64_t)shnum * shdr_size;
    if (shoff + sht_size > filesz) {
      return error(
          "Section header table (offset: {:#x}, size: {:#x}) is beyond file size",
          shoff, sht_size
      );
    }

    // Required by Android
    if (shoff % elf.ptr_size() != 0) {
      return error("shdr offset ({:#010x}) misaligned", shoff);
    }

    if (hdr.section_name_table_idx() >= shnum) {
      return error("Invalid section name table index: {}",
                   hdr.section_name_table_idx());
    }
  }

  // Android specific checks if targeting Android
  if (elf.is_targeting_android()) {
    if (hdr.identity_data() != Header::ELF_DATA::LSB) {
      return error("Android ELF must be little-endian");
    }
    if (hdr.file_type() != Header::FILE_TYPE::DYN) {
      return error("Android ELF must be ET_DYN (PIE or shared library)");
    }
  }

  return true;
}

bool LayoutChecker::check_segments() {
  bool has_pt_load = false;
  auto segments = elf.segments();

  const Segment* pt_phdr = nullptr;

  for (size_t i = 0; i < segments.size(); ++i) {
    const Segment& seg = segments[i];

    if (seg.file_offset() + seg.physical_size() > filesz) {
      return error("Segment[{}] offset ({:#x}) + filesz ({:#x}) is beyond file "
                   "size ({:#x})",
                   i, seg.file_offset(), seg.physical_size(), filesz);
    }

    if (seg.virtual_size() < seg.physical_size()) {
      return error("Segment[{}] memsz ({:#x}) is smaller than filesz ({:#x})", i,
                   seg.virtual_size(), seg.physical_size());
    }

    const uint64_t alignment = seg.alignment();
    if (alignment > 1 && (alignment & (alignment - 1)) != 0) {
      return error("Segment[{}] alignment ({:#x}) is not a power of 2", i,
                   alignment);
    }

    if (seg.type() == Segment::TYPE::PHDR) {
      pt_phdr = &seg;
    }

    if (seg.type() == Segment::TYPE::LOAD) {
      has_pt_load = true;
      if (alignment > 1) {
        if ((seg.virtual_address() % alignment) != (seg.file_offset() % alignment))
        {
          return error(
              "Segment[{}] (PT_LOAD) p_vaddr ({:#x}) and p_offset ({:#x}) "
              "are not congruent modulo p_align ({:#x})",
              i, seg.virtual_address(), seg.file_offset(), alignment
          );
        }
      }

      // Android W+E check
      if (elf.is_targeting_android()) {
        if (seg.has(Segment::FLAGS::W) && seg.has(Segment::FLAGS::X)) {
          return error("Segment[{}] (PT_LOAD) is both writable and executable", i);
        }
      }

      // Overlap check for PT_LOAD
      for (size_t j = i + 1; j < segments.size(); ++j) {
        const Segment& other = segments[j];
        if (other.type() != Segment::TYPE::LOAD) {
          continue;
        }

        const uint64_t start1 = seg.virtual_address();
        const uint64_t end1 = seg.virtual_address() + seg.virtual_size();
        const uint64_t start2 = other.virtual_address();
        const uint64_t end2 = other.virtual_address() + other.virtual_size();

        if (std::max(start1, start2) < std::min(end1, end2)) {
          return error("Segment[{}] and Segment[{}] (PT_LOAD) overlap in memory",
                       i, j);
        }
      }
    }
  }

  if (!has_pt_load) {
    return error("No loadable segments (PT_LOAD)");
  }

  // PT_PHDR must be wrapped by a PT_LOAD
  if (pt_phdr != nullptr) {
    bool wrapped = false;
    for (const Segment& seg : segments) {
      if (seg.type() != Segment::TYPE::LOAD) {
        continue;
      }
      if (seg.virtual_address() <= pt_phdr->virtual_address() &&
          (pt_phdr->virtual_address() + pt_phdr->virtual_size()) <=
              (seg.virtual_address() + seg.physical_size()))
      {
        wrapped = true;
        break;
      }
    }
    if (!wrapped) {
      return error("PT_PHDR segment is not wrapped by a PT_LOAD segment");
    }
  } else {
    // If no PT_PHDR, program headers are usually at the beginning of the first
    // PT_LOAD (offset 0) Linker checks this in FindPhdr()
    bool found_pht_in_load = false;
    const uint64_t pht_offset = elf.header().program_headers_offset();
    const uint32_t phdr_size =
        is64() ? sizeof(details::Elf64_Phdr) : sizeof(details::Elf32_Phdr);
    const uint64_t pht_size =
        (uint64_t)elf.header().numberof_segments() * phdr_size;

    for (const Segment& seg : segments) {
      if (seg.type() != Segment::TYPE::LOAD) {
        continue;
      }
      if (seg.file_offset() <= pht_offset &&
          (pht_offset + pht_size) <= (seg.file_offset() + seg.physical_size()))
      {
        found_pht_in_load = true;
        break;
      }
    }
    if (!found_pht_in_load) {
      return error("Program header table is not wrapped by a PT_LOAD segment");
    }
  }

  return true;
}

bool LayoutChecker::check_sections() {
  const uint64_t filesz = elf.original_size();
  auto sections = elf.sections();
  const Section* dynamic_sec = nullptr;
  const Section* dynsym_sec = nullptr;

  for (size_t i = 0; i < sections.size(); ++i) {
    const Section& sec = sections[i];
    if (sec.type() == Section::TYPE::DYNAMIC) {
      dynamic_sec = &sec;
    }

    if (sec.type() == Section::TYPE::DYNSYM) {
      if (dynsym_sec != nullptr) {
        return error("Multiple SHT_DYNSYM sections found");
      }
      dynsym_sec = &sec;
    }

    if (sec.type() == Section::TYPE::NOBITS) {
      continue;
    }

    if (sec.offset() + sec.size() > filesz) {
      return error("Section[{}] ('{}') offset ({:#x}) + size ({:#x}) is beyond "
                   "file size ({:#x})",
                   i, sec.name(), sec.offset(), sec.size(), filesz);
    }

    // sh_entsize checks
    uint64_t expected_entsize = 0;
    switch (sec.type()) {
      case Section::TYPE::SYMTAB:
      case Section::TYPE::DYNSYM:
        expected_entsize =
            is64() ? sizeof(details::Elf64_Sym) : sizeof(details::Elf32_Sym);
        break;
      case Section::TYPE::DYNAMIC:
        expected_entsize =
            is64() ? sizeof(details::Elf64_Dyn) : sizeof(details::Elf32_Dyn);
        break;
      case Section::TYPE::REL:
        expected_entsize =
            is64() ? sizeof(details::Elf64_Rel) : sizeof(details::Elf32_Rel);
        break;
      case Section::TYPE::RELA:
        expected_entsize =
            is64() ? sizeof(details::Elf64_Rela) : sizeof(details::Elf32_Rela);
        break;
      case Section::TYPE::GROUP:
      case Section::TYPE::SYMTAB_SHNDX: expected_entsize = sizeof(uint32_t); break;
      default: break;
    }

    if (expected_entsize > 0 && sec.entry_size() != expected_entsize) {
      return error(
          "Section[{}] ('{}') has invalid sh_entsize: {:#x} (expected: {:#x})", i,
          sec.name(), sec.entry_size(), expected_entsize
      );
    }

    if (sec.entry_size() > 0 && (sec.size() % sec.entry_size()) != 0) {
      return error("Section[{}] ('{}') size ({:#x}) is not a multiple of "
                   "sh_entsize ({:#x})",
                   i, sec.name(), sec.size(), sec.entry_size());
    }

    // sh_link checks
    switch (sec.type()) {
      case Section::TYPE::DYNAMIC:
      case Section::TYPE::SYMTAB:
      case Section::TYPE::DYNSYM:
        if (sec.link() >= sections.size()) {
          return error("Section[{}] ('{}') has invalid sh_link: {}", i, sec.name(),
                       sec.link());
        }
        {
          const Section& strtab = sections[sec.link()];
          if (strtab.type() != Section::TYPE::STRTAB) {
            return error("Section[{}] ('{}') has invalid sh_link: {} (expected "
                         "SHT_STRTAB, but got {})",
                         i, sec.name(), sec.link(), to_string(strtab.type()));
          }
        }
        break;
      case Section::TYPE::REL:
      case Section::TYPE::RELA:
      case Section::TYPE::HASH:
      case Section::TYPE::GNU_HASH:
        if (sec.link() >= sections.size()) {
          return error("Section[{}] ('{}') has invalid sh_link: {}", i, sec.name(),
                       sec.link());
        }
        break;
      default: break;
    }

    // sh_info checks
    switch (sec.type()) {
      case Section::TYPE::SYMTAB:
      case Section::TYPE::DYNSYM:
        if (sec.entry_size() > 0 &&
            sec.information() > sec.size() / sec.entry_size())
        {
          return error("Section[{}] ('{}') has invalid sh_info: {} (larger than "
                       "symbol count {})",
                       i, sec.name(), sec.information(),
                       sec.size() / sec.entry_size());
        }
        break;
      default: break;
    }
  }

  if (const Segment* pt_dynamic = elf.get(Segment::TYPE::DYNAMIC)) {
    if (dynamic_sec != nullptr) {
      if (dynamic_sec->offset() != pt_dynamic->file_offset()) {
        return error("SHT_DYNAMIC section header and PT_DYNAMIC program header "
                     "disagree about "
                     "the location of the dynamic table: section offset {:#x} vs "
                     "segment offset {:#x}",
                     dynamic_sec->offset(), pt_dynamic->file_offset());
      }
      if (dynamic_sec->size() != pt_dynamic->physical_size()) {
        return error("SHT_DYNAMIC section header and PT_DYNAMIC program header "
                     "disagree about "
                     "the size of the dynamic table: section size {:#x} vs "
                     "segment size {:#x}",
                     dynamic_sec->size(), pt_dynamic->physical_size());
      }
    }

    // Android specific: .dynamic section header must exist if PT_DYNAMIC exists
    if (elf.is_targeting_android()) {
      if (dynamic_sec == nullptr) {
        return error(".dynamic section header was not found");
      }
      if (dynamic_sec->link() >= sections.size()) {
        return error(".dynamic section has invalid sh_link: {}",
                     dynamic_sec->link());
      }
      const Section& strtab = sections[dynamic_sec->link()];
      if (strtab.type() != Section::TYPE::STRTAB) {
        return error(".dynamic section has invalid link({}) sh_type: {} (expected "
                     "SHT_STRTAB)",
                     dynamic_sec->link(), (uint32_t)strtab.type());
      }
    }
  }

  return true;
}

bool LayoutChecker::check_dynamic() {
  const bool is64 = this->is64();
  uint64_t relaent = 0;
  uint64_t relent = 0;
  uint64_t relasz = 0;
  uint64_t relsz = 0;
  uint64_t syment = 0;
  uint64_t strsz = 0;
  uint64_t strtab = 0;
  uint64_t symtab = 0;

  for (const DynamicEntry& entry : elf.dynamic_entries()) {
    switch (entry.tag()) {
      case DynamicEntry::TAG::RELAENT: relaent = entry.value(); break;
      case DynamicEntry::TAG::RELENT: relent = entry.value(); break;
      case DynamicEntry::TAG::RELASZ: relasz = entry.value(); break;
      case DynamicEntry::TAG::RELSZ: relsz = entry.value(); break;
      case DynamicEntry::TAG::SYMENT: syment = entry.value(); break;
      case DynamicEntry::TAG::STRSZ: strsz = entry.value(); break;
      case DynamicEntry::TAG::STRTAB: strtab = entry.value(); break;
      case DynamicEntry::TAG::SYMTAB: symtab = entry.value(); break;
      case DynamicEntry::TAG::PLTREL:
      {
        uint64_t val = entry.value();
        if (val != (uint64_t)DynamicEntry::TAG::REL &&
            val != (uint64_t)DynamicEntry::TAG::RELA)
        {
          return error("Invalid DT_PLTREL value: {}", val);
        }
        break;
      }
      default: break;
    }
  }

  if (relaent > 0) {
    const uint64_t expected =
        is64 ? sizeof(details::Elf64_Rela) : sizeof(details::Elf32_Rela);
    if (relaent != expected) {
      return error("DT_RELAENT ({:#x}) does not match expected size ({:#x})",
                   relaent, expected);
    }
  }
  if (relent > 0) {
    const uint64_t expected =
        is64 ? sizeof(details::Elf64_Rel) : sizeof(details::Elf32_Rel);
    if (relent != expected) {
      return error("DT_RELENT ({:#x}) does not match expected size ({:#x})",
                   relent, expected);
    }
  }

  if (relasz > 0 && relaent > 0 && (relasz % relaent) != 0) {
    return error("DT_RELASZ ({:#x}) is not a multiple of DT_RELAENT ({:#x})",
                 relasz, relaent);
  }

  if (relsz > 0 && relent > 0 && (relsz % relent) != 0) {
    return error("DT_RELSZ ({:#x}) is not a multiple of DT_RELENT ({:#x})", relsz,
                 relent);
  }

  if (syment > 0) {
    const uint64_t expected =
        is64 ? sizeof(details::Elf64_Sym) : sizeof(details::Elf32_Sym);
    if (syment != expected) {
      return error("DT_SYMENT ({:#x}) does not match expected size ({:#x})",
                   syment, expected);
    }
  }

  if (strtab > 0 && strsz > 0) {
    if (result<uint64_t> res = elf.virtual_address_to_offset(strtab)) {
      uint64_t offset = *res;
      if (offset + strsz > filesz) {
        return error("Dynamic string table (offset: {:#x}, size: {:#x}) goes "
                     "past the end of the file (size: {:#x})",
                     offset, strsz, filesz);
      }
    }
  }

  if (symtab > 0) {
    const Section* dynsym = elf.get(Section::TYPE::DYNSYM);
    if (dynsym != nullptr && dynsym->virtual_address() != symtab) {
      return error("SHT_DYNSYM section header and DT_SYMTAB disagree about "
                   "the location of the dynamic symbol table: section VA {:#x} "
                   "vs dynamic VA {:#x}",
                   dynsym->virtual_address(), symtab);
    }
  }

  if (elf.use_sysv_hash()) {
    const SysvHash* hash = elf.sysv_hash();
    const Section* dynsym = elf.get(Section::TYPE::DYNSYM);
    if (dynsym != nullptr && dynsym->entry_size() > 0) {
      uint64_t dynsym_count = dynsym->size() / dynsym->entry_size();
      if (hash->nchain() != dynsym_count) {
        return error("hash table nchain ({}) differs from symbol count derived "
                     "from SHT_DYNSYM section header ({})",
                     hash->nchain(), dynsym_count);
      }
    }
  }

  return true;
}

bool LayoutChecker::check_mips_rld_map() {
  // DT_MIPS_RLD_MAP_REL stores the offset of the run-time loader map
  // **relative** to the address of its own dynamic entry.
  // When `DT_MIPS_RLD_MAP` is also present (absolute version)
  // both tags must reference the same address
  const bool is64 = this->is64();

  const DynamicEntry* rld_map_rel = elf.get(DynamicEntry::TAG::MIPS_RLD_MAP_REL);
  if (rld_map_rel == nullptr) {
    return true;
  }
  const DynamicEntry* rld_map = elf.get(DynamicEntry::TAG::MIPS_RLD_MAP);
  const Segment* pt_dynamic = elf.get(Segment::TYPE::DYNAMIC);

  if (rld_map == nullptr || pt_dynamic == nullptr) {
    return true;
  }

  const uint64_t dynent_size =
      is64 ? sizeof(details::Elf64_Dyn) : sizeof(details::Elf32_Dyn);

  uint64_t index = 0;
  for (const DynamicEntry& entry : elf.dynamic_entries()) {
    if (entry.tag() == DynamicEntry::TAG::MIPS_RLD_MAP_REL) {
      break;
    }
    ++index;
  }

  const uint64_t entry_va = pt_dynamic->virtual_address() + index * dynent_size;
  uint64_t resolved = entry_va + rld_map_rel->value();
  if (!is64) {
    resolved &= 0xffffffff;
  }

  if (resolved != rld_map->value()) {
    return error("DT_MIPS_RLD_MAP_REL resolves to {:#x} but DT_MIPS_RLD_MAP "
                 "points to {:#x}",
                 resolved, rld_map->value());
  }
  return true;
}

bool LayoutChecker::check_notes() {
  const uint64_t filesz = elf.original_size();
  for (const Segment& seg : elf.segments()) {
    if (seg.type() != Segment::TYPE::NOTE) {
      continue;
    }
    if (seg.physical_size() == 0) {
      continue;
    }

    if (seg.file_offset() + seg.physical_size() > filesz) {
      return error("PT_NOTE segment runs off end of file");
    }
    if (seg.physical_size() != seg.virtual_size()) {
      return error("PT_NOTE segment p_filesz ({:#x}) != p_memsz ({:#x})",
                   seg.physical_size(), seg.virtual_size());
    }
  }
  return true;
}

bool LayoutChecker::check_tls() {
  const Segment* tls_seg = nullptr;
  for (const Segment& seg : elf.segments()) {
    if (seg.type() == Segment::TYPE::TLS) {
      if (tls_seg != nullptr) {
        return error("Multiple PT_TLS segments");
      }
      tls_seg = &seg;
    }
  }

  if (tls_seg == nullptr) {
    return true;
  }

  for (const Symbol& sym : elf.symbols()) {
    if (sym.type() != Symbol::TYPE::TLS) {
      continue;
    }

    uint64_t addr = sym.value();
    if (addr > tls_seg->virtual_size() ||
        (addr + sym.size()) > tls_seg->virtual_size())
    {
      return error("TLS symbol {:#010}: {} out of PT_TLS segment range"
                   "([{:#010x}, {:#010x}])",
                   sym.value(), sym.name(), 0, tls_seg->virtual_size());
    }
  }

  return true;
}

bool check_layout(const Binary& bin, std::string* error_info) {
  LayoutChecker checker(bin);
  if (!checker.check()) {
    if (error_info != nullptr) {
      *error_info = checker.get_error();
    }
    return false;
  }
  return true;
}

}
