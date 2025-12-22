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
#ifndef LIEF_PE_DATADIRECTORY_H
#define LIEF_PE_DATADIRECTORY_H

#include <cstdint>
#include <ostream>
#include <memory>

#include "LIEF/Object.hpp"
#include "LIEF/visibility.h"
#include "LIEF/span.hpp"

namespace LIEF {
class SpanStream;

namespace PE {

class Builder;
class Parser;
class Binary;
class Section;

namespace details {
struct pe_data_directory;
}

/// Class that represents a PE data directory entry
class LIEF_API DataDirectory : public Object {

  friend class Builder;
  friend class Parser;
  friend class Binary;

  public:
  static constexpr size_t DEFAULT_NB = 16;

  enum class TYPES: uint32_t  {
    EXPORT_TABLE = 0,
    IMPORT_TABLE,
    RESOURCE_TABLE,
    EXCEPTION_TABLE,
    CERTIFICATE_TABLE,
    BASE_RELOCATION_TABLE,
    DEBUG_DIR,
    ARCHITECTURE,
    GLOBAL_PTR,
    TLS_TABLE,
    LOAD_CONFIG_TABLE,
    BOUND_IMPORT,
    IAT,
    DELAY_IMPORT_DESCRIPTOR,
    CLR_RUNTIME_HEADER,
    RESERVED,

    UNKNOWN,
  };
  DataDirectory() = default;
  DataDirectory(TYPES type) :
    type_{type}
  {}

  DataDirectory(const details::pe_data_directory& header, TYPES type);

  DataDirectory(const DataDirectory& other) = default;
  DataDirectory& operator=(const DataDirectory& other) = default;

  DataDirectory(DataDirectory&& other) noexcept = default;
  DataDirectory& operator=(DataDirectory&& other) noexcept = default;

  ~DataDirectory() override = default;

  /// The relative virtual address of the content of this data
  /// directory
  uint32_t RVA() const {
    return rva_;
  }

  /// The size of the content
  uint32_t size() const {
    return size_;
  }

  /// Check if the content of this data directory is associated
  /// with a PE Section
  bool has_section() const {
    return section_ != nullptr;
  }

  /// Raw content (bytes) referenced by this data directory
  span<const uint8_t> content() const {
    return const_cast<DataDirectory*>(this)->content();
  }

  span<uint8_t> content();

  /// Section associated with the DataDirectory
  Section* section() {
    return section_;
  }

  const Section* section() const {
    return section_;
  }

  /// Type of the data directory
  TYPES type() const {
    return type_;
  }

  void size(uint32_t size) {
    size_ = size;
  }

  void RVA(uint32_t rva) {
    rva_ = rva;
  }

  void accept(Visitor& visitor) const override;

  LIEF_API friend
    std::ostream& operator<<(std::ostream& os, const DataDirectory& entry);

  /// \private
  LIEF_LOCAL
    std::unique_ptr<SpanStream> stream(bool sized = true) const;

  private:
  uint32_t rva_ = 0;
  uint32_t size_ = 0;
  TYPES type_ = TYPES::UNKNOWN;
  Section* section_ = nullptr;
};

LIEF_API const char* to_string(DataDirectory::TYPES e);

}
}

#endif /* LIEF_PE_DATADIRECTORY_H */
