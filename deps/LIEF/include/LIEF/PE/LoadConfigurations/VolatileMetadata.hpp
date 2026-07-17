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
#ifndef LIEF_PE_LOAD_CONFIGURATION_VOLATILE_METADATA_H
#define LIEF_PE_LOAD_CONFIGURATION_VOLATILE_METADATA_H
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "LIEF/visibility.h"
#include "LIEF/iterators.hpp"

namespace LIEF {
class BinaryStream;
namespace PE {
class Parser;

/// This class represents volatile metadata which can be enabled at link time
/// with `/volatileMetadata`.
///
/// This metadata aims to improve performances when running x64 code on ARM64.
class LIEF_API VolatileMetadata {
  public:
  struct range_t {
    uint32_t start = 0;
    uint32_t size = 0;
    uint32_t end() const {
      return start + size;
    }
  };

  using access_table_t = std::vector<uint32_t>;

  using info_ranges_t = std::vector<range_t>;
  using it_info_ranges_t = ref_iterator<info_ranges_t&>;
  using it_const_info_ranges_t = const_ref_iterator<const info_ranges_t&>;

  VolatileMetadata() = default;
  VolatileMetadata(const VolatileMetadata&) = default;
  VolatileMetadata& operator=(const VolatileMetadata&) = default;

  VolatileMetadata(VolatileMetadata&&) = default;
  VolatileMetadata& operator=(VolatileMetadata&&) = default;

  std::unique_ptr<VolatileMetadata> clone() const {
    return std::unique_ptr<VolatileMetadata>(new VolatileMetadata(*this));
  }

  /// Size (in bytes) of the current raw structure
  uint32_t size() const {
    return size_;
  }

  uint16_t min_version() const {
    return min_version_;
  }

  uint16_t max_version() const {
    return max_version_;
  }

  uint32_t access_table_rva() const {
    return access_table_rva_;
  }

  const access_table_t& access_table() const {
    return access_table_;
  }

  uint32_t access_table_size() const {
    return access_table_.size() * sizeof(uint32_t);
  }

  uint32_t info_range_rva() const {
    return info_range_rva_;
  }

  uint32_t info_ranges_size() const {
    static_assert(sizeof(range_t) == 8, "Can't be used for computing the raw size");
    return info_ranges_.size() * sizeof(range_t);
  }

  it_const_info_ranges_t info_ranges() const {
    return info_ranges_;
  }

  it_info_ranges_t info_ranges() {
    return info_ranges_;
  }

  VolatileMetadata& size(uint32_t value) {
    size_ = value;
    return *this;
  }

  VolatileMetadata& min_version(uint16_t min) {
    min_version_ = min;
    return *this;
  }

  VolatileMetadata& max_version(uint16_t max) {
    max_version_ = max;
    return *this;
  }

  VolatileMetadata& access_table_rva(uint32_t value) {
    access_table_rva_ = value;
    return *this;
  }

  VolatileMetadata& info_range_rva(uint32_t value) {
    info_range_rva_ = value;
    return *this;
  }

  std::string to_string() const;

  LIEF_API friend
    std::ostream& operator<<(std::ostream& os, const VolatileMetadata& meta)
  {
    os << meta.to_string();
    return os;
  }

  /// \private
  LIEF_LOCAL static std::unique_ptr<VolatileMetadata>
    parse(Parser& ctx, BinaryStream& stream);

  private:
  uint32_t size_ = 0;
  uint16_t min_version_ = 0;
  uint16_t max_version_ = 0;
  uint32_t access_table_rva_ = 0;

  uint32_t info_range_rva_ = 0;

  access_table_t access_table_;
  info_ranges_t info_ranges_;

};
}
}

#endif
