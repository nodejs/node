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
#ifndef LIEF_PE_LOAD_CONFIGURATION_CHPE_METADATA_ARM64_H
#define LIEF_PE_LOAD_CONFIGURATION_CHPE_METADATA_ARM64_H
#include <memory>
#include <cstdint>

#include "LIEF/errors.hpp"
#include "LIEF/visibility.h"
#include "LIEF/iterators.hpp"
#include "LIEF/PE/LoadConfigurations/CHPEMetadata/Metadata.hpp"

namespace LIEF {
namespace PE {

/// This class represents ARM64-specific metadata used in CHPE
/// (Compatible Hybrid PE) binaries, particularly for hybrid architectures like
/// ARM64EC and ARM64X.
///
/// It extends the CHPEMetadata base class and provides access to metadata
/// describing code ranges, redirections, entry points, and other hybrid-specific
/// information relevant for binary analysis or instrumentation.
class LIEF_API CHPEMetadataARM64 : public CHPEMetadata {
  public:
  /// Structure that describes architecture-specific ranges
  struct range_entry_t {
    static constexpr uint32_t TYPE_MASK = 3;

    enum class TYPE {
      /// Pure ARM64 code
      ARM64 = 0,

      /// ARM64EC hybrid code (compatible with x64).
      ARM64EC = 1,

      /// x64 code.
      AMD64 = 2,
    };

    /// Raw data (include start RVA and type)
    uint32_t start_offset = 0;

    /// Range's length
    uint32_t length = 0;

    /// Start of the range (RVA)
    uint32_t start() const {
      return start_offset & ~TYPE_MASK;
    }

    /// Architecture for this range
    TYPE type() const {
      return TYPE(start_offset & TYPE_MASK);
    }

    /// End of the range (RVA)
    uint32_t end() const {
      return start() + length;
    }
  };

  /// Structure that describes a redirection
  struct redirection_entry_t {
    uint32_t src = 0;
    uint32_t dst = 0;
  };

  /// Mirror of `IMAGE_ARM64EC_CODE_RANGE_ENTRY_POINT`:
  /// Represents a mapping between code range and its entry point.
  struct code_range_entry_point_t {
    /// Start of the code range.
    uint32_t start_rva = 0;

    /// End of the code range (RVA).
    uint32_t end_rva = 0;

    /// RVA of the entry point for this range.
    uint32_t entrypoint = 0;
  };

  using range_entries_t = std::vector<range_entry_t>;
  using it_range_entries = ref_iterator<range_entries_t&>;
  using it_const_range_entries = const_ref_iterator<const range_entries_t&>;

  using redirection_entries_t = std::vector<redirection_entry_t>;
  using it_redirection_entries = ref_iterator<redirection_entries_t&>;
  using it_const_redirection_entries = const_ref_iterator<const redirection_entries_t&>;

  using code_range_entry_point_entries = std::vector<code_range_entry_point_t>;
  using it_code_range_entry_point = ref_iterator<code_range_entry_point_entries&>;
  using it_const_code_range_entry_point = const_ref_iterator<const code_range_entry_point_entries&>;

  CHPEMetadataARM64(uint32_t version) :
    CHPEMetadata(KIND::ARM64, version)
  {}

  CHPEMetadataARM64(const CHPEMetadataARM64&) = default;
  CHPEMetadataARM64& operator=(const CHPEMetadataARM64&) = default;

  CHPEMetadataARM64(CHPEMetadataARM64&&) = default;
  CHPEMetadataARM64& operator=(CHPEMetadataARM64&&) = default;

  std::unique_ptr<CHPEMetadata> clone() const override {
    return std::unique_ptr<CHPEMetadataARM64>(new CHPEMetadataARM64(*this));
  }

  static std::unique_ptr<CHPEMetadataARM64> parse(
    Parser& ctx, BinaryStream& stream, uint32_t version);

  static ok_error_t parse_code_map(Parser& ctx, CHPEMetadataARM64& metadata);

  static ok_error_t parse_redirections(Parser& ctx, CHPEMetadataARM64& metadata);

