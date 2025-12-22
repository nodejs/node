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

#include "LIEF/PE/Builder.hpp"
#include "LIEF/PE/Binary.hpp"
#include "LIEF/PE/ImportEntry.hpp"
#include "LIEF/PE/Section.hpp"
#include "LIEF/PE/DataDirectory.hpp"
#include "LIEF/PE/Relocation.hpp"
#include "LIEF/PE/RelocationEntry.hpp"
#include "LIEF/PE/TLS.hpp"
#include "LIEF/PE/LoadConfigurations.hpp"
#include "PE/Structures.hpp"

#include "logging.hpp"
#include "LIEF/utils.hpp"

namespace LIEF {
namespace PE {

template<typename PE_T>
ok_error_t Builder::build_optional_header(const OptionalHeader& optional_header) {
  using uint__             = typename PE_T::uint;
  using pe_optional_header = typename PE_T::pe_optional_header;

  // Build optional header
  binary_->optional_header()
    .sizeof_image(static_cast<uint32_t>(binary_->virtual_size()));
  binary_->optional_header()
    .sizeof_headers(static_cast<uint32_t>(binary_->sizeof_headers()));

  vector_iostream pe_opt_hdr_strm;
  pe_opt_hdr_strm.reserve(sizeof(pe_optional_header));

  pe_opt_hdr_strm
    .write<uint16_t>((int)optional_header.magic())
    .write<uint8_t>(optional_header.major_linker_version())
    .write<uint8_t>(optional_header.minor_linker_version())
    .write<uint32_t>(optional_header.sizeof_code())
    .write<uint32_t>(optional_header.sizeof_initialized_data())
    .write<uint32_t>(optional_header.sizeof_uninitialized_data())
    .write<uint32_t>(optional_header.addressof_entrypoint())
    .write<uint32_t>(optional_header.baseof_code())
  ;

  if constexpr (std::is_same_v<PE_T, details::PE32>) {
    pe_opt_hdr_strm.write<uint32_t>(optional_header.baseof_data());
  }

  pe_opt_hdr_strm
    .template write<uint__>(optional_header.imagebase())
    .template write<uint32_t>(optional_header.section_alignment())
    .template write<uint32_t>(optional_header.file_alignment())
    .template write<uint16_t>(optional_header.major_operating_system_version())
    .template write<uint16_t>(optional_header.minor_operating_system_version())
    .template write<uint16_t>(optional_header.major_image_version())
    .template write<uint16_t>(optional_header.minor_image_version())
    .template write<uint16_t>(optional_header.major_subsystem_version())
    .template write<uint16_t>(optional_header.minor_subsystem_version())
    .template write<uint32_t>(optional_header.win32_version_value())
    .template write<uint32_t>(optional_header.sizeof_image())
    .template write<uint32_t>(optional_header.sizeof_headers())
    .template write<uint32_t>(optional_header.checksum())
    .template write<uint16_t>((int)optional_header.subsystem())
    .template write<uint16_t>((int)optional_header.dll_characteristics())
    .template write<uint__>(optional_header.sizeof_stack_reserve())
    .template write<uint__>(optional_header.sizeof_stack_commit())
    .template write<uint__>(optional_header.sizeof_heap_reserve())
    .template write<uint__>(optional_header.sizeof_heap_commit())
    .template write<uint32_t>(optional_header.loader_flags())
    .template write<uint32_t>(optional_header.numberof_rva_and_size())
  ;

  assert(pe_opt_hdr_strm.size() == sizeof(pe_optional_header));

  const uint32_t address_next_header =
    binary_->dos_header().addressof_new_exeheader() +
    sizeof(details::pe_header);

  ios_
    .seekp(address_next_header)
    .write(pe_opt_hdr_strm);
  return ok();
}


template<typename PE_T>
ok_error_t Builder::build_tls() {
  using uint__ = typename PE_T::uint;
  using tls_header = typename PE_T::pe_tls;
  TLS* tls = binary_->tls();

  if (tls == nullptr) {
    return ok();
  }

  const bool is64_bit = binary_->optional_header().magic() == PE_TYPE::PE32_PLUS;

  DataDirectory* tls_dir = binary_->tls_dir();

  // True if the TLS object has been created and was not
  // originally present in the binary
  const bool is_created = tls_dir->RVA() == 0 || tls_dir->size() == 0;

  LIEF_DEBUG("TLS: [0x{:08x}, 0x{:08x}]", tls_dir->RVA(),
             tls_dir->RVA() + tls_dir->size());

  vector_iostream ios_callbacks;

  if (!tls->callbacks().empty()) {
    ios_callbacks.reserve(tls->callbacks().size() * sizeof(uint__));
    for (uint64_t callback : tls->callbacks()) {
      ios_callbacks.write<uint__>(callback);
    }
    ios_callbacks.write<uint32_t>(0);
    ios_callbacks.move(tls_data_.callbacks);
  }

  /// 0 ending is on uint32_t not uint64_t
  const size_t original_callback_size = binary_->sizing_info_.nb_tls_callbacks > 0 ?
    binary_->sizing_info_.nb_tls_callbacks * sizeof(uint__) + sizeof(uint32_t) : 0;

  const uint64_t imagebase = binary_->optional_header().imagebase();
  // addressof_callbacks is a VA not a RVA
  uint32_t tls_callbacks_start = 0;
  uint32_t tls_callbacks_end = 0;

  uint32_t tls_header_start = 0;
  uint32_t tls_header_end = 0;

  if (tls->addressof_callbacks() > 0) {
    assert(tls->addressof_callbacks() >= imagebase);
    tls_callbacks_start = tls->addressof_callbacks() - imagebase;
    tls_callbacks_end = tls_callbacks_start + original_callback_size;
  }

  if (tls_dir->RVA() > 0 && tls_dir->size() > 0) {
    tls_header_start = tls_dir->RVA();
    tls_header_end = tls_header_start + tls_dir->size();
  }

  if (is_created) {
    LIEF_DEBUG("Need to create a TLS section");
    Section tls_section(config_.tls_section);
    const size_t allocated_size =
      align(tls_data_.callbacks.size() + sizeof(tls_header) +
            /* index */ sizeof(uint__),
            binary_->optional_header().file_alignment());

    tls_section
      .reserve(allocated_size)
      .add_characteristic(Section::CHARACTERISTICS::CNT_INITIALIZED_DATA)
      .add_characteristic(Section::CHARACTERISTICS::MEM_READ)
      .add_characteristic(Section::CHARACTERISTICS::MEM_WRITE);

    Section* new_tls_section = binary_->add_section(tls_section);
    vector_iostream write_strm = new_tls_section->edit();
    write_strm
      .write(tls_data_.callbacks)
      .write(sizeof(tls_header), 0)
      .write(0);

    tls_dir->RVA(new_tls_section->virtual_address() + write_strm.tellp());
    tls_dir->size(sizeof(tls_header));


    if (tls->addressof_index() == 0) {
      tls->addressof_index(imagebase + new_tls_section->virtual_address() +
                           tls_data_.callbacks.size() + sizeof(tls_header));
    }

    if (tls->addressof_callbacks() != 0) {
      LIEF_WARN("Replacing existing address of callbacks from "
                "0x{:016x} to 0x{:016x}", tls->addressof_callbacks(),
                new_tls_section->virtual_address());
    }
    tls->addressof_callbacks(imagebase + new_tls_section->virtual_address());

    uint32_t tls_header_start = tls_dir->RVA();
    uint32_t tls_cbk_start = new_tls_section->virtual_address();

    Relocation r_base(new_tls_section->virtual_address());
    const uint32_t r_base_addr = r_base.virtual_address();
    const auto default_ty = is64_bit ? RelocationEntry::BASE_TYPES::DIR64 :
                                       RelocationEntry::BASE_TYPES::HIGHLOW;

    // Create relocations
    if (auto addr_raw = tls->addressof_raw_data(); addr_raw.first > 0) {
      const size_t pos = tls_header_start + offsetof(tls_header, RawDataStartVA);
      assert(pos >= r_base_addr && (pos - r_base_addr) < RelocationEntry::MAX_ADDR);
      r_base.add_entry({(uint16_t)(pos - r_base_addr), default_ty});
    }

    if (auto addr_raw = tls->addressof_raw_data(); addr_raw.second > 0) {
      const size_t pos = tls_header_start + offsetof(tls_header, RawDataEndVA);
      assert(pos >= r_base_addr && (pos - r_base_addr) < RelocationEntry::MAX_ADDR);
      r_base.add_entry({(uint16_t)(pos - r_base_addr), default_ty});
    }

    if (auto addr = tls->addressof_index(); addr > 0) {
      const size_t pos = tls_header_start + offsetof(tls_header, AddressOfIndex);
      assert(pos >= r_base_addr && (pos - r_base_addr) < RelocationEntry::MAX_ADDR);
      r_base.add_entry({(uint16_t)(pos - r_base_addr), default_ty});
    }

    if (auto addr = tls->addressof_callbacks(); addr > 0) {
      const size_t pos = tls_header_start + offsetof(tls_header, AddressOfCallback);
      assert(pos >= r_base_addr && (pos - r_base_addr) < RelocationEntry::MAX_ADDR);
      r_base.add_entry({(uint16_t)(pos - r_base_addr), default_ty});
    }

    for (size_t i = 0; i < tls->callbacks().size(); ++i) {
      const size_t pos = tls_cbk_start + i * sizeof(uint__);
      assert(pos >= r_base_addr && (pos - r_base_addr) < RelocationEntry::MAX_ADDR);
      r_base.add_entry({(uint16_t)(pos - r_base_addr), default_ty});
    }
    binary_->add_relocation(r_base);
  }
  // User added new callbacks
  else if (tls_data_.callbacks.size() > original_callback_size || config_.force_relocating) {
    LIEF_DEBUG("Need to relocated TLS Callbacks (0x{:06x} new bytes)",
               tls_data_.callbacks.size() - original_callback_size);
    Section tls_section(config_.tls_section);

    // We don't necessary need to relocate the TLS header but it can be worth
    // reserving this space ahead of a potential need. In the "worse" case
    // of a PE32+, 36 bytes are reserved.
    const size_t relocated_size =
      align(tls_data_.callbacks.size() + sizeof(tls_header),
            binary_->optional_header().file_alignment());

    tls_section
      .reserve(relocated_size)
      .add_characteristic(Section::CHARACTERISTICS::CNT_INITIALIZED_DATA)
      .add_characteristic(Section::CHARACTERISTICS::MEM_READ)
      .add_characteristic(Section::CHARACTERISTICS::MEM_WRITE);

    Section* new_tls_section = binary_->add_section(tls_section);
    new_tls_section->edit().write(tls_data_.callbacks);
    tls->addressof_callbacks(imagebase + new_tls_section->virtual_address());
    LIEF_DEBUG("TLS new callbacks address: 0x{:08x} (0x{:016x})",
               new_tls_section->virtual_address(), tls->addressof_callbacks());

    // Process relocations to make sure they are consistent with the new
    // location.
    std::vector<RelocationEntry*> hdr_relocations;
    std::vector<RelocationEntry*> cbk_relocations;
    hdr_relocations.reserve(4);
    cbk_relocations.reserve(tls->callbacks_.size());

    for (Relocation& R : binary_->relocations()) {
      for (std::unique_ptr<RelocationEntry>& E : R.entries_) {
        const uint32_t addr = E->address();

        bool in_hdr = tls_header_start <= addr && addr < tls_header_end;
        bool in_cbk = tls_callbacks_start <= addr && addr < tls_callbacks_end;

        if (in_hdr) {
          LIEF_DEBUG("{:16}: 0x{:04x} ({})", "TLS reloc header", addr,
                     to_string(E->type()));
          hdr_relocations.push_back(E.get());
        }

        if (in_cbk) {
          LIEF_DEBUG("{:16}: 0x{:04x} ({})", "TLS reloc cbk", addr,
                     to_string(E->type()));
          cbk_relocations.push_back(E.get());
        }
      }
    }

    LIEF_DEBUG("relocations[header]   : #{:04d}", hdr_relocations.size());
    LIEF_DEBUG("relocations[callbacks]: #{:04d}", cbk_relocations.size());

    // Erase relocations associated with callbacks
    for (RelocationEntry* E : cbk_relocations) {
      Relocation* parent = E->parent();

      if (parent == nullptr) {
        LIEF_WARN("Missing parent relocation");
        continue;
      }

      parent->entries_.erase(
        std::remove_if(parent->entries_.begin(), parent->entries_.end(),
          [E] (std::unique_ptr<RelocationEntry>& F) {
            return E == F.get();
        }), parent->entries_.end());
    }

    // Relocation that need to be created:
    const size_t req_relocs = tls->callbacks().size();
    LIEF_DEBUG("Need to create #{} relocations", req_relocs);

    const uint32_t new_cbk_rva = new_tls_section->virtual_address();

    Relocation reloc_base(new_cbk_rva);
    const auto default_ty = is64_bit ? RelocationEntry::BASE_TYPES::DIR64 :
                                       RelocationEntry::BASE_TYPES::HIGHLOW;
    for (size_t i = 0; i < req_relocs; ++i) {
      const uint16_t pos = i * sizeof(uint__);
      assert (pos < RelocationEntry::MAX_ADDR);
      reloc_base.add_entry(RelocationEntry(pos, default_ty));
    }

    binary_->add_relocation(reloc_base);

  } else /* We can write the TLS callbacks without relocating the table */ {
    [[maybe_unused]]
      int64_t delta_size = original_callback_size - tls_data_.callbacks.size();
    LIEF_DEBUG("TLS Callbacks -0x{:06x}", delta_size);
    if (tls_data_.callbacks.empty() && original_callback_size != 0) {
      // The callbacks have been removed. In this case, we need to remove any
      // relocation pointing in the current callback section, set addressof_callbacks
      // to 0 and fill the existing callback bytes with 0
      for (Relocation& R : binary_->relocations()) {
        auto& entries = R.entries_;
        [[maybe_unused]] const size_t S1 = R.entries_.size();
        entries.erase(
          std::remove_if(entries.begin(), entries.end(),
          [tls_callbacks_start, tls_callbacks_end] (std::unique_ptr<RelocationEntry>& E)
          {
            uint32_t addr = E->address();
            return tls_callbacks_start <= addr && addr < tls_callbacks_end;
          }
        ), entries.end());
        [[maybe_unused]] const size_t S2 = R.entries_.size();
        LIEF_DEBUG("#{} relocation deleted", S1 - S2);
      }

      binary_->fill_address(tls_callbacks_start, original_callback_size, 0);
      tls->addressof_callbacks(0);
    } else {
      binary_->patch_address(tls_callbacks_start, tls_data_.callbacks);
    }
  }

  vector_iostream ios_header;
  ios_header.reserve(sizeof(tls_header));
  ios_header
    .template write<uint__>(tls->addressof_raw_data())
    .template write<uint__>(tls->addressof_index())
    .template write<uint__>(tls->addressof_callbacks())
    .template write<uint32_t>(tls->sizeof_zero_fill())
    .template write<uint32_t>(tls->characteristics())
  ;

  ios_header.move(tls_data_.header);
  if (const DataDirectory* dir = binary_->tls_dir(); dir->RVA() > 0) {
    if (dir->size() < tls_data_.header.size()) {
      LIEF_WARN("TLS Header is larger than the original. original=0x{:08x}, "
                "new=0x{:08x}", dir->size(), tls_data_.header.size());
    }
    binary_->patch_address(dir->RVA(), tls_data_.header);
  }

  return ok();
}

template<typename PE_T>
ok_error_t Builder::build_imports() {
  // The layout of the import table may vary depending on the linker used.
  //
  // ================================================================
  // link.exe (Microsoft)
  // ================================================================
  //
  // +--------------------------+
  // | IAT(s)                   |
  // +--------------------------+
  // ///////////////////////////
  // +--------------------------+ <------------+ import_dir.rva()
  // |                          |              |
  // | Import Headers           |              |
  // |                          |              |
  // +--------------------------+              v + import_dir.size()
  // ---------------------------+
  // | ILT[DLL#1]               |
  // +--------------------------+
  // ---------------------------+
  // | ILT[DLL#2]               |
  // +--------------------------+
  // +--------------------------+
  // | Names: DLL Names         |
  // |        Hint Names        |
  // +--------------------------+
  //
  // ================================================================
  // ld-link.exe (LLVM)
  // Reference (LLVM 19.X):
  //   Writer::addSyntheticIdata() from <llvm>/lld/COFF/Writer.cpp
  // ================================================================
  //
  // +--------------------------+
  // |                          |
  // | Import Headers           |
  // |                          |
  // +--------------------------+
  // ---------------------------+
  // | ILT[DLL#1]               |
  // +--------------------------+
  // ---------------------------+
  // | ILT[DLL#2]               |
  // +--------------------------+
  // ---------------------------+
  // | IAT[DLL#1]               |
  // +--------------------------+
  // ---------------------------+
  // | IAT[DLL#2]               |
  // +--------------------------+
  // +--------------------------+
  // | Hint Names               |
  // +--------------------------+
  // +--------------------------+
  // | DLL Names                |
  // +--------------------------+
  //
  // and the layout for PE compiled with MinGW is also different [...].
  // Thus, we can't make any assumptions about a standardized layout of the
  // imports.
  //
  // While the import headers, ILTs, hint names and DLL names can be relocated
  // elsewhere in the binary, the IAT can't be as it is used in the assembly
  // code to call (imported) functions:
  //
  // With cl.exe / link.exe:
  //
  // .text
  // +--------------------------+
  // | call FlsAlloc.stub       | ---+
  // +--------------------------+    |
  // .text[thunk]                    |
  // +--------------------------+ <--+
  // | jmp [rip + IAT_OFF]      |
  // +--------------------------+ ---+
  // .idata[IAT]                     |
  // +--------------------------+    |
  // | FlsAlloc@addr            | <--+
  // +--------------------------+
  //
  // With clang-cl.exe / ld-link.exe:
  //
  // .text
  // +--------------------------+
  // | mov rax, [rip + IAT_OFF] |
  // | call rax                 | ---+
  // +--------------------------+    |
  // .idata[IAT]                     |
  // +--------------------------+    |
  // | FlsAlloc@addr            | <--+
  // +--------------------------+
  //
  // In any case, we can't consider patching the assembly code.
  //
  // Instead, we relocate the import table while still keeping the original
  // location of the IAT.
  //
  // ================================================================
  // LIEF Layout
  // ================================================================
  //
  // +--------------------------+ <------------+ import_dir.rva()
  // |                          |              |
  // | Import Headers           |              |
  // |                          |              |
  // +--------------------------+              |
  // ---------------------------+              |
  // | ILT[DLL#1]               |              |
  // +--------------------------+              |
  // ---------------------------+              |
  // | ILT[DLL#2]               |              |
  // +--------------------------+              v + import_dir.size()
  // +--------------------------+
  // | Names: DLL Names         |
  // |        Hint Names        |
  // +--------------------------+
  // .                          .
  // .                          .
  // +--------------------------+ --------> Custom IAT for user-added imports
  // | IAT(s)                   |
  // +--------------------------+
  enum FIXUP {
    FX_IMPORT_TABLE = 0,
    _FX_CNT,
  };
  static constexpr auto RELOCATED_IAT = uint64_t(1) << 60;
  using uint__ = typename PE_T::uint;

  LIEF_DEBUG("Rebuilding imports");

  DataDirectory* import_dir = binary_->import_dir();
  if (binary_->imports_.empty()) {
    import_dir->RVA(0);
    import_dir->size(0);
    return ok();
  }

  std::unordered_map<std::string, uint32_t> names_offset;

  vector_iostream headers_ilt;
  vector_iostream names_stream;
  vector_iostream iat_stream;

  uint32_t nb_imported_functions = 0;

  for (const Import& imp : binary_->imports()) {
    const std::string& imp_name = imp.name();
    if (auto it = names_offset.find(imp_name); it == names_offset.end()) {
      names_offset[imp_name] = names_stream.tellp();
      names_stream
        .write(imp_name);
    }
    for (const ImportEntry& entry : imp.entries()) {
      const std::string& entry_name = entry.name();
      if (auto it = names_offset.find(entry_name); it == names_offset.end()) {
        names_offset[entry_name] = names_stream.tellp();
        names_stream
          .write<uint16_t>(entry.hint())
          .write(entry_name)
          .align(2);
      }
    }
    nb_imported_functions += imp.nb_original_func() > 0 ? imp.nb_original_func() :
                                                          imp.entries().size();
    ++nb_imported_functions; // For the null entry
  }

  uint32_t headers_offset = 0;
  uint32_t ilt_offsets    = headers_offset + (binary_->imports().size() + 1) * sizeof(details::pe_import);
  uint32_t strings_offset = ilt_offsets + nb_imported_functions * sizeof(uint__);
  uint32_t iat_offset     = align(strings_offset + names_stream.size(), sizeof(uint16_t));

  headers_ilt.init_fixups(_FX_CNT);
  iat_stream.init_fixups(_FX_CNT);

  for (Import& imp : binary_->imports()) {
    const bool has_existing_iat = imp.import_address_table_rva() != 0;

    const std::string& imp_name = imp.name();
    const auto it = names_offset.find(imp_name);

    if (it == names_offset.end()) {
      LIEF_ERR("Can't find offset for name: {}", imp_name);
      return make_error_code(lief_errors::build_error);
    }
    uint32_t iat_rva = has_existing_iat ? imp.import_address_table_rva() :
                                          iat_offset + (uint32_t)iat_stream.tellp();
    headers_ilt
      .seekp(headers_offset)
      .record_fixup(FX_IMPORT_TABLE)
      .template write<uint32_t>(/*ImportLookupTableRVA=*/ilt_offsets)
      .template write<uint32_t>(/*TimeDateStamp=*/imp.timedatestamp())
      .template write<uint32_t>(/*ForwarderChain=*/imp.forwarder_chain())
      .record_fixup(FX_IMPORT_TABLE)
      .template write<uint32_t>(/*NameRVA=*/strings_offset + it->second)
      .record_fixup(FX_IMPORT_TABLE, !has_existing_iat)
      .template write<uint32_t>(/*ImportAddressTableRVA=*/iat_rva)
    ;
    headers_offset = headers_ilt.tellp();
    headers_ilt.seekp(ilt_offsets);

    uint32_t IAT_pos = imp.import_address_table_rva();

    for (ImportEntry& entry : imp.entries()) {
      size_t count = 1;
      if (has_existing_iat && entry.iat_address() != IAT_pos) {
        if (entry.iat_address() > IAT_pos) {
          // This case can happen when a function (or multiple functions)
          // has been removed from the current Import. To fill this gap,
          // we can pad the missing ILT/IAT entries with the current one
          uint64_t delta = entry.iat_address() - IAT_pos;
          assert(delta % sizeof(uint__) == 0);
          count += delta / sizeof(uint__);
          LIEF_DEBUG("padding count: {}", delta / sizeof(uint__));
        } else {
          LIEF_WARN("IAT location mismatch for {}:{} "
                    "(RVA=0x{:08x}, Expected=0x{:08x})",
                    imp_name, entry.is_ordinal() ?
                    '#' + std::to_string(entry.ordinal()) : entry.name(),
                    IAT_pos, entry.iat_address());
        }
      }

      uint__ ilt_value = entry.data();
      if (!entry.is_ordinal()) {
        auto it_name = names_offset.find(entry.name());
        if (it_name == names_offset.end()) {
          LIEF_ERR("Can't find name offset for the function: {}", entry.name());
        }
        ilt_value = strings_offset + it_name->second;
      }

      for (size_t i = 0; i < count; ++i) {
        const bool is_last = i == (count - 1);

        headers_ilt
          // ilt_value is the name RVA, hence the FX_IMPORT_TABLE fixup
          .record_fixup(FX_IMPORT_TABLE, !entry.is_ordinal())
          .template write<uint__>(ilt_value);

        if (!has_existing_iat) {
          entry.iat_address((iat_offset + iat_stream.tellp()) | RELOCATED_IAT);
          iat_stream
            .record_fixup(FX_IMPORT_TABLE, !entry.is_ordinal())
            .template write<uint__>(ilt_value);
        }

        if (!is_last && has_existing_iat) {
          binary_->patch_address(IAT_pos, entry.iat_value(), sizeof(uint__));
        }
        IAT_pos += sizeof(uint__);
      }

      ilt_offsets = headers_ilt.tellp();
    }

    // Null entry
    headers_ilt
      .seekp(ilt_offsets)
      .write<uint__>(0);

    if (!has_existing_iat) {
      iat_stream.write<uint__>(0);
    }
    ilt_offsets = headers_ilt.tellp();
  }

  // Null entry at the end
  headers_ilt
    .seekp(headers_offset)
    .write(sizeof(details::pe_import), 0);

  headers_offset = headers_ilt.tellp();

  const size_t relocated_size = align(iat_offset + iat_stream.size(),
                                      binary_->optional_header().file_alignment());

  Section idata_section(config_.idata_section);
  idata_section.reserve(relocated_size);
  idata_section
    .add_characteristic(Section::CHARACTERISTICS::CNT_INITIALIZED_DATA)
    .add_characteristic(Section::CHARACTERISTICS::MEM_READ);

  if (!iat_stream.empty()) {
    /* The additional (lief-created) IAT associated with imports added by the
     * user is located in the **new** idata section.
     *
     * This section is flagged as CNT_INITIALIZED_DATA | MEM_READ
     * and these characteristics are enough as long as the IAT area
     * is correctly represented in the IAT DataDirectory.
     * If this is not the case (i.e. we have an IAT that is not a slice of the
     * IAT wrapped by the IAT DataDirectory), we get a
     * "Access violation - code c0000005" when the loader tries to write the
     * resolved symbol in our custom IAT.
     *
     * It seems that windows loader set implicit permissions for the memory
     * page wrapped by the IAT DataDirectory:
     *
     *   0:000> !address 0x7ff7bfdb3f00 <--- Address in the genuine IAT
     *
     *   Usage:                  Image
     *   Base Address:           00007ff7`bfda1000
     *   End Address:            00007ff7`bfedf000
     *   Region Size:            00000000`0013e000 (   1.242 MB)
     *   State:                  00001000          MEM_COMMIT
     *   Protect:                00000020          PAGE_EXECUTE_READ <---------+
     *   Type:                   01000000          MEM_IMAGE                   |
     *   Allocation Base:        00007ff7`bfda0000                             |
     *   Allocation Protect:     00000080          PAGE_EXECUTE_WRITECOPY      |
     *                                                                         |
     *   --------------------------------------------------------------------- |
     *                                                                         |
     *   0:000> !address 00007FF7BFFAEB00  <--- Address in our custom IAT      |
     *                                                                         |
     *   Usage:                  Image                                         |
     *   Base Address:           00007ff7`bffa9000                             |
     *   End Address:            00007ff7`bffaf000                             |
     *   Region Size:            00000000`00006000 (  24.000 kB)               |
     *   State:                  00001000          MEM_COMMIT                  |
     *   Protect:                00000002          PAGE_READONLY <-------------+
     *   Type:                   01000000          MEM_IMAGE
     *   Allocation Base:        00007ff7`bfda0000
     *   Allocation Protect:     00000080          PAGE_EXECUTE_WRITECOPY
     *
     * To make sure we don't get a memory access violation when the loader
     * writes the resolved symbol in our IAT, we need to **both**
     * characteristics: MEM_WRITE | CNT_CODE.
     */
    idata_section
      .add_characteristic(Section::CHARACTERISTICS::MEM_WRITE)
      .add_characteristic(Section::CHARACTERISTICS::CNT_CODE);
  }

  Section* new_idata_section = binary_->add_section(idata_section);
  if (new_idata_section == nullptr) {
    LIEF_ERR("Can't add a new idata section");
    return make_error_code(lief_errors::build_error);
  }
  import_dir->RVA(new_idata_section->virtual_address());

  // We can skip the names string size. MS is even skipping the ILTs size
  // The requirement seems that the size must be at least as long as the
  // size of the import DLL **headers**
  import_dir->size(headers_ilt.size());

  headers_ilt.apply_fixup<uint32_t>(FX_IMPORT_TABLE, import_dir->RVA());
  iat_stream.apply_fixup<uint32_t>(FX_IMPORT_TABLE, import_dir->RVA());

  headers_ilt
    .seekp(strings_offset)
    .write(names_stream.raw())
    .align(sizeof(uint16_t))
    .write(iat_stream.raw());

  new_idata_section->content(headers_ilt.raw());


  if (config_.resolved_iat_cbk) {
    for (const Import& imp : binary_->imports()) {
      for (const ImportEntry& entry : imp.entries()) {
        const bool is_relocated = (entry.iat_address() & RELOCATED_IAT) != 0;
        if (!is_relocated) {
          continue;
        }
        uint32_t rva = (uint32_t)entry.iat_address() + import_dir->RVA();
        config_.resolved_iat_cbk(binary_, &imp, &entry, rva);
      }
    }
  }

  return ok();
}

template<class PE_T>
ok_error_t write_load_config(const LoadConfiguration& config, vector_iostream& ios) {
  using uint__ = typename PE_T::uint;
  ios
    .template write<uint32_t>(config.characteristics())
    .template write<uint32_t>(config.timedatestamp())
    .template write<uint16_t>(config.major_version())
    .template write<uint16_t>(config.minor_version())
    .template write<uint32_t>(config.global_flags_clear())
    .template write<uint32_t>(config.global_flags_set())
    .template write<uint32_t>(config.critical_section_default_timeout())
    .template write<uint__>(config.decommit_free_block_threshold())
    .template write<uint__>(config.decommit_total_free_threshold())
    .template write<uint__>(config.lock_prefix_table())
    .template write<uint__>(config.maximum_allocation_size())
    .template write<uint__>(config.virtual_memory_threshold())
    .template write<uint__>(config.process_affinity_mask())
    .template write<uint32_t>(config.process_heap_flags())
    .template write<uint16_t>(config.csd_version())
    .template write<uint16_t>(config.reserved1())
    .template write<uint__>(config.editlist())
    .template write<uint__>(config.security_cookie())

    .template write<uint__>(config.se_handler_table())
    .template write<uint__>(config.se_handler_count())

    .template write<uint__>(config.guard_cf_check_function_pointer())
    .template write<uint__>(config.guard_cf_dispatch_function_pointer())
    .template write<uint__>(config.guard_cf_function_table())
    .template write<uint__>(config.guard_cf_function_count())
    .template write<uint32_t>(config.guard_flags())
  ;

  if (const CodeIntegrity* integrity = config.code_integrity()) {
    ios
      .write<uint16_t>(integrity->flags())
      .write<uint16_t>(integrity->catalog())
      .write<uint32_t>(integrity->catalog_offset())
      .write<uint32_t>(integrity->reserved());
  }

  ios
    .template write<uint__>(config.guard_address_taken_iat_entry_table())
    .template write<uint__>(config.guard_address_taken_iat_entry_count())
    .template write<uint__>(config.guard_long_jump_target_table())
    .template write<uint__>(config.guard_long_jump_target_count())

    .template write<uint__>(config.dynamic_value_reloc_table())
    .template write<uint__>(config.hybrid_metadata_pointer())

    .template write<uint__>(config.guard_rf_failure_routine())
    .template write<uint__>(config.guard_rf_failure_routine_function_pointer())
    .template write<uint32_t>(config.dynamic_value_reloctable_offset())
    .template write<uint16_t>(config.dynamic_value_reloctable_section())
    .template write<uint16_t>(config.reserved2())

    .template write<uint__>(config.guard_rf_verify_stackpointer_function_pointer())
    .template write<uint32_t>(config.hotpatch_table_offset())
    .template write<uint32_t>(config.reserved3())
    .template write<uint__>(config.enclave_configuration_ptr())
    .template write<uint__>(config.volatile_metadata_pointer())
    .template write<uint__>(config.guard_eh_continuation_table())
    .template write<uint__>(config.guard_eh_continuation_count())
    .template write<uint__>(config.guard_xfg_check_function_pointer())
    .template write<uint__>(config.guard_xfg_dispatch_function_pointer())
    .template write<uint__>(config.guard_xfg_table_dispatch_function_pointer())
    .template write<uint__>(config.cast_guard_os_determined_failure_mode())
    .template write<uint__>(config.guard_memcpy_function_pointer());
  return ok();
}

template<typename PE_T>
ok_error_t Builder::build_load_config() {

  DataDirectory* lconf_dir = binary_->load_config_dir();
  LoadConfiguration* lconf = binary_->load_configuration();

  LIEF_DEBUG("Writing load config");

  if (lconf == nullptr) {
    if (lconf_dir->RVA() > 0 && lconf_dir->size() > 0) {
      binary_->fill_address(lconf_dir->RVA(), lconf_dir->size());
    }
    lconf_dir->RVA(0);
    lconf_dir->size(0);
    return ok();
  }

  if (lconf->size() > binary_->sizing_info_.load_config_size) {
    /* Not supported yet since relocating this structure also means
     * patching relocations.
     */
    LIEF_DEBUG("LoadConfiguration.size:           0x{:08x}", lconf->size());
    LIEF_DEBUG("IMAGE_DIRECTORY_LOAD_CONFIG.size: 0x{:08x}", lconf_dir->size());

    LIEF_WARN("Larger load config directory is not supported");
    return make_error_code(lief_errors::not_supported);
  }


  vector_iostream ios;
  ios.reserve(lconf_dir->size());

  if (auto is_ok = write_load_config<PE_T>(*lconf, ios); !is_ok) {
    return is_ok;
  }
  ios.align(sizeof(uint32_t));

  size_t write_sz = std::min<size_t>(lconf_dir->size(), ios.size());

  span<uint8_t> raw_lconf = lconf_dir->content();
  ios.copy_into(raw_lconf, write_sz);
  return ok();
}

}
}
