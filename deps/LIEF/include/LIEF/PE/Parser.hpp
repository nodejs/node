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
#ifndef LIEF_PE_PARSER_H
#define LIEF_PE_PARSER_H

#include <string>
#include <vector>
#include <map>

#include "LIEF/visibility.h"
#include "LIEF/utils.hpp"
#include "LIEF/errors.hpp"

#include "LIEF/Abstract/Parser.hpp"
#include "LIEF/PE/enums.hpp"
#include "LIEF/PE/ParserConfig.hpp"
#include "LIEF/COFF/String.hpp"

namespace LIEF {
class BinaryStream;
class SpanStream;

namespace PE {
class Debug;
class ResourceNode;
class Binary;
class DelayImport;
class Section;
class ExceptionInfo;
class LoadConfiguration;
class RelocationEntry;

namespace details {
struct pe_debug;
struct pe_section;
}

/// Main interface to parse PE binaries. In particular, the **static**
/// Parser::parse functions should be used to get a LIEF::PE::Binary instance.
class LIEF_API Parser : public LIEF::Parser {
  public:
  /// Maximum size of the data read
  static constexpr size_t MAX_DATA_SIZE = 3_GB;

  static constexpr size_t MAX_TLS_CALLBACKS = 3000;

  // Bounds the import thunk loop to prevent hangs on malformed IATs
  static constexpr size_t MAX_IMPORT_ENTRIES = 0x10000;

  // Bounds peek_string_at to prevent multi-megabyte reads on invalid RVAs
  // According to https://stackoverflow.com/a/23340781
  static constexpr size_t MAX_IMPORT_NAME_SIZE = 0x1000;

  // According to https://stackoverflow.com/a/265782/87207
  static constexpr size_t MAX_DLL_NAME_SIZE = 255;

  /// Max size of the padding section
  static constexpr size_t MAX_PADDING_SIZE = 1_GB;

  public:
  /// Check if the given name is a valid import.
  ///
  /// This check verifies that:
  ///   1. The name is not too large or empty (cf.
  ///   https://stackoverflow.com/a/23340781)
  ///   2. All the characters are printable
  static bool is_valid_import_name(const std::string& name);

  /// Check if the given name is a valid DLL name.
  ///
  /// This check verifies that:
  ///   1. The name of the DLL is at least 4 characters long
  ///   2. All the characters are printable
  static bool is_valid_dll_name(const std::string& name);

  public:
  /// Parse a PE binary from the given filename
  static std::unique_ptr<Binary>
      parse(const std::string& filename,
            const ParserConfig& conf = ParserConfig::default_conf());

  /// Parse a PE binary from a data buffer
  static std::unique_ptr<Binary>
      parse(std::vector<uint8_t> data,
            const ParserConfig& conf = ParserConfig::default_conf());

  static std::unique_ptr<Binary>
      parse(const uint8_t* buffer, size_t size,
            const ParserConfig& conf = ParserConfig::default_conf());

  /// Parse a PE binary from the given BinaryStream
  static std::unique_ptr<Binary>
      parse(std::unique_ptr<BinaryStream> stream,
            const ParserConfig& conf = ParserConfig::default_conf());

  /// Parse the PE binary at the given memory address
  static std::unique_ptr<Binary>
      parse_from_memory(uintptr_t address,
                        const ParserConfig& config = ParserConfig::default_conf());

  /// Parse the PE binary at the given memory address and with the given size
  static std::unique_ptr<Binary>
      parse_from_memory(uintptr_t address, size_t size,
                        const ParserConfig& config = ParserConfig::default_conf());

  /// Parse a PE binary from a memory dump located on disk.
  ///
  /// A dump is a raw capture of the process memory that was mapped starting at
  /// the virtual address `addr`. This is typically used to parse a PE image that
  /// has been dumped from memory (e.g. from a debugger or a runtime hook).
  ///
  /// @param[in] filepath Path to the file that contains the memory dump
  /// @param[in] addr     Virtual address at which the dump was mapped
  /// @param[in] config   Optional configuration for the parser
  static std::unique_ptr<Binary>
      parse_from_dump(const std::string& filepath, uint64_t addr,
                      const ParserConfig& config = ParserConfig::default_conf());

  /// Same as parse_from_dump(const std::string&, uint64_t, const ParserConfig&)
  /// but the dump is wrapped in the given **non-owned** stream.
  static std::unique_ptr<Binary>
      parse_from_dump(BinaryStream& stream, uint64_t addr,
                      const ParserConfig& config = ParserConfig::default_conf());