  static ok_error_t parse_code_ranges_to_entry_points(
      Parser& ctx, CHPEMetadataARM64& metadata);

  /// RVA to the array that describes architecture-specific ranges
  uint32_t code_map() const {
    return code_map_rva_;
  }

  /// Number of entries in the code map
  uint32_t code_map_count() const {
    return code_map_cnt_;
  }

  uint32_t code_ranges_to_entrypoints() const {
    return code_ranges_to_ep_;
  }

  uint32_t redirection_metadata() const {
    return redirection_metadata_;
  }

  uint32_t os_arm64x_dispatch_call_no_redirect() const {
    return os_arm64x_dispatch_call_no_redirect_;
  }

  uint32_t os_arm64x_dispatch_ret() const {
    return os_arm64x_dispatch_ret_;
  }

  uint32_t os_arm64x_dispatch_call() const {
    return os_arm64x_dispatch_call_;
  }

  uint32_t os_arm64x_dispatch_icall() const {
    return os_arm64x_dispatch_icall_;
  }

  uint32_t os_arm64x_dispatch_icall_cfg() const {
    return os_arm64x_dispatch_icall_cfg_;
  }

  uint32_t alternate_entry_point() const {
    return alternate_ep_;
  }

  uint32_t auxiliary_iat() const {
    return aux_iat_;
  }

  uint32_t code_ranges_to_entry_points_count() const {
    return code_ranges_to_ep_cnt_;
  }

  uint32_t redirection_metadata_count() const {
    return redirection_metadata_cnt_;
  }

  uint32_t get_x64_information_function_pointer() const {
    return get_x64_information_function_pointer_;
  }

  uint32_t set_x64_information_function_pointer() const {
    return set_x64_information_function_pointer_;
  }

  /// RVA to this architecture-specific exception table
  uint32_t extra_rfe_table() const {
    return extra_rfe_table_;
  }

  /// architecture-specific exception table size
  uint32_t extra_rfe_table_size() const {
    return extra_rfe_table_size_;
  }

  uint32_t os_arm64x_dispatch_fptr() const {
    return os_arm64x_dispatch_fptr_;
  }

  uint32_t auxiliary_iat_copy() const {
    return aux_iat_copy_;
  }

  uint32_t auxiliary_delay_import() const {
    return aux_delay_import_;
  }

  uint32_t auxiliary_delay_import_copy() const {
    return aux_delay_import_copy_;
  }

  uint32_t bitfield_info() const {
    return bitfield_info_;
  }

  it_range_entries code_ranges() {
    return range_entries_;
  }

  it_const_range_entries code_ranges() const {
    return range_entries_;
  }

  it_redirection_entries redirections() {
    return redirection_entries_;
  }

  it_const_redirection_entries redirections() const {
    return redirection_entries_;
  }

  it_code_range_entry_point code_range_entry_point() {
    return code_range_entry_point_entries_;
  }

  it_const_code_range_entry_point code_range_entry_point() const {
    return code_range_entry_point_entries_;
  }

  CHPEMetadataARM64& code_map(uint32_t value) {
    code_map_rva_ = value;
    return *this;
  }

  CHPEMetadataARM64& code_map_count(uint32_t value) {
    code_map_cnt_ = value;
    return *this;
  }

  CHPEMetadataARM64& code_ranges_to_entrypoints(uint32_t value) {
    code_ranges_to_ep_ = value;
    return *this;
  }

  CHPEMetadataARM64& redirection_metadata(uint32_t value) {
    redirection_metadata_ = value;
    return *this;
  }

  CHPEMetadataARM64& os_arm64x_dispatch_call_no_redirect(uint32_t value) {
    os_arm64x_dispatch_call_no_redirect_ = value;
    return *this;
  }

  CHPEMetadataARM64& os_arm64x_dispatch_ret(uint32_t value) {
    os_arm64x_dispatch_ret_ = value;
    return *this;
  }

  CHPEMetadataARM64& os_arm64x_dispatch_call(uint32_t value) {
    os_arm64x_dispatch_call_ = value;
    return *this;
  }

