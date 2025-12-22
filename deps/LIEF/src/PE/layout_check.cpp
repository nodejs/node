/* Copyright 2017 - 2023 R. Thomas
 * Copyright 2017 - 2023 Quarkslab
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
#include "LIEF/PE/Binary.hpp"
#include "LIEF/PE/Section.hpp"
#include "LIEF/PE/Relocation.hpp"
#include "LIEF/PE/RelocationEntry.hpp"
#include "LIEF/PE/LoadConfigurations.hpp"
#include "LIEF/PE/TLS.hpp"
#include "LIEF/utils.hpp"
#include "LIEF/PE/utils.hpp"

#include "PE/Structures.hpp"

#include <spdlog/fmt/fmt.h>

namespace LIEF {
namespace PE {

class LayoutChecker {
  public:
  static constexpr uint32_t DEFAULT_PAGE_SIZE = 0x1000;
  static constexpr uint32_t MAX_SIZEOF_IMAGE = 0x77000000;

  LayoutChecker() = delete;
  LayoutChecker(const Binary& bin) :
    pe(bin),
    arch(bin.header().machine()),
    e_lfanew(bin.dos_header().addressof_new_exeheader()),
    filesz(bin.original_size()),
    section_alignment(bin.optional_header().section_alignment()),
    file_alignment(bin.optional_header().file_alignment()),
    sizeof_headers(bin.optional_header().sizeof_headers())
  {}

  bool check_dos_header();
  bool check_header();
  bool check_opt_header();
  bool check_sections();
  bool check_code_integrity();
  bool check_load_config();
  bool check_imports();
  bool check_tls();

  bool is_legacy_arch() const {
    return arch == Header::MACHINE_TYPES::I386 ||
           arch == Header::MACHINE_TYPES::AMD64;
  }

  bool check() {
    if (filesz == 0) {
      return error("Invalid filesz");
    }

    if (!check_dos_header()) {
      return false;
    }

    if (!check_header()) {
      return false;
    }

    if (!check_opt_header()) {
      return false;
    }

    if (!check_sections()) {
      return false;
    }

    if (!check_code_integrity()) {
      return false;
    }

    if (!check_load_config()) {
      return false;
    }

    if (!check_imports()) {
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

  template <typename... Args>
  bool error(const char *fmt, const Args &... args) {
    error_msg = fmt::format(fmt, args...);
    return false;
  }

  const std::string& get_error() const {
    return error_msg;
  }

  uint32_t ptr_size() const {
    return pe.optional_header().magic() == PE_TYPE::PE32 ? sizeof(uint32_t) :
                                                           sizeof(uint64_t);
  }

  bool is_pe64() const {
    return pe.optional_header().magic() == PE_TYPE::PE32_PLUS;
  }

  bool is_pe32() const {
    return pe.optional_header().magic() == PE_TYPE::PE32;
  }

  private:
  std::string error_msg;
  const Binary& pe;
  Header::MACHINE_TYPES arch;
  uint32_t e_lfanew = 0;
  uint64_t filesz = 0;
  uint32_t section_alignment;
  uint32_t file_alignment;
  uint64_t sizeof_headers;
};


bool LayoutChecker::check_dos_header() {
  const DosHeader& dos = pe.dos_header();
  if (dos.magic() != /* MZ */0x5A4D) {
    return error("Invalid DOS Magic");
  }

  if (dos.addressof_new_exeheader() & 3) {
    return error("DOS.e_lfanew is not correctly aligned");
  }

  if (e_lfanew > filesz) {
    return error("DOS.e_lfanew is beyond the file size");
  }

  if (e_lfanew < sizeof(details::pe_dos_header)) {
    return error("DOS.e_lfanew is less than the size of the DOS header");
  }

  if (e_lfanew > section_alignment) {
    return error("DOS.e_lfanew is larger than the section's alignment");
  }
  return true;
}

