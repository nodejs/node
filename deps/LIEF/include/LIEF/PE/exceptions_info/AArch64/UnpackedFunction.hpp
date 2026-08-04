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
#ifndef LIEF_PE_RUNTIME_FUNCTION_AARCH64_UNPACKED_H
#define LIEF_PE_RUNTIME_FUNCTION_AARCH64_UNPACKED_H

#include "LIEF/span.hpp"
#include "LIEF/visibility.h"
#include "LIEF/iterators.hpp"
#include "LIEF/PE/exceptions_info/RuntimeFunctionAArch64.hpp"

namespace LIEF {
namespace PE {
namespace unwind_aarch64 {

/// This class represents an unpacked AArch64 exception entry
///
/// Reference: https://learn.microsoft.com/en-us/cpp/build/arm64-exception-handling?view=msvc-170#xdata-records
class LIEF_API UnpackedFunction : public RuntimeFunctionAArch64 {
  public:

  /// This structure describes an epilog scope.
  struct epilog_scope_t {
    static epilog_scope_t from_raw(uint32_t raw);
    /// Offset of the epilog relatives to the start of the function
    uint32_t start_offset = 0;

    /// Byte index of the first unwind code that describes this epilog
    uint16_t start_index = 0;

    /// Reserved for future expansion. Should be 0.
    uint8_t reserved = 0;
  };

  using epilog_scopes_t = std::vector<epilog_scope_t>;
  using it_epilog_scopes = ref_iterator<epilog_scopes_t&>;
  using it_const_epilog_scopes = const_ref_iterator<const epilog_scopes_t&>;

  static std::unique_ptr<UnpackedFunction>
    parse(Parser& ctx, BinaryStream& strm, uint32_t xdata_rva, uint32_t rva);

  UnpackedFunction(uint32_t rva, uint32_t length) :
    RuntimeFunctionAArch64(rva, length, PACKED_FLAGS::UNPACKED)
  {}

  UnpackedFunction(const UnpackedFunction&) = default;
  UnpackedFunction& operator=(const UnpackedFunction&) = default;

  UnpackedFunction(UnpackedFunction&&) = default;
  UnpackedFunction& operator=(UnpackedFunction&&) = default;

  ~UnpackedFunction() override = default;

  std::unique_ptr<ExceptionInfo> clone() const override {
    return std::unique_ptr<UnpackedFunction>(new UnpackedFunction(*this));
  }

  std::string to_string() const override;

  /// RVA where this unpacked data is located (usually pointing in `.xdata`)
  uint32_t xdata_rva() const {
    return xdata_rva_;
  }

  /// Describes the version of the remaining `.xdata`.
  ///
  /// Currently (2025-01-04), only version 0 is defined, so values of 1-3 aren't
  /// permitted.
  uint32_t version() const {
    return version_;
  }

  /// 1-bit field that indicates the presence (1) or absence (0) of exception
  /// data.
  uint8_t X() const {
    return x_;
  }

  /// 1-bit field that indicates that information describing a single epilog is
  /// packed into the header (1) rather than requiring more scope words later (0).
  uint8_t E() const {
    return e_;
  }

  /// **If E() == 0**, specifies the count of the total number of epilog scopes.
  /// Otherwise, return 0
  uint16_t epilog_count() const {
    return E() == 1 ? 0 : epilog_count_;
  }

  /// **If E() == 1**, index of the first unwind code that describes the one and
  /// only epilog.
  uint16_t epilog_offset() const {
    return E() == 1 ? epilog_offset_ : uint16_t(-1);
  }

  /// Number of 32-bit words needed to contain all of the unwind codes
  uint32_t code_words() const {
    return code_words_;
  }

  /// Exception handler RVA (if any)
  uint32_t exception_handler() const {
    return exception_handler_;
  }

  /// Bytes that contain the unwind codes.
  span<const uint8_t> unwind_code() const {
    return unwind_code_;
  }

  span<uint8_t> unwind_code() {
    return unwind_code_;
  }

  /// Whether it uses 2-words encoding
  bool is_extended() const {
    return is_extended_;
  }

  uint64_t epilog_scopes_offset() const {
    return epilog_scopes_offset_;
  }

  uint64_t unwind_code_offset() const {
    return unwind_code_offset_;
  }

  uint64_t exception_handler_offset() const {
    return exception_handler_offset_;
  }

  /// Iterator over the epilog scopes
  it_epilog_scopes epilog_scopes() {
    return epilog_scopes_;
  }

  it_const_epilog_scopes epilog_scopes() const {
    return epilog_scopes_;
  }

  UnpackedFunction& xdata_rva(uint32_t value) {
    xdata_rva_ = value;
    return *this;
  }

  UnpackedFunction& version(uint32_t value) {
    version_ = value;
    return *this;
  }

  UnpackedFunction& X(uint8_t value) {
    x_ = value;
    return *this;
  }

  UnpackedFunction& E(uint8_t value) {
    e_ = value;
    return *this;
  }

  UnpackedFunction& epilog_cnt_offset(uint16_t value) {
    epilog_offset_ = value;
    return *this;
  }

  UnpackedFunction& code_words(uint32_t value) {
    code_words_ = value;
    return *this;
  }

  UnpackedFunction& exception_handler(uint32_t value) {
    exception_handler_ = value;
    return *this;
  }

  UnpackedFunction& epilog_scopes(epilog_scopes_t scopes) {
    epilog_scopes_ = std::move(scopes);
    return *this;
  }

  UnpackedFunction& unwind_code(std::vector<uint8_t> code) {
    unwind_code_ = std::move(code);
    return *this;
  }

  UnpackedFunction& is_extended(bool value) {
    is_extended_ = value;
    return *this;
  }

  static bool classof(const ExceptionInfo* info) {
    if (!RuntimeFunctionAArch64::classof(info)) {
      return false;
    }
    const auto* aarch64 = static_cast<const RuntimeFunctionAArch64*>(info);
    return aarch64->flag() == PACKED_FLAGS::UNPACKED;
  }

  private:
  uint32_t xdata_rva_ = 0;
  uint32_t version_ = 0;
  uint8_t x_ = 0;
  uint8_t e_ = 0;
  union {
    uint32_t epilog_count_;
    uint32_t epilog_offset_ = 0;
  };
  uint32_t code_words_ = 0;
  uint32_t exception_handler_ = 0;

  epilog_scopes_t epilog_scopes_;
  std::vector<uint8_t> unwind_code_;
  bool is_extended_ = false;

  uint64_t epilog_scopes_offset_ = 0;
  uint64_t unwind_code_offset_ = 0;
  uint64_t exception_handler_offset_ = 0;
};
}
}
}
#endif
