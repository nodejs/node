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
#ifndef LIEF_PE_DOS_HEADER_H
#define LIEF_PE_DOS_HEADER_H
#include <cstdint>
#include <array>
#include <ostream>

#include "LIEF/Object.hpp"
#include "LIEF/visibility.h"

namespace LIEF {
namespace PE {

enum class PE_TYPE : uint16_t;

namespace details {
struct pe_dos_header;
}

/// Class which represents the DosHeader, the **first**
/// structure presents at the beginning of a PE file.
///
/// Most of the attributes of this structures are no longer relevant.
class LIEF_API DosHeader : public Object {
  public:
  using reserved_t  = std::array<uint16_t, 4>;
  using reserved2_t = std::array<uint16_t, 10>;

  static constexpr uint16_t MAGIC = 0x5a4d; // MZ

  DosHeader(const details::pe_dos_header& header);
  DosHeader(const DosHeader&) = default;
  DosHeader& operator=(const DosHeader&) = default;

  DosHeader(DosHeader&&) = default;
  DosHeader& operator=(DosHeader&&) = default;

  ~DosHeader() override = default;

  /// Magic bytes identifying a DOS/PE binary
  uint16_t magic() const {
    return magic_;
  }

  uint16_t used_bytes_in_last_page() const {
    return used_bytes_in_last_page_;
  }

  uint16_t file_size_in_pages() const {
    return file_sz_in_pages_;
  }

  uint16_t numberof_relocation() const {
    return nb_relocations_;
  }

  uint16_t header_size_in_paragraphs() const {
    return header_sz_in_paragraphs_;
  }

  uint16_t minimum_extra_paragraphs() const {
    return min_extra_paragraphs_;
  }

  uint16_t maximum_extra_paragraphs() const {
    return max_extra_paragraphs_;
  }

  uint16_t initial_relative_ss() const {
    return init_relative_ss_;
  }

  uint16_t initial_sp() const {
    return init_sp_;
  }

  uint16_t checksum() const {
    return checksum_;
  }

  uint16_t initial_ip() const {
    return init_ip_;
  }

  uint16_t initial_relative_cs() const {
    return init_rel_cs_;
  }

  uint16_t addressof_relocation_table() const {
    return addr_reloc_table_;
  }

  uint16_t overlay_number() const {
    return overlay_number_;
  }

  const reserved_t& reserved() const {
    return reserved_;
  }

  uint16_t oem_id() const {
    return oem_id_;
  }

  uint16_t oem_info() const {
    return oem_info_;
  }

  const reserved2_t& reserved2() const {
    return reserved2_;
  }

  /// Return the offset to the PE::Header structure.
  uint32_t addressof_new_exeheader() const {
    return addr_new_exe_header_;
  }

  void magic(uint16_t magic) {
    magic_ = magic;
  }

  void used_bytes_in_last_page(uint16_t value) {
    used_bytes_in_last_page_ = value;
  }

  void file_size_in_pages(uint16_t value) {
    file_sz_in_pages_ = value;
  }

  void numberof_relocation(uint16_t value) {
    nb_relocations_ = value;
  }

  void header_size_in_paragraphs(uint16_t value) {
    header_sz_in_paragraphs_ = value;
  }

  void minimum_extra_paragraphs(uint16_t value) {
    min_extra_paragraphs_ = value;
  }

  void maximum_extra_paragraphs(uint16_t value) {
    max_extra_paragraphs_ = value;
  }

  void initial_relative_ss(uint16_t value) {
    init_relative_ss_ = value;
  }

  void initial_sp(uint16_t value) {
    init_sp_ = value;
  }

  void checksum(uint16_t value) {
    checksum_ = value;
  }

  void initial_ip(uint16_t value) {
    init_ip_ = value;
  }

  void initial_relative_cs(uint16_t value) {
    init_rel_cs_ = value;
  }

  void addressof_relocation_table(uint16_t value) {
    addr_reloc_table_ = value;
  }

  void overlay_number(uint16_t value) {
    overlay_number_ = value;
  }

  void reserved(const reserved_t& reserved) {
    reserved_ = reserved;
  }

  void oem_id(uint16_t value) {
    oem_id_ = value;
  }

  void oem_info(uint16_t value) {
    oem_info_ = value;
  }

  void reserved2(const reserved2_t& reserved2) {
    reserved2_ = reserved2;
  }

  void addressof_new_exeheader(uint32_t value) {
    addr_new_exe_header_ = value;
  }

  void accept(Visitor& visitor) const override;

  LIEF_API friend std::ostream& operator<<(std::ostream& os, const DosHeader& entry);

  static DosHeader create(PE_TYPE type);

  /// \private
  LIEF_LOCAL DosHeader() = default;

  private:
  uint16_t    magic_ = 0;
  uint16_t    used_bytes_in_last_page_ = 0;
  uint16_t    file_sz_in_pages_ = 0;
  uint16_t    nb_relocations_ = 0;
  uint16_t    header_sz_in_paragraphs_ = 0;
  uint16_t    min_extra_paragraphs_ = 0;
  uint16_t    max_extra_paragraphs_ = 0;
  uint16_t    init_relative_ss_ = 0;
  uint16_t    init_sp_ = 0;
  uint16_t    checksum_ = 0;
  uint16_t    init_ip_ = 0;
  uint16_t    init_rel_cs_ = 0;
  uint16_t    addr_reloc_table_ = 0;
  uint16_t    overlay_number_ = 0;
  reserved_t  reserved_ = {0};
  uint16_t    oem_id_ = 0;
  uint16_t    oem_info_ = 0;
  reserved2_t reserved2_ = {0};
  uint32_t    addr_new_exe_header_ = 0;
};
}
}

#endif

