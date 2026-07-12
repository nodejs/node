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
#ifndef LIEF_COFF_BIGOBJ_HEADER_H
#define LIEF_COFF_BIGOBJ_HEADER_H
#include <cstdint>
#include <array>

#include "LIEF/COFF/Header.hpp"

#include "LIEF/visibility.h"
#include "LIEF/span.hpp"

namespace LIEF {
namespace COFF {

/// This class represents the header for a COFF object compiled
/// with `/bigobj` support (i.e. the number of sections can exceed 65536).
///
/// The raw definition of the bigobj header is located in `winnt.h` and named
/// `ANON_OBJECT_HEADER_BIGOBJ`
class LIEF_API BigObjHeader : public Header {
  public:
  static constexpr auto UUID_SZ = 16;
  BigObjHeader() :
    Header(KIND::BIGOBJ)
  {}

  static std::unique_ptr<BigObjHeader> create(BinaryStream& stream);

  BigObjHeader& operator=(const BigObjHeader&) = default;
  BigObjHeader(const BigObjHeader&) = default;

  BigObjHeader& operator=(BigObjHeader&&) = default;
  BigObjHeader(BigObjHeader&&) = default;

  std::unique_ptr<Header> clone() const override {
    return std::unique_ptr<Header>(new BigObjHeader(*this));
  }

  /// The version of this header which must be >= 2
  uint16_t version() const {
    return version_;
  }

  /// Originally named `ClassID`, this uuid should match: `{D1BAA1C7-BAEE-4ba9-AF20-FAF66AA4DCB8}`
  span<const uint8_t> uuid() const {
    return uuid_;
  }

  /// Size of data that follows the header
  uint32_t sizeof_data() const {
    return sizeof_data_;
  }

  /// 1 means that it contains metadata
  uint32_t flags() const {
    return flags_;
  }

  /// Size of CLR metadata
  uint32_t metadata_size() const {
    return metadata_size_;
  }

  /// Offset of CLR metadata
  uint32_t metadata_offset() const {
    return metadata_offset_;
  }

  void version(uint16_t value) {
    version_ = value;
  }

  void sizeof_data(uint32_t value) {
    sizeof_data_ = value;
  }

  void flags(uint32_t value) {
    flags_ = value;
  }

  void metadata_size(uint32_t value) {
    metadata_size_ = value;
  }

  void metadata_offset(uint32_t value) {
    metadata_offset_ = value;
  }

  static bool classof(const Header* header) {
    return header->kind() == Header::KIND::BIGOBJ;
  }

  ~BigObjHeader() override = default;

  std::string to_string() const override;

  protected:
  uint16_t version_ = 0;
  std::array<uint8_t, UUID_SZ> uuid_ = {};

  uint32_t sizeof_data_ = 0;
  uint32_t flags_ = 0;
  uint32_t metadata_size_ = 0;
  uint32_t metadata_offset_ = 0;
};

}
}
#endif
