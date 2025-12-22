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
#ifndef LIEF_PE_LOAD_CONFIGURATION_CHPE_METADATA_X86_H
#define LIEF_PE_LOAD_CONFIGURATION_CHPE_METADATA_X86_H
#include <memory>
#include <cstdint>

#include "LIEF/optional.hpp"
#include "LIEF/visibility.h"
#include "LIEF/PE/LoadConfigurations/CHPEMetadata/Metadata.hpp"

namespace LIEF {
namespace PE {

/// This class represents hybrid metadata for X86.
class LIEF_API CHPEMetadataX86 : public CHPEMetadata {
  public:
  CHPEMetadataX86(uint32_t version) :
    CHPEMetadata(KIND::X86, version)
  {}

  CHPEMetadataX86(const CHPEMetadataX86&) = default;
  CHPEMetadataX86& operator=(const CHPEMetadataX86&) = default;

  CHPEMetadataX86(CHPEMetadataX86&&) = default;
  CHPEMetadataX86& operator=(CHPEMetadataX86&&) = default;

  std::unique_ptr<CHPEMetadata> clone() const override {
    return std::unique_ptr<CHPEMetadataX86>(new CHPEMetadataX86(*this));
  }

  static std::unique_ptr<CHPEMetadataX86> parse(
    Parser& ctx, BinaryStream& stream, uint32_t version);


  uint32_t chpe_code_address_range_offset() const {
    return chpe_code_address_range_offset_;
  }

  uint32_t chpe_code_address_range_count() const {
    return chpe_code_address_range_count_;
  }

  uint32_t wowa64_exception_handler_function_pointer() const {
    return wowa64_exception_handler_function_pointer_;
  }

  uint32_t wowa64_dispatch_call_function_pointer() const {
    return wowa64_dispatch_call_function_pointer_;
  }

  uint32_t wowa64_dispatch_indirect_call_function_pointer() const {
    return wowa64_dispatch_indirect_call_function_pointer_;
  }

  uint32_t wowa64_dispatch_indirect_call_cfg_function_pointer() const {
    return wowa64_dispatch_indirect_call_cfg_function_pointer_;
  }

  uint32_t wowa64_dispatch_ret_function_pointer() const {
    return wowa64_dispatch_ret_function_pointer_;
  }

  uint32_t wowa64_dispatch_ret_leaf_function_pointer() const {
    return wowa64_dispatch_ret_leaf_function_pointer_;
  }

  uint32_t wowa64_dispatch_jump_function_pointer() const {
    return wowa64_dispatch_jump_function_pointer_;
  }

  optional<uint32_t> compiler_iat_pointer() const {
    return compiler_iat_pointer_;
  }

  optional<uint32_t> wowa64_rdtsc_function_pointer() const {
    return wowa64_rdtsc_function_pointer_;
  }

  CHPEMetadataX86& chpe_code_address_range_offset(uint32_t value) {
    chpe_code_address_range_offset_ = value;
    return *this;
  }

  CHPEMetadataX86& chpe_code_address_range_count(uint32_t value) {
    chpe_code_address_range_count_ = value;
    return *this;
  }

  CHPEMetadataX86& wowa64_exception_handler_function_pointer(uint32_t value) {
    wowa64_exception_handler_function_pointer_ = value;
    return *this;
  }

  CHPEMetadataX86& wowa64_dispatch_call_function_pointer(uint32_t value) {
    wowa64_dispatch_call_function_pointer_ = value;
    return *this;
  }

  CHPEMetadataX86& wowa64_dispatch_indirect_call_function_pointer(uint32_t value) {
    wowa64_dispatch_indirect_call_function_pointer_ = value;
    return *this;
  }

  CHPEMetadataX86& wowa64_dispatch_indirect_call_cfg_function_pointer(uint32_t value) {
    wowa64_dispatch_indirect_call_cfg_function_pointer_ = value;
    return *this;
  }

  CHPEMetadataX86& wowa64_dispatch_ret_function_pointer(uint32_t value) {
    wowa64_dispatch_ret_function_pointer_ = value;
    return *this;
  }

  CHPEMetadataX86& wowa64_dispatch_ret_leaf_function_pointer(uint32_t value) {
    wowa64_dispatch_ret_leaf_function_pointer_ = value;
    return *this;
  }

  CHPEMetadataX86& wowa64_dispatch_jump_function_pointer(uint32_t value) {
    wowa64_dispatch_jump_function_pointer_ = value;
    return *this;
  }

  CHPEMetadataX86& compiler_iat_pointer(uint32_t value) {
    compiler_iat_pointer_ = value;
    return *this;
  }

  CHPEMetadataX86& wowa64_rdtsc_function_pointer(uint32_t value) {
    wowa64_rdtsc_function_pointer_ = value;
    return *this;
  }

  static bool classof(const CHPEMetadata* meta) {
    return meta->kind() == KIND::X86;
  }

  std::string to_string() const override;

  ~CHPEMetadataX86() override = default;

  private:
  uint32_t chpe_code_address_range_offset_ = 0;
  uint32_t chpe_code_address_range_count_ = 0;
  uint32_t wowa64_exception_handler_function_pointer_ = 0;
  uint32_t wowa64_dispatch_call_function_pointer_ = 0;
  uint32_t wowa64_dispatch_indirect_call_function_pointer_ = 0;
  uint32_t wowa64_dispatch_indirect_call_cfg_function_pointer_ = 0;
  uint32_t wowa64_dispatch_ret_function_pointer_ = 0;
  uint32_t wowa64_dispatch_ret_leaf_function_pointer_ = 0;
  uint32_t wowa64_dispatch_jump_function_pointer_ = 0;
  optional<uint32_t> compiler_iat_pointer_;
  optional<uint32_t> wowa64_rdtsc_function_pointer_;
};

}
}

#endif