  /// Same as parse_from_dump(const std::string&, uint64_t, const ParserConfig&)
  /// but the dump is wrapped in the given **owned** stream.
  static std::unique_ptr<Binary>
      parse_from_dump(std::unique_ptr<BinaryStream> stream, uint64_t addr,
                      const ParserConfig& config = ParserConfig::default_conf());

  Parser& operator=(const Parser& copy) = delete;
  Parser(const Parser& copy) = delete;

  COFF::String* find_coff_string(uint32_t offset) const;

  ExceptionInfo* find_exception_info(uint32_t rva) const {
    auto it = memoize_exception_info_.find(rva);
    return it == memoize_exception_info_.end() ? nullptr : it->second;
  }

  const Binary& bin() const {
    return *binary_;
  }

  Binary& bin() {
    return *binary_;
  }

  BinaryStream& stream() {
    return *stream_;
  }

  const ParserConfig& config() const {
    return config_;
  }

  void memoize(ExceptionInfo& info);
  void memoize(COFF::String str);

  void add_non_resolved(ExceptionInfo& info, uint32_t target) {
    unresolved_chains_.emplace_back(&info, target);
  }

  std::unique_ptr<SpanStream> stream_from_rva(uint32_t rva, size_t size = 0);

  void record_relocation(uint32_t rva, span<const uint8_t> data);
  ok_error_t record_delta_relocation(uint32_t rva, int64_t delta, size_t size);

  private:
  struct relocation_t {
    uint64_t size = 0;
    uint64_t value = 0;
  };
  Parser(const std::string& file);
  Parser(std::vector<uint8_t> data);
  Parser(std::unique_ptr<BinaryStream> stream);

  ~Parser() override;
  Parser();

  ok_error_t init(const ParserConfig& config);

  template<typename PE_T>
  ok_error_t parse();

  ok_error_t parse_exports();
  ok_error_t parse_sections();

  ok_error_t read_section_content(const details::pe_section& raw_sec,
                                  uint32_t index, Section& section);

  template<typename PE_T>
  ok_error_t parse_headers();

  ok_error_t parse_configuration();

  template<typename PE_T>
  ok_error_t parse_data_directories();

  template<typename PE_T>
  ok_error_t parse_import_table();

  template<typename PE_T>
  ok_error_t parse_delay_imports();

  template<typename PE_T>
  ok_error_t parse_delay_names_table(DelayImport& import, uint32_t names_offset,
                                     uint32_t iat_offset);

  ok_error_t parse_export_table();
  ok_error_t parse_debug();

  ok_error_t parse_exceptions();

  std::unique_ptr<Debug> parse_code_view(const details::pe_debug& debug_info,
                                         Section* sec, span<uint8_t> payload);
  std::unique_ptr<Debug> parse_pogo(const details::pe_debug& debug_info,
                                    Section* sec, span<uint8_t> payload);
  std::unique_ptr<Debug> parse_repro(const details::pe_debug& debug_info,
                                     Section* sec, span<uint8_t> payload);

  template<typename PE_T>
  ok_error_t parse_tls();

  template<typename PE_T>
  ok_error_t parse_load_config();

  template<typename PE_T>
  ok_error_t process_load_config(LoadConfiguration& config);

  template<typename PE_T>
  ok_error_t parse_nested_relocated();

  ok_error_t parse_relocations();
  ok_error_t parse_resources();
  ok_error_t parse_string_table();
  ok_error_t parse_symbols();
  ok_error_t parse_signature();
  ok_error_t parse_overlay();
  ok_error_t parse_dos_stub();
  ok_error_t parse_rich_header();
  ok_error_t parse_chpe_exceptions();

  template<typename PE_T>
  ok_error_t undo_relocations();

  template<typename PE_T>
  ok_error_t fix_iat();

  template<typename PE_T>
  ok_error_t fix_tls();

  template<typename PE_T>
  ok_error_t fix_load_config();

  PE_TYPE type_ = PE_TYPE::PE32_PLUS;
  std::unique_ptr<Binary> binary_;
  std::unique_ptr<BinaryStream> stream_;
  std::map<uint32_t, size_t> memoize_coff_str_;
  std::map<uint32_t, ExceptionInfo*> memoize_exception_info_;
  std::map<uint32_t, relocation_t> dyn_hdr_relocs_;
  std::vector<std::pair<ExceptionInfo*, uint32_t>> unresolved_chains_;
  ParserConfig config_;
};


}
}
#endif