bool LayoutChecker::check_header() {
  const Header& hdr = pe.header();
  if (arch == Header::MACHINE_TYPES::UNKNOWN &&
      hdr.sizeof_optional_header() == 0)
  {
    return error("Header error: invalid machine type and size of optional header");
  }

  if (!hdr.has_characteristic(Header::CHARACTERISTICS::EXECUTABLE_IMAGE)) {
    return error("Header does not have the EXECUTABLE_IMAGE characteristic");
  }

  // This check is not really enforced by Windows
  if (hdr.numberof_sections() > 50) {
    return error("Invalid number of sections");
  }

  if (pe.type() == PE_TYPE::PE32) {
    if (arch != Header::MACHINE_TYPES::ARMNT &&
        arch != Header::MACHINE_TYPES::I386)
    {
      return error("Invalid Architecture");
    }
  }

  if (pe.type() == PE_TYPE::PE32_PLUS) {
    if (arch != Header::MACHINE_TYPES::IA64 &&
        arch != Header::MACHINE_TYPES::AMD64 &&
        arch != Header::MACHINE_TYPES::ARM64)
    {
      return error("Invalid Architecture");
    }
  }
  if ((hdr.sizeof_optional_header() & 0x07) != 0) {
    return error("Size of OptionalHeader is wrongly aligned");
  }

  return true;
}


bool LayoutChecker::check_opt_header() {
  const OptionalHeader& opt_hdr = pe.optional_header();
  if (is_legacy_arch()) {
    if (!pe.header().has_characteristic(Header::CHARACTERISTICS::RELOCS_STRIPPED)) {
      if (opt_hdr.has(OptionalHeader::DLL_CHARACTERISTICS::APPCONTAINER)) {
        return error("AppContainer with stripped relocations");
      }
    }
  } else {
    if (!pe.header().has_characteristic(Header::CHARACTERISTICS::RELOCS_STRIPPED)) {
      if (!opt_hdr.has(OptionalHeader::DLL_CHARACTERISTICS::DYNAMIC_BASE)) {
        return error("Missing DYNAMIC_BASE characteristic");
      }

      if (!opt_hdr.has(OptionalHeader::DLL_CHARACTERISTICS::NX_COMPAT)) {
        return error("Missing NX_COMPAT characteristic");
      }
    }
  }

  if (section_alignment >= DEFAULT_PAGE_SIZE && sizeof_headers == 0) {
    return error("OptionalHeader.sizeof_headers is null");
  }

  if (file_alignment == 0) {
    return error("File alignment can't be null");
  }

  if (const auto fa = file_alignment; (fa & (fa - 1)) != 0) {
    return error("File alignment must be a power of 2");
  }

  if (section_alignment == 0) {
    return error("Section alignment can't be null");
  }

  if (const auto sa = section_alignment; (sa & (sa - 1)) != 0) {
    return error("Section alignment must be a power of 2");
  }

  if (section_alignment < file_alignment) {
    return error("Section alignment must be larger than file alignment");
  }

  // This check is not accurate (c.f. PE64_x86-64_binary_winhello64-mingw.exe)
  if (0) {
    if (opt_hdr.major_linker_version() < 3 && opt_hdr.major_linker_version() < 5) {
      return error("Bad linker version");
    }
  }

  if ((file_alignment & 0x1ff) != 0 &&
      file_alignment != section_alignment)
  {
    return error("Bad consistency between file/section alignment");
  }

  if (opt_hdr.sizeof_image() > MAX_SIZEOF_IMAGE) {
    return error("Image is too big");
  }

  if (sizeof_headers > opt_hdr.sizeof_image()) {
    return error("Size of Headers is larger than the image size");
  }

  if ((opt_hdr.imagebase() & 0xffff) != 0) {
    return error("Image base is not correctly aligned");
  }

  return true;
}