  CHPEMetadataARM64& os_arm64x_dispatch_icall(uint32_t value) {
    os_arm64x_dispatch_icall_ = value;
    return *this;
  }

  CHPEMetadataARM64& os_arm64x_dispatch_icall_cfg(uint32_t value) {
    os_arm64x_dispatch_icall_cfg_ = value;
    return *this;
  }

  CHPEMetadataARM64& alternate_entry_point(uint32_t value) {
    alternate_ep_ = value;
    return *this;
  }

  CHPEMetadataARM64& auxiliary_iat(uint32_t value) {
    aux_iat_ = value;
    return *this;
  }

  CHPEMetadataARM64& code_ranges_to_entry_points_count(uint32_t value) {
    code_ranges_to_ep_cnt_ = value;
    return *this;
  }

  CHPEMetadataARM64& redirection_metadata_count(uint32_t value) {
    redirection_metadata_cnt_ = value;
    return *this;
  }

  CHPEMetadataARM64& get_x64_information_function_pointer(uint32_t value) {
    get_x64_information_function_pointer_ = value;
    return *this;
  }

  CHPEMetadataARM64& set_x64_information_function_pointer(uint32_t value) {
    set_x64_information_function_pointer_ = value;
    return *this;
  }

  CHPEMetadataARM64& extra_rfe_table(uint32_t value) {
    extra_rfe_table_ = value;
    return *this;
  }

  CHPEMetadataARM64& extra_rfe_table_size(uint32_t value) {
    extra_rfe_table_size_ = value;
    return *this;
  }

  CHPEMetadataARM64& os_arm64x_dispatch_fptr(uint32_t value) {
    os_arm64x_dispatch_fptr_ = value;
    return *this;
  }

  CHPEMetadataARM64& auxiliary_iat_copy(uint32_t value) {
    aux_iat_copy_ = value;
    return *this;
  }

  CHPEMetadataARM64& auxiliary_delay_import(uint32_t value) {
    aux_delay_import_ = value;
    return *this;
  }

  CHPEMetadataARM64& auxiliary_delay_import_copy(uint32_t value) {
    aux_delay_import_copy_ = value;
    return *this;
  }

  CHPEMetadataARM64& bitfield_info(uint32_t value) {
    bitfield_info_ = value;
    return *this;
  }

  static bool classof(const CHPEMetadata* meta) {
    return meta->kind() == KIND::ARM64;
  }

  std::string to_string() const override;

  ~CHPEMetadataARM64() override = default;

  private:
  uint32_t code_map_rva_ = 0;
  uint32_t code_map_cnt_ = 0;
  uint32_t code_ranges_to_ep_ = 0;
  uint32_t redirection_metadata_ = 0;
  uint32_t os_arm64x_dispatch_call_no_redirect_ = 0;
  uint32_t os_arm64x_dispatch_ret_ = 0;
  uint32_t os_arm64x_dispatch_call_ = 0;
  uint32_t os_arm64x_dispatch_icall_ = 0;
  uint32_t os_arm64x_dispatch_icall_cfg_ = 0;
  uint32_t alternate_ep_ = 0;
  uint32_t aux_iat_ = 0;
  uint32_t code_ranges_to_ep_cnt_ = 0;
  uint32_t redirection_metadata_cnt_ = 0;
  uint32_t get_x64_information_function_pointer_ = 0;
  uint32_t set_x64_information_function_pointer_ = 0;
  uint32_t extra_rfe_table_ = 0;
  uint32_t extra_rfe_table_size_ = 0;
  uint32_t os_arm64x_dispatch_fptr_ = 0;
  uint32_t aux_iat_copy_ = 0;
  uint32_t aux_delay_import_ = 0;
  uint32_t aux_delay_import_copy_ = 0;
  uint32_t bitfield_info_ = 0;

  range_entries_t range_entries_;
  redirection_entries_t redirection_entries_;
  code_range_entry_point_entries code_range_entry_point_entries_;

};

LIEF_API const char* to_string(CHPEMetadataARM64::range_entry_t::TYPE e);
}
}

#endif
