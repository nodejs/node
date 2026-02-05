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
#ifndef LIEF_PE_TLS_H
#define LIEF_PE_TLS_H

#include <vector>
#include <ostream>

#include "LIEF/span.hpp"
#include "LIEF/Object.hpp"
#include "LIEF/visibility.h"

namespace LIEF {
namespace PE {

class Parser;
class Builder;
class DataDirectory;
class Section;
class Binary;

namespace details {
struct pe32_tls;
struct pe64_tls;
}

/// Class which represents the PE Thread Local Storage
///
/// This PE structure is also used to implement binary/library constructors.
class LIEF_API TLS : public Object {
  friend class Parser;
  friend class Builder;
  friend class Binary;

  public:
  TLS() = default;

  LIEF_LOCAL TLS(const details::pe32_tls& header);
  LIEF_LOCAL TLS(const details::pe64_tls& header);

  ~TLS() override = default;

  TLS(const TLS& copy) = default;
  TLS& operator=(const TLS& copy) = default;

  TLS(TLS&& other) noexcept = default;
  TLS& operator=(TLS&& other) noexcept = default;

  /// List of the callback associated with the current TLS.
  ///
  /// These functions are called before any other functions.
  const std::vector<uint64_t>& callbacks() const {
    return callbacks_;
  }

  /// Add a new TLS callback
  TLS& add_callback(uint64_t addr) {
    callbacks_.push_back(addr);
    return *this;
  }

  /// Pair `(start address, end address)` of the TLS template.
  /// The template is a block of data that is used to initialize TLS data.
  /// The system copies all of this data each time a thread is created, so it
  /// must not be corrupted.
  ///
  /// \note These addresses are not RVA. It is addresses for which there
  ///       should be a rebase relocation in the `.reloc` section.
  const std::pair<uint64_t, uint64_t>& addressof_raw_data() const {
    return va_rawdata_;
  }

  /// The location to receive the TLS index assigned by the loader.
  /// This location should be located in a writable section like `.data`
  uint64_t addressof_index() const {
    return addressof_index_;
  }

  /// Pointer to an array of TLS callback functions.
  ///
  /// The array is null-terminated, so if there is no callback function
  /// this field points to **4 bytes** set to zero.
  uint64_t addressof_callbacks() const {
    return addressof_callbacks_;
  }

  /// Size in bytes of the zero to be *padded* after the data specified by
  /// data_template.
  uint32_t sizeof_zero_fill() const {
    return sizeof_zero_fill_;
  }

  /// The four bits [23:20] describe alignment info.
  /// Possible values are those defined as `IMAGE_SCN_ALIGN_*`, which are also
  /// used to describe alignment of section in object files.
  /// The other 28 bits are reserved for future use.
  uint32_t characteristics() const {
    return characteristics_;
  }

  /// The initial content used to initialize TLS data.
  span<const uint8_t> data_template() const {
    return data_template_;
  }

  /// True if there is a data directory associated with this entry
  bool has_data_directory() const {
    return directory_ != nullptr;
  }

  /// Return the DataDirectory associated with this object or a nullptr
  /// If it exists, its type should be DataDirectory::TYPES::TLS_TABLE
  DataDirectory* directory() {
    return directory_;
  }

  const DataDirectory* directory() const {
    return directory_;
  }

  /// Check if there is a section associated with this entry
  bool has_section() const {
    return section_ != nullptr;
  }

  /// The section associated with the entry (or a nullptr)
  Section* section() {
    return section_;
  }

  const Section* section() const {
    return section_;
  }

  void callbacks(std::vector<uint64_t> callbacks) {
    callbacks_ = std::move(callbacks);
  }

  void addressof_raw_data(std::pair<uint64_t, uint64_t> addresses) {
    va_rawdata_ = addresses;
  }

  void addressof_index(uint64_t addr_idx) {
    addressof_index_ = addr_idx;
  }

  void addressof_callbacks(uint64_t addr) {
    addressof_callbacks_ = addr;
  }

  void sizeof_zero_fill(uint32_t size) {
    sizeof_zero_fill_ = size;
  }

  void characteristics(uint32_t characteristics) {
    characteristics_ = characteristics;
  }

  void data_template(std::vector<uint8_t> data_template) {
    data_template_ = std::move(data_template);
  }

  void accept(Visitor& visitor) const override;

  LIEF_API friend std::ostream& operator<<(std::ostream& os, const TLS& entry);

  private:
  std::vector<uint64_t> callbacks_;
  std::pair<uint64_t, uint64_t> va_rawdata_;
  uint64_t addressof_index_ = 0;
  uint64_t addressof_callbacks_ = 0;
  uint32_t sizeof_zero_fill_ = 0;
  uint32_t characteristics_ = 0;
  DataDirectory* directory_ = nullptr;
  Section* section_ = nullptr;
  std::vector<uint8_t> data_template_;
};
}
}
#endif
