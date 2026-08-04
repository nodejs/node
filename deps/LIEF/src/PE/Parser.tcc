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

#include "logging.hpp"

#include "LIEF/BinaryStream/BinaryStream.hpp"
#include "LIEF/BinaryStream/SpanStream.hpp"
#include "LIEF/PE/LoadConfigurations.hpp"
#include "LIEF/PE/LoadConfigurations/CHPEMetadata.hpp"
#include "LIEF/PE/LoadConfigurations/LoadConfiguration.hpp"
#include "LIEF/PE/Parser.hpp"
#include "LIEF/PE/Binary.hpp"
#include "LIEF/PE/DataDirectory.hpp"
#include "LIEF/PE/DataDirectory.hpp"
#include "LIEF/PE/EnumToString.hpp"
#include "LIEF/PE/Section.hpp"
#include "LIEF/PE/ImportEntry.hpp"

#include "internal_utils.hpp"
#include "frozen.hpp"
#include "PE/Structures.hpp"
#include "LIEF/PE/Parser.hpp"

namespace LIEF {
namespace PE {

bool inline warn_missing_section(const DataDirectory& dir) {
  return dir.type() != DataDirectory::TYPES::CERTIFICATE_TABLE &&
         dir.type() != DataDirectory::TYPES::BOUND_IMPORT;
}

template<typename PE_T>
ok_error_t Parser::parse() {
  if (!parse_headers<PE_T>()) {
    LIEF_WARN("Failed to parse regular PE headers");
    return make_error_code(lief_errors::parsing_error);
  }

  if (!parse_dos_stub()) {
    LIEF_WARN("Failed to parse DOS Stub");
  }

  if (!parse_rich_header()) {
    LIEF_WARN("Failed to parse rich header");
  }

  if (!parse_string_table()) {
    LIEF_WARN("Failed to parse COFF string table");
  }

  if (!parse_sections()) {
    LIEF_WARN("Failed to parse sections");
  }

  if (!parse_data_directories<PE_T>()) {
    LIEF_WARN("Failed to parse data directories");
  }


  if (!parse_load_config<PE_T>()) {
    LIEF_WARN("Failed to parse load config");
  }

  if (!parse_exceptions()) {
    LIEF_WARN("Failed to parse exceptions entries");
  }

  if (!parse_overlay()) {
    LIEF_WARN("Failed to parse the overlay");
  }

  if (!parse_debug()) {
    LIEF_WARN("Failed to parse debug entries");
  }

  if (!parse_symbols()) {
    LIEF_WARN("Failed to parse symbols");
  }

  if (!parse_nested_relocated<PE_T>()) {
    LIEF_WARN("Nested PE ARM64X parsed with errors");
  }

  return ok();
}


template<typename PE_T>
ok_error_t Parser::parse_nested_relocated() {
  class RelocatedStream : public BinaryStream {
    public:
    RelocatedStream(Parser& parent) :
      parent_(&parent)
    {}

    ~RelocatedStream() override = default;

    RelocatedStream(const RelocatedStream&) = delete;
    RelocatedStream& operator=(const RelocatedStream&) = delete;

    RelocatedStream(RelocatedStream&& other) noexcept = default;
    RelocatedStream& operator=(RelocatedStream&& other) noexcept = default;

    uint64_t size() const override {
      return parent_->stream_->size();
    }

    ok_error_t peek_in(void* dst, uint64_t offset, uint64_t size,
                       uint64_t va = 0) const override {
      auto it_value = parent_->dyn_hdr_relocs_.lower_bound(offset);
      if (it_value == parent_->dyn_hdr_relocs_.end()) {
        return parent_->stream_->peek_in(dst, offset, size, va);
      }

      auto is_ok = parent_->stream_->peek_in(dst, offset, size, va);
      if (!is_ok) {
        return make_error_code(is_ok.error());
      }

      // Exact match
      if (it_value->first == offset && (offset + it_value->second.size) <= size)
      {
        const relocation_t& R = it_value->second;

        std::memcpy(dst, &R.value, R.size);
        return ok();
      }

      auto upper_bound = parent_->dyn_hdr_relocs_.upper_bound(offset + size);
      for (; it_value != upper_bound; ++it_value) {
        int64_t delta_offset = (int64_t)it_value->first - (int64_t)offset;
        assert(delta_offset >= 0);
        const relocation_t& R = it_value->second;

        if (delta_offset + R.size > size) {
          continue;
        }

        assert(delta_offset + R.size <= size);
        std::memcpy(reinterpret_cast<uint8_t*>(dst) + delta_offset, &R.value,
                    R.size);
      }
      return ok();
    }

    result<const void*> read_at(uint64_t /*offset*/, uint64_t /*size*/,
                                uint64_t /*va*/) const override
    {
      return make_error_code(lief_errors::not_supported);
    }

    private:
    Parser* parent_ = nullptr;
  };

  if (!config_.parse_arm64x_binary || dyn_hdr_relocs_.empty()) {
    return ok();
  }

  LIEF_DEBUG("Parsing nested binary");
  auto rstream = std::make_unique<RelocatedStream>(*this);
  config_.parse_arm64x_binary = false;
  std::unique_ptr<Binary> nested = parse(std::move(rstream), config_);
  config_.parse_arm64x_binary = true;

  if (nested == nullptr) {
    return make_error_code(lief_errors::parsing_error);
  }

  binary_->nested_ = std::move(nested);

  return ok();
}

template<typename PE_T>
ok_error_t Parser::parse_headers() {
  using pe_optional_header = typename PE_T::pe_optional_header;

  auto dos_hdr = stream_->peek<details::pe_dos_header>(0);
  if (!dos_hdr) {
    LIEF_ERR("Can't read the DOS Header");
    return make_error_code(dos_hdr.error());
  }

  binary_->dos_header_ = *dos_hdr;
  const uint64_t addr_new_exe = binary_->dos_header().addressof_new_exeheader();

  {
    auto pe_header = stream_->peek<details::pe_header>(addr_new_exe);
    if (!pe_header) {
      LIEF_ERR("Can't read the PE header");
      return make_error_code(pe_header.error());
    }
    binary_->header_ = *pe_header;
  }

  {
    const uint64_t offset = addr_new_exe + sizeof(details::pe_header);
    auto opt_header = stream_->peek<pe_optional_header>(offset);
    if (!opt_header) {
      LIEF_ERR("Can't read the optional header");
      return make_error_code(opt_header.error());
    }
    binary_->optional_header_ = *opt_header;
  }

  return ok();
}

template<typename PE_T>
ok_error_t Parser::parse_data_directories() {
  using pe_optional_header = typename PE_T::pe_optional_header;
  const uint32_t directories_offset = binary_->dos_header().addressof_new_exeheader() +
                                      sizeof(details::pe_header) + sizeof(pe_optional_header);
  static constexpr auto NB_DIR = DataDirectory::DEFAULT_NB;
  binary_->data_directories_.resize(NB_DIR);
  // Make sure the data_directory array is correctly initialized
  for (size_t i = 0; i < NB_DIR; ++i) {
    binary_->data_directories_[i] = std::make_unique<DataDirectory>();
  }

  stream_->setpos(directories_offset);

  // WARNING: The PE specifications require that the data directory table ends
  // with a null entry (RVA / Size, set to 0).
  //
  // Nevertheless it seems that this requirement is not enforced by the PE loader.
  // The binary bc203f2b6a928f1457e9ca99456747bcb7adbbfff789d1c47e9479aac11598af contains a non-null final
  // data directory (watermarking?)
  for (size_t i = 0; i < NB_DIR; ++i) {
    auto raw_dir = stream_->read<details::pe_data_directory>();
    if (!raw_dir) {
      LIEF_ERR("Can't read data directory at #{}", i);
      return make_error_code(lief_errors::read_error);
    }
    const auto dir_type = DataDirectory::TYPES(i);
    auto directory = std::make_unique<DataDirectory>(*raw_dir, dir_type);

    if (directory->RVA() > 0) {
      const uint64_t offset = binary_->rva_to_offset(directory->RVA());
      directory->section_   = binary_->section_from_offset(offset);
      if (directory->section_ == nullptr && warn_missing_section(*directory)) {
        LIEF_WARN("Unable to find the section associated with {}",
                  to_string(dir_type));
      }
    }
    binary_->data_directories_[i] = std::move(directory);
  }

  // Import Table
  if (DataDirectory* import_data_dir = binary_->import_dir()) {
    if (import_data_dir->RVA() > 0 && config_.parse_imports) {
      LIEF_DEBUG("Processing Import Table");
      parse_import_table<PE_T>();
    }
  }

  // Exports
  if (const DataDirectory* export_dir = binary_->export_dir()) {
    if (export_dir->RVA() > 0 && config_.parse_exports) {
      LIEF_DEBUG("Parsing Exports");
      parse_exports();
    }
  }

  // Signature
  if (const DataDirectory* dir = binary_->cert_dir()) {
    if (dir->RVA() > 0 && config_.parse_signature) {
      parse_signature();
    }
  }

  if (DataDirectory* dir = binary_->tls_dir()) {
    if (dir->RVA() > 0) {
      parse_tls<PE_T>();
    }
  }

  if (DataDirectory* dir = binary_->relocation_dir()) {
    if (dir->RVA() > 0 && config_.parse_reloc) {
      LIEF_DEBUG("Parsing Relocations");
      parse_relocations();
    }
  }

  if (DataDirectory* dir = binary_->rsrc_dir()) {
    if (dir->RVA() > 0 && config_.parse_rsrc) {
      LIEF_DEBUG("Parsing Resources");
      parse_resources();
    }
  }

  if (DataDirectory* dir = binary_->delay_dir()) {
    if (dir->RVA() > 0) {
      auto is_ok = parse_delay_imports<PE_T>();
      if (!is_ok) {
        LIEF_WARN("The parsing of delay imports has failed or is incomplete ('{}')",
                  to_string(get_error(is_ok)));
      }
    }
  }

  return ok();
}

template<typename PE_T>
ok_error_t Parser::parse_import_table() {
  using uint = typename PE_T::uint;
  DataDirectory* import_dir = binary_->import_dir();
  DataDirectory* iat_dir    = binary_->iat_dir();

  if (import_dir == nullptr || iat_dir == nullptr) {
    return make_error_code(lief_errors::not_found);
  }


  const uint32_t import_rva    = import_dir->RVA();
  const uint64_t import_offset = binary_->rva_to_offset(import_rva);
  const size_t   import_end    = import_offset + import_dir->size();

  uint64_t last_imp_offset = 0;

  stream_->setpos(import_offset);
  result<details::pe_import> imp_res;

  while (stream_->pos() < import_end && (imp_res = stream_->read<details::pe_import>())) {
    const auto raw_imp = *imp_res;
    if (BinaryStream::is_all_zero(raw_imp)) {
      break;
    }

    auto import = std::make_unique<Import>(raw_imp);
    import->directory_       = import_dir;
    import->iat_directory_   = iat_dir;
    import->type_            = type_;

    if (import->name_rva_ == 0) {
      LIEF_DEBUG("Name's RVA is null");
      break;
    }

    // Offset to the Import (Library) name
    const uint64_t offset_name = binary_->rva_to_offset(import->name_rva_);

    if (auto res_name = stream_->peek_string_at(offset_name))  {
      import->name_ = std::move(*res_name);
    } else {
      LIEF_ERR("Can't read the import name (offset: 0x{:x})", offset_name);
      continue;
    }


    // We assume that a DLL name should be at least 4 length size and "printable
    const std::string& imp_name = import->name();
    if (!is_valid_dll_name(imp_name)) {
      if (!imp_name.empty()) {
        LIEF_WARN("'{}' is not a valid import name and will be discarded", imp_name);
        continue;
      }
      continue; // skip
    }

    last_imp_offset = std::max<uint64_t>(last_imp_offset, offset_name + imp_name.size() + 1);

    // Offset to import lookup table
    uint64_t LT_offset = import->import_lookup_table_rva() > 0 ?
                         binary_->rva_to_offset(import->import_lookup_table_rva()) :
                         0;


    // Offset to the import address table
    uint64_t IAT_offset = import->import_address_table_rva() > 0 ?
                          binary_->rva_to_offset(import->import_address_table_rva()) :
                          0;
    LIEF_DEBUG("IAT Offset: 0x{:08x}", IAT_offset);
    LIEF_DEBUG("IAT RVA:    0x{:08x}", import->import_address_table_rva());
    LIEF_DEBUG("ILT Offset: 0x{:08x}", LT_offset);
    LIEF_DEBUG("ILT RVA:    0x{:08x}", import->import_lookup_table_rva());

    uint IAT = 0;
    uint table = 0;

    if (IAT_offset > 0) {
      if (auto res_iat = stream_->peek<uint>(IAT_offset)) {
        IAT   = *res_iat;
        table = IAT;
        IAT_offset += sizeof(uint);
      }
    }

    if (LT_offset > 0) {
      if (auto res_lt = stream_->peek<uint>(LT_offset)) {
        table      = *res_lt;
        LT_offset += sizeof(uint);
      }
    }

    size_t idx = 0;

    while (table != 0 || IAT != 0) {
      auto entry = std::make_unique<ImportEntry>();
      entry->iat_value_ = IAT;
      entry->ilt_value_ = table;
      entry->data_      = table > 0 ? table : IAT; // In some cases, ILT can be corrupted
      entry->type_      = type_;
      entry->rva_       = import->iat_rva_ + sizeof(uint) * (idx++);

      LIEF_DEBUG("IAT: 0x{:08x} | ILT: 0x{:08x}", IAT, table);

      if (!entry->is_ordinal()) {
        const size_t hint_off = binary_->rva_to_offset(entry->hint_name_rva());
        const size_t name_off = hint_off + sizeof(uint16_t);
        if (auto entry_name = stream_->peek_string_at(name_off)) {
          entry->name_ = std::move(*entry_name);
        } else {
          LIEF_ERR("Can't read import entry name");
        }
        if (auto hint = stream_->peek<uint16_t>(hint_off)) {
          entry->hint_ = *hint;
        } else {
          LIEF_INFO("Can't read hint value @0x{:x}", hint_off);
        }

        last_imp_offset = std::max<uint64_t>(last_imp_offset, name_off + entry->name().size() + 1);

        // Check that the import name is valid
        if (is_valid_import_name(entry->name())) {
          import->entries_.push_back(std::move(entry));
        } else if (!entry->name().empty()){
          LIEF_INFO("'{}' is an invalid import name and will be discarded", entry->name());
        }
      } else {
        import->entries_.push_back(std::move(entry));
      }

      if (IAT_offset > 0) {
        if (auto iat = stream_->peek<uint>(IAT_offset)) {
          IAT = *iat;
          IAT_offset += sizeof(uint);
        } else {
          LIEF_ERR("Can't read the IAT value at 0x{:x}", IAT_offset);
          IAT = 0;
        }
      } else {
        IAT = 0;
      }

      if (LT_offset > 0) {
        last_imp_offset = std::max<uint64_t>(last_imp_offset, LT_offset);
        if (auto lt = stream_->peek<uint>(LT_offset)) {
          table = *lt;
          LT_offset += sizeof(uint);
        } else {
          LIEF_ERR("Can't read the Lookup Table value at 0x{:x}", LT_offset);
          table = 0;
        }
      } else {
        table = 0;
      }
    }
    import->nb_original_func_ = import->entries_.size();
    binary_->imports_.push_back(std::move(import));
  }

  return ok();
}


template<class PE_T>
ok_error_t Parser::parse_delay_names_table(DelayImport& import,
                                           uint32_t names_offset,
                                           uint32_t iat_offset)
{
  using ptr_t = typename PE_T::uint;
  ScopedStream nstream(*stream_, names_offset);

  auto entry_val = nstream->read<ptr_t>();
  if (!entry_val) {
    LIEF_ERR("Can't read delay_imports.names_table[0]");
    return make_error_code(entry_val.error());
  }

  while (*nstream && entry_val && *entry_val != 0) {
    auto entry = std::make_unique<DelayImportEntry>(*entry_val, type_);
    // Index of the current entry (-1 as we start with a read())
    const size_t index = (nstream->pos() - names_offset) / sizeof(ptr_t) - 1;
    const uint32_t iat_pos = index * sizeof(ptr_t);

    if (auto iat_value = stream_->peek<ptr_t>(iat_offset + iat_pos)) {
      entry->value_ =import.iat() + iat_pos; // Symbol's value for the base class
      entry->iat_value_ = *iat_value;
      LIEF_DEBUG("  [{}].iat : 0x{:010x}", index, entry->iat_value_);
    }

    if (!entry->is_ordinal()) {
      uint64_t hint_off = binary_->rva_to_offset(entry->hint_name_rva());
      const uint64_t name_off = hint_off + sizeof(uint16_t);

      if (auto entry_name = stream_->peek_string_at(name_off)) {
        entry->name_ = std::move(*entry_name);
      }

      if (auto hint = stream_->peek<uint16_t>(hint_off)) {
        entry->hint_ = *hint;
      }

      if (Parser::is_valid_import_name(entry->name())) {
        import.entries_.push_back(std::move(entry));
      }
    }
    else /* is ordinal */ {
      import.entries_.push_back(std::move(entry));
    }
    entry_val = nstream->read<ptr_t>();
    if (!entry_val) {
      break;
    }
  }
  return ok();
}

template<typename PE_T>
ok_error_t Parser::parse_delay_imports() {
  LIEF_DEBUG("Parsing Delay Import Table");

  const DataDirectory* dir = binary_->delay_dir();
  if (dir == nullptr) {
    return make_error_code(lief_errors::not_found);
  }

  if (dir->RVA() == 0 || dir->size() == 0) {
    return ok();
  }

  std::unique_ptr<SpanStream> stream = dir->stream();
  if (!stream) {
    return make_error_code(lief_errors::read_error);
  }

  while (stream) {
    auto import = stream->read<details::delay_imports>();
    if (!import) {
      LIEF_DEBUG("Error: {}:{}", __FUNCTION__, __LINE__);
      return make_error_code(import.error());
    }

    auto imp = std::make_unique<DelayImport>(*import, type_);

    if (BinaryStream::is_all_zero(*import)) {
      return ok();
    }

    uint64_t name_offset = binary_->rva_to_offset(import->name);
    auto dll_name = stream_->peek_string_at(name_offset);

    if (dll_name && !dll_name->empty() && is_valid_dll_name(*dll_name)) {
      imp->name_ = std::move(*dll_name);
    }

    LIEF_DEBUG("  delay_imports.name:       {}",       imp->name_);
    LIEF_DEBUG("  delay_imports.attribute:  {}",       import->attribute);
    LIEF_DEBUG("  delay_imports.handle:     0x{:04x}", import->handle);
    LIEF_DEBUG("  delay_imports.iat:        0x{:04x}", import->iat);
    LIEF_DEBUG("  delay_imports.name_table: 0x{:04x}", import->name_table);
    LIEF_DEBUG("  delay_imports.bound_iat:  0x{:04x}", import->bound_iat);
    LIEF_DEBUG("  delay_imports.unload_iat: 0x{:04x}", import->unload_iat);
    LIEF_DEBUG("  delay_imports.timestamp:  0x{:04x}", import->timestamp);

    // Offset to Delay Import Name Table
    uint64_t names_offset =
      import->name_table > 0 ? binary_->rva_to_offset(import->name_table) : 0;

    // Offset to the import address table
    uint64_t IAT_offset =
      import->iat > 0 ? binary_->rva_to_offset(import->iat) : 0;

    LIEF_DEBUG("  [IAT  ]: 0x{:04x}", IAT_offset);
    LIEF_DEBUG("  [Names]: 0x{:04x}", names_offset);

    if (names_offset > 0) {
      auto is_ok = parse_delay_names_table<PE_T>(*imp, names_offset,
                                                 IAT_offset);
      if (!is_ok) {
        LIEF_WARN("Delay imports names table parsed with errors ('{}')",
                  to_string(get_error(is_ok)));
      }
    }

    binary_->delay_imports_.push_back(std::move(imp));
  }

  return ok();
}


template<typename PE_T>
ok_error_t Parser::parse_tls() {
  using pe_tls = typename PE_T::pe_tls;
  using uint = typename PE_T::uint;

  LIEF_DEBUG("Parsing TLS");

  DataDirectory* tls_dir = binary_->tls_dir();
  if (tls_dir == nullptr) {
    return make_error_code(lief_errors::not_found);
  }
  const uint32_t tls_rva = tls_dir->RVA();
  const uint64_t offset  = binary_->rva_to_offset(tls_rva);

  stream_->setpos(offset);

  const auto tls_header = stream_->read<pe_tls>();

  if (!tls_header) {
    return make_error_code(lief_errors::read_error);
  }

  auto tls = std::make_unique<TLS>(*tls_header);

  const uint64_t imagebase = binary_->optional_header().imagebase();

  if (tls_header->RawDataStartVA >= imagebase && tls_header->RawDataEndVA > tls_header->RawDataStartVA) {
    const uint64_t start_data_rva = tls_header->RawDataStartVA - imagebase;
    const uint64_t stop_data_rva  = tls_header->RawDataEndVA - imagebase;

    const uint start_template_offset = binary_->rva_to_offset(start_data_rva);
    const uint end_template_offset   = binary_->rva_to_offset(stop_data_rva);

    const size_t size_to_read = end_template_offset - start_template_offset;

    if (size_to_read > Parser::MAX_DATA_SIZE) {
      LIEF_DEBUG("TLS's template is too large!");
    } else {
      if (!stream_->peek_data(tls->data_template_, start_template_offset, size_to_read)) {
        LIEF_WARN("TLS's template corrupted");
      }
    }
  }

  if (tls->addressof_callbacks() > imagebase) {
    uint64_t callbacks_offset = binary_->rva_to_offset(tls->addressof_callbacks() - imagebase);
    stream_->setpos(callbacks_offset);
    size_t count = 0;
    while (count++ < Parser::MAX_TLS_CALLBACKS) {
      auto res_callback_rva = stream_->read<uint>();
      if (!res_callback_rva) {
        break;
      }

      auto callback_rva = *res_callback_rva;

      if (static_cast<uint32_t>(callback_rva) == 0) {
        break;
      }
      tls->callbacks_.push_back(callback_rva);
    }
  }

  tls->directory_ = tls_dir;
  tls->section_ = tls_dir->section();
  binary_->sizing_info_.nb_tls_callbacks = tls->callbacks_.size();

  binary_->tls_ = std::move(tls);
  return ok();
}

template<typename PE_T>
ok_error_t Parser::parse_load_config() {
  const DataDirectory* lconf_dir = bin().load_config_dir();
  assert(lconf_dir != nullptr);

  if (lconf_dir->RVA() == 0 || lconf_dir->size() == 0) {
    return ok();
  }

  std::unique_ptr<SpanStream> stream = lconf_dir->stream(/*sized=*/false);
  if (stream == nullptr) {
    return make_error_code(lief_errors::read_error);
  }

  auto config = LoadConfiguration::parse<PE_T>(*this, *stream);
  if (config == nullptr) {
    return make_error_code(lief_errors::read_error);
  }

  if (!process_load_config<PE_T>(*config)) {
    LIEF_DEBUG("Load configuration processing finished with errors");
  }

  binary_->sizing_info_.load_config_size = config->size();
  binary_->loadconfig_ = std::move(config);

  return ok();
}

template<typename PE_T>
ok_error_t Parser::process_load_config(LoadConfiguration& lconf) {
  if (uint64_t addr = lconf.se_handler_table().value_or((0)); addr > 0) {
    uint64_t offset = bin().va_to_offset(addr);
    ScopedStream stream(*stream_, offset);
    if (!LoadConfiguration::parse_seh_table(*this, *stream, lconf)) {
      LIEF_WARN("SEH table processing finished with errors");
    }
  }

  if (uint64_t addr = lconf.guard_cf_function_table().value_or((0)); addr > 0) {
    uint64_t offset = bin().va_to_offset(addr);
    ScopedStream stream(*stream_, offset);
    auto dst = &LoadConfiguration::guard_cf_functions_;
    const size_t count = lconf.guard_cf_function_count().value_or(0);

    auto is_ok = LoadConfiguration::parse_guard_functions(
      *this, *stream, lconf, count, dst);
    if (!is_ok) {
      LIEF_WARN("Guard CF table processing finished with errors");
    }
  }

  if (uint64_t addr = lconf.guard_address_taken_iat_entry_table().value_or((0));
      addr > 0) {
    uint64_t offset = bin().va_to_offset(addr);
    ScopedStream stream(*stream_, offset);
    auto dst = &LoadConfiguration::guard_address_taken_iat_entries_;
    const size_t count = lconf.guard_address_taken_iat_entry_count().value_or(0);

    auto is_ok = LoadConfiguration::parse_guard_functions(
      *this, *stream, lconf, count, dst);
    if (!is_ok) {
      LIEF_WARN("Guard CF Address Taken IAT Entry Table processing finished "
                "with errors");
    }
  }

  if (uint64_t addr = lconf.guard_long_jump_target_table().value_or((0));
      addr > 0) {
    uint64_t offset = bin().va_to_offset(addr);
    ScopedStream stream(*stream_, offset);
    auto dst = &LoadConfiguration::guard_long_jump_targets_;
    const size_t count = lconf.guard_long_jump_target_count().value_or(0);

    auto is_ok = LoadConfiguration::parse_guard_functions(
      *this, *stream, lconf, count, dst);

    if (!is_ok) {
      LIEF_WARN("Guard CF long jump target processing finished with errors");
    }
  }

  if (uint64_t addr = lconf.guard_eh_continuation_table().value_or((0));
      addr > 0) {
    uint64_t offset = bin().va_to_offset(addr);
    ScopedStream stream(*stream_, offset);
    auto dst = &LoadConfiguration::guard_eh_continuation_functions_;
    const size_t count = lconf.guard_eh_continuation_count().value_or(0);

    auto is_ok = LoadConfiguration::parse_guard_functions(
      *this, *stream, lconf, count, dst);

    if (!is_ok) {
      LIEF_WARN("Guard EH continuation function processing finished with errors");
    }
  }

  if (uint64_t addr = lconf.hybrid_metadata_pointer().value_or((0)); addr > 0) {
    uint64_t offset = bin().va_to_offset(addr);
    ScopedStream stream(*stream_, offset);
    auto metadata = CHPEMetadata::parse(*this, *stream);
    if (metadata != nullptr) {
      lconf.chpe_ = std::move(metadata);
    }
  }

  if (!LoadConfiguration::parse_dyn_relocs(*this, lconf)) {
    LIEF_WARN("Dynamic relocation processing finished with errors");
  }

  if (!LoadConfiguration::parse_enclave_config<PE_T>(*this, lconf)) {
    LIEF_WARN("Errors while processing enclave configuration");
  }

  if (!LoadConfiguration::parse_volatile_metadata(*this, lconf)) {
    LIEF_WARN("Errors while processing volatile metadata");
  }

  return ok();
}

}
}