bool LayoutChecker::check_sections() {
  const auto sections = pe.sections();
  if (sections.empty()) {
    return error("No sections");
  }

  const uint64_t sizeof_image = pe.optional_header().sizeof_image();
  const uint32_t file_alignment_mask = file_alignment - 1;
  uint32_t nb_pages =  (sizeof_image >> 12) + ((sizeof_image & (DEFAULT_PAGE_SIZE - 1)) != 0);
  uint32_t nb_sec_pages = align(sizeof_headers, section_alignment) / DEFAULT_PAGE_SIZE;
  uint64_t next_va = 0;

  if (section_alignment >= DEFAULT_PAGE_SIZE) {
    if ((sizeof_headers + (section_alignment - 1)) < sizeof_headers) {
      return error("Size of header is wrong");
    }

    if (nb_sec_pages > nb_pages) {
      return error("Invalid number of pages");
    }

    next_va += nb_sec_pages * DEFAULT_PAGE_SIZE;
  } else {
    nb_sec_pages = align(sizeof_image, DEFAULT_PAGE_SIZE) / DEFAULT_PAGE_SIZE;
  }

  nb_pages -= nb_sec_pages;

  for (size_t i = 0; i < sections.size(); ++i) {
    const Section& section = sections[i];

    uint32_t offset = section.sizeof_raw_data() != 0 ? section.pointerto_raw_data() : 0;
    [[maybe_unused]] uint32_t end_offset = offset + section.sizeof_raw_data();
    uint32_t vsize =  section.virtual_size() != 0 ? section.virtual_size() :
                                                    section.sizeof_raw_data();

    if (offset + section.size() < offset) {
      return error("Section overflow");
    }

    if (section_alignment < DEFAULT_PAGE_SIZE) {
      if (section.virtual_address() != section.pointerto_raw_data() ||
          section.sizeof_raw_data() < vsize)
      {
        return error("Section size mismatch");
      }
    } else {
      if (next_va != section.virtual_address()) {
        return error("Invalid section virtual address");
      }

      if ((next_va + vsize) <= next_va) {
        return error("Invalid section vsize");
      }

      if ((vsize + (DEFAULT_PAGE_SIZE - 1)) <= vsize) {
        return error("Invalid section vsize");
      }

      nb_sec_pages = align(vsize, section_alignment) / DEFAULT_PAGE_SIZE;
      if (nb_sec_pages > nb_pages) {
        return error("Invalid section vsize");
      }

      nb_pages -= nb_sec_pages;

      if (((offset + section.sizeof_raw_data() + file_alignment_mask) & ~file_alignment_mask) < section.pointerto_raw_data()) {
        return error("Invalid section raw size");
      }

      const bool is_last_section = i == (sections.size() - 1);

      if (is_last_section && section.sizeof_raw_data() != 0) {
        if (section.pointerto_raw_data() + section.sizeof_raw_data() > pe.original_size()) {
          return error("File is cut");
        }
      }

      next_va += nb_sec_pages * DEFAULT_PAGE_SIZE;
    }
  }

  return true;
}

bool LayoutChecker::check_code_integrity() {
  if (section_alignment - e_lfanew <= e_lfanew) {
    return error("{}:{}", __FUNCTION__, __LINE__);
  }

  const size_t sizeof_nt_headers = sizeof(details::pe_header) +
                                   sizeof(details::pe32_optional_header);
  if ((e_lfanew + sizeof_nt_headers) > section_alignment) {
    return error("{}:{}", __FUNCTION__, __LINE__);
  }

  if (pe.optional_header().sizeof_headers() == 0 ||
      pe.optional_header().sizeof_headers() > pe.original_size())
  {
    return error("{}:{}", __FUNCTION__, __LINE__);
  }

  if (file_alignment == 0 || (file_alignment & (file_alignment - 1)) != 0) {
    return error("{}:{}", __FUNCTION__, __LINE__);
  }

  if ((section_alignment & (section_alignment - 1)) != 0) {
    return error("{}:{}", __FUNCTION__, __LINE__);
  }

  if (file_alignment > section_alignment) {
    return error("{}:{}", __FUNCTION__, __LINE__);
  }

  if ((file_alignment & (0x200 - 1)) != 0 && file_alignment != section_alignment) {
    return error("{}:{}", __FUNCTION__, __LINE__);
  }

  uint64_t end_of_raw =
    e_lfanew +
    sizeof(details::pe_header) +
    pe.header().sizeof_optional_header() +
    pe.sections().size() * sizeof(details::pe_section);

  if (end_of_raw >= DEFAULT_PAGE_SIZE) {
    return error("{}:{}", __FUNCTION__, __LINE__);
  }

  for (const Section& sec : pe.sections()) {
    if (pe.type() == LIEF::PE::PE_TYPE::PE32_PLUS) {
      if (sec.pointerto_raw_data() != 0 && sec.sizeof_raw_data() != 0 &&
          sec.pointerto_raw_data() < pe.optional_header().sizeof_headers())
      {
        return error("{}:{}", __FUNCTION__, __LINE__);
      }
    }

    if (sec.pointerto_raw_data() != 0 && sec.pointerto_raw_data() < end_of_raw) {
      return error("{}:{}", __FUNCTION__, __LINE__);
    }

    if ((sec.pointerto_raw_data() + sec.sizeof_raw_data()) < sec.pointerto_raw_data()) {
      return error("{}:{}", __FUNCTION__, __LINE__);
    }

    if ((sec.pointerto_raw_data() + sec.sizeof_raw_data()) > pe.original_size()) {
      return error("{}:{}", __FUNCTION__, __LINE__);
    }

    if ((sec.virtual_address() + sec.sizeof_raw_data() - 1) < sec.sizeof_raw_data()) {
      return error("{}:{}", __FUNCTION__, __LINE__);
    }

    if (sec.sizeof_raw_data() != 0) {
      end_of_raw = std::max<uint64_t>(end_of_raw, sec.pointerto_raw_data() + sec.sizeof_raw_data());
    }
  }
  return true;
}

bool LayoutChecker::check_load_config() {
  const LoadConfiguration* config = pe.load_configuration();
  if (config == nullptr) {
    return true;
  }

  // NOTE(romain): dynamic_value_reloctable_section starts by indexing from 1
  if (auto value = config->dynamic_value_reloctable_section(); value.value_or(0) > 0) {
    if ((*value - 1) < 0 || static_cast<size_t>((*value - 1)) >= pe.sections().size()) {
      return false;
    }
  }

  if (auto value = config->guard_cf_function_table(); value.value_or(0) > 0) {
    if (*value < pe.optional_header().imagebase()) {
      return error("{}:{}", __FUNCTION__, __LINE__);
    }

    if (config->guard_cf_function_count().value_or(0) == 0) {
      return error("{}:{}", __FUNCTION__, __LINE__);
    }
  }

  return true;
}

bool LayoutChecker::check_imports() {
  auto imports = pe.imports();
  if (imports.empty()) {
    return true;
  }

  const DataDirectory* iat_dir = pe.iat_dir();

  for (const Import& imp : imports) {
    for (const ImportEntry& entry : imp.entries()) {
      const uint32_t iat_address = entry.iat_address(); // RVA
      std::string entry_name = entry.is_ordinal() ?
                               fmt::format("#{:04d}", entry.ordinal()) :
                               entry.name();
      // 2 bytes alignment seems sufficient
      if (iat_address % sizeof(uint16_t) != 0) {
        return error("{}:{} IAT is wrongly aligned: 0x{:08x}",
                     imp.name(), entry_name, entry.iat_address());
      }

      if (iat_dir->RVA() == 0 || iat_dir->size() == 0) {
        return error("IAT directory is not set");
      }

      const uint32_t iat_dir_start = iat_dir->RVA();
      const uint32_t iat_dir_end = iat_dir_start + iat_dir->size();

      if (iat_dir_start <= iat_address && iat_address < iat_dir_end) {
        const Section* section = pe.section_from_rva(iat_address);
        if (section == nullptr) {
          return error("Can't find section associated with the IAT "
                       "address: 0x{:08x}", iat_address);
        }
        /* Not a requirement (even though most of the PE have it)
         * if (!section->has_characteristic(Section::CHARACTERISTICS::CNT_INITIALIZED_DATA)) {
         *   return error(R"err(
         *     {}:{} IAT: 0x{:08x} -- Missing CNT_INITIALIZED_DATA in the pointed section ('{}')
         *   )err", imp.name(), entry_name, iat_address, section->name());
         * }
         */

        if (!section->has_characteristic(Section::CHARACTERISTICS::MEM_READ)) {
          return error(R"err(
            {}:{} IAT: 0x{:08x} -- Missing MEM_READ in the pointed section ('{}')
          )err", imp.name(), entry_name, iat_address, section->name());
        }
      } else {
        const Section* section = pe.section_from_rva(iat_address);
        if (section == nullptr) {
          return error("Can't find section associated with the IAT "
                       "address: 0x{:08x}", iat_address);
        }
        /*
         * if (!section->has_characteristic(Section::CHARACTERISTICS::CNT_INITIALIZED_DATA)) {
         *   return error(R"err(
         *     {}:{} IAT: 0x{:08x} -- Missing CNT_INITIALIZED_DATA in the pointed section ('{}')
         *   )err", imp.name(), entry_name, iat_address, section->name());
         * }
         */

        if (!section->has_characteristic(Section::CHARACTERISTICS::MEM_READ)) {
          return error(R"err(
            {}:{} IAT: 0x{:08x} -- Missing MEM_READ in the pointed section ('{}')
          )err", imp.name(), entry_name, iat_address, section->name());
        }

        if (!section->has_characteristic(Section::CHARACTERISTICS::MEM_WRITE)) {
          return error(R"err(
            {}:{} IAT: 0x{:08x} -- Missing MEM_WRITE in the pointed section ('{}')
          )err", imp.name(), entry_name, iat_address, section->name());
        }

        if (!section->has_characteristic(Section::CHARACTERISTICS::CNT_CODE)) {
          return error(R"err(
            {}:{} IAT: 0x{:08x} -- Missing CNT_CODE in the pointed section ('{}')
          )err", imp.name(), entry_name, iat_address, section->name());
        }
      }

    }
  }
  return true;
}

bool LayoutChecker::check_tls() {
  const TLS* tls = pe.tls();
  const DataDirectory* tls_dir = pe.tls_dir();

  if (tls == nullptr) {
    return true;
  }
  const size_t ptr_size = this->ptr_size();
  const uint64_t imagebase = pe.optional_header().imagebase();

  const Section* tls_sec = tls_dir->section();
  if (tls_sec == nullptr) {
    return error("Missing section associated with the TLS data directory");
  }

  if (!tls_sec->has_characteristic(Section::CHARACTERISTICS::MEM_READ)) {
    return error("Missing MEM_READ for TLS section: '{}'", tls_sec->name());
  }

  if (!tls_sec->has_characteristic(Section::CHARACTERISTICS::CNT_INITIALIZED_DATA)) {
    return error("Missing CNT_INITIALIZED_DATA for TLS section: '{}'", tls_sec->name());
  }

  if (tls->addressof_index() == 0) {
    return error("Missing address of index in the TLS structure");
  }

  for (uint64_t addr : tls->callbacks()) {
    if (addr <= imagebase) {
      return error("TLS callback: 0x{:016x} not in imagebase range", addr);
    }
    const uint64_t rva = addr - imagebase;
    const Section* sec = pe.section_from_rva(rva);
    if (sec == nullptr) {
      return error("Can't find section associated with TLS callback: 0x{:016x}", addr);
    }

    if (!sec->has_characteristic(Section::CHARACTERISTICS::MEM_READ)) {
      return error("Missing MEM_READ for TLS callback: 0x{:016x} ('{}')", addr,
                   sec->name());
    }

    if (!sec->has_characteristic(Section::CHARACTERISTICS::MEM_EXECUTE)) {
      return error("Missing MEM_EXECUTE for TLS callback: 0x{:016x} ('{}')", addr,
                   sec->name());
    }
  }

  const uint64_t addr_cbk = tls->addressof_callbacks();
  int64_t addr_cbk_rva = addr_cbk - imagebase;

  if (addr_cbk > 0 && addr_cbk_rva < 0) {
    return error("TLS's address of callbacks should be a VA. Addr=0x{:06x}, "
                 "Imagebase=0x{:016x}", addr_cbk, imagebase);
  }

  // If the binary is pie, make sure it has relocations for the TLS structures
  if (pe.is_pie()) {
    auto relocations  = pe.relocations();
    if (relocations.empty()) {
      return error("Missing relocations for the TLS structures");
    }

    const size_t nb_cbk = tls->callbacks().size();

    uint32_t tls_hdr_start = tls_dir->RVA();
    uint32_t tls_hdr_end = tls_hdr_start + tls_dir->size();

    uint32_t tls_cbk_start = addr_cbk_rva;
    uint32_t tls_cbk_end = addr_cbk_rva + nb_cbk * ptr_size;

    size_t nb_hdr_reloc = 0;
    size_t nb_cbk_reloc = 0;

    for (const Relocation& R : relocations) {
      for (const RelocationEntry& E : R.entries()) {
        const uint64_t addr = E.address();
        if (tls_hdr_start <= addr && addr < tls_hdr_end) {
          ++nb_hdr_reloc;
        }

        if (tls_cbk_start <= addr && addr < tls_cbk_end) {
          ++nb_cbk_reloc;
        }
      }
    }

    if (nb_cbk_reloc < nb_cbk) {
      return error("Expecting #{} callback relocations. Found: #{}",
                   nb_cbk, nb_cbk_reloc);
    }

    size_t expected_hdr_reloc = 0;

    expected_hdr_reloc += tls->addressof_callbacks() > 0 ? 1 : 0;
    expected_hdr_reloc += tls->addressof_raw_data().first > 0 ? 2 : 0;
    expected_hdr_reloc += tls->addressof_index() > 0 ? 1 : 0;

    if (expected_hdr_reloc > nb_hdr_reloc) {
      return error("Expecting #{} TLS header relocations. Found: #{}",
                   expected_hdr_reloc, nb_hdr_reloc);
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
}
