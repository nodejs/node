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
#ifndef LIEF_PE_SECTION_H
#define LIEF_PE_SECTION_H
#include <ostream>
#include <vector>
#include <string>
#include <memory>

#include "LIEF/iostream.hpp"
#include "LIEF/visibility.h"
#include "LIEF/Abstract/Section.hpp"
#include "LIEF/enums.hpp"

namespace LIEF {
class SpanStream;

namespace COFF {
class String;
}

namespace PE {

class Parser;
class Builder;
class Binary;

namespace details {
struct pe_section;
}

/// Class which represents a PE section
class LIEF_API Section : public LIEF::Section {

  friend class Parser;
  friend class Builder;
  friend class Binary;

  public:
  using LIEF::Section::name;
  static constexpr size_t MAX_SECTION_NAME = 8;

  enum class CHARACTERISTICS: uint64_t  {
    TYPE_NO_PAD            = 0x00000008,
    CNT_CODE               = 0x00000020,
    CNT_INITIALIZED_DATA   = 0x00000040,
    CNT_UNINITIALIZED_DATA = 0x00000080,
    LNK_OTHER              = 0x00000100,
    LNK_INFO               = 0x00000200,
    LNK_REMOVE             = 0x00000800,
    LNK_COMDAT             = 0x00001000,
    GPREL                  = 0x00008000,
    MEM_PURGEABLE          = 0x00010000,
    MEM_16BIT              = 0x00020000,
    MEM_LOCKED             = 0x00040000,
    MEM_PRELOAD            = 0x00080000,
    ALIGN_1BYTES           = 0x00100000,
    ALIGN_2BYTES           = 0x00200000,
    ALIGN_4BYTES           = 0x00300000,
    ALIGN_8BYTES           = 0x00400000,
    ALIGN_16BYTES          = 0x00500000,
    ALIGN_32BYTES          = 0x00600000,
    ALIGN_64BYTES          = 0x00700000,
    ALIGN_128BYTES         = 0x00800000,
    ALIGN_256BYTES         = 0x00900000,
    ALIGN_512BYTES         = 0x00A00000,
    ALIGN_1024BYTES        = 0x00B00000,
    ALIGN_2048BYTES        = 0x00C00000,
    ALIGN_4096BYTES        = 0x00D00000,
    ALIGN_8192BYTES        = 0x00E00000,
    LNK_NRELOC_OVFL        = 0x01000000,
    MEM_DISCARDABLE        = 0x02000000,
    MEM_NOT_CACHED         = 0x04000000,
    MEM_NOT_PAGED          = 0x08000000,
    MEM_SHARED             = 0x10000000,
    MEM_EXECUTE            = 0x20000000,
    MEM_READ               = 0x40000000,
    MEM_WRITE              = 0x80000000
  };

  Section(const details::pe_section& header);
  Section() = default;
  Section(std::string name) :
    Section::Section()
  {
    name_ = std::move(name);
  }

  Section(std::string name, std::vector<uint8_t> content) :
    Section(std::move(name))
  {
    content_ = std::move(content);
    size_ = content_.size();
  }

  Section& operator=(const Section&) = default;
  Section(const Section&) = default;
  ~Section() override = default;

  /// Return the size of the data in the section.
  uint32_t sizeof_raw_data() const;

  /// Return the size of the data when mapped in memory
  ///
  /// If this value is greater than sizeof_raw_data, the section is zero-padded.
  uint32_t virtual_size() const {
    return virtual_size_;
  }

  /// The actual content of the section
  span<const uint8_t> content() const override {
    return content_;
  }

  /// Content of the section's padding area
  span<const uint8_t> padding() const {
    return padding_;
  }

  /// The offset of the section data in the PE file
  uint32_t pointerto_raw_data() const;

  /// The file pointer to the beginning of the COFF relocation entries for the section. This is set to zero for
  /// executable images or if there are no relocations.
  ///
  /// For modern PE binaries, this value is usually set to 0 as the relocations are managed by
  /// PE::Relocation.
  uint32_t pointerto_relocation() const {
    return pointer_to_relocations_;
  }

  /// The file pointer to the beginning of line-number entries for the section.
  /// This is set to zero if there are no COFF line numbers. This value should be zero for an image because COFF
  /// debugging information is deprecated and modern debug information relies on the PDB files.
  uint32_t pointerto_line_numbers() const {
    return pointer_to_linenumbers_;
  }

  /// No longer used in recent PE binaries produced by Visual Studio
  uint16_t numberof_relocations() const {
    return number_of_relocations_;
  }

  /// No longer used in recent PE binaries produced by Visual Studio
  uint16_t numberof_line_numbers() const {
    return number_of_linenumbers_;
  }

  /// Characteristics of the section: it provides information about
  /// the permissions of the section when mapped. It can also provide
  /// information about the *purpose* of the section (contain code, BSS-like, ...)
  uint32_t characteristics() const {
    return characteristics_;
  }

  /// Check if the section has the given CHARACTERISTICS
  bool has_characteristic(CHARACTERISTICS c) const {
    return (characteristics() & (uint32_t)c) > 0;
  }

  /// List of the section characteristics
  std::vector<CHARACTERISTICS> characteristics_list() const {
    return characteristics_to_list(characteristics_);
  }

  /// True if the section can be discarded as needed.
  ///
  /// This is typically the case for debug-related sections
  bool is_discardable() const {
    return has_characteristic(CHARACTERISTICS::MEM_DISCARDABLE);
  }

  /// Fill the content of the section with the given `char`
  void clear(uint8_t c);
  void content(const std::vector<uint8_t>& data) override;

  void name(std::string name) override;

  void virtual_size(uint32_t virtual_sz) {
    virtual_size_ = virtual_sz;
  }

  void pointerto_raw_data(uint32_t ptr) {
    offset(ptr);
  }

  void pointerto_relocation(uint32_t ptr) {
    pointer_to_relocations_ = ptr;
  }

  void pointerto_line_numbers(uint32_t ptr) {
    pointer_to_linenumbers_ = ptr;
  }

  void numberof_relocations(uint16_t nb) {
    number_of_relocations_ = nb;
  }

  void numberof_line_numbers(uint16_t nb) {
    number_of_linenumbers_ = nb;
  }

  void sizeof_raw_data(uint32_t size) {
    this->size(size);
  }

  void characteristics(uint32_t characteristics) {
    characteristics_ = characteristics;
  }

  /// Return the COFF string associated with the section's name (or a nullptr)
  ///
  /// This coff string is usually present for long section names whose length
  /// does not fit in the 8 bytes allocated by the PE format.
  COFF::String* coff_string() {
    return coff_string_;
  }

  const COFF::String* coff_string() const {
    return coff_string_;
  }

  Section& remove_characteristic(CHARACTERISTICS characteristic) {
    characteristics_ &= ~static_cast<size_t>(characteristic);
    return *this;
  }

  Section& add_characteristic(CHARACTERISTICS characteristic) {
    characteristics_ |= static_cast<size_t>(characteristic);
    return *this;
  }

  std::unique_ptr<SpanStream> stream() const;

  /// \private
  LIEF_LOCAL Section& reserve(size_t size, uint8_t value = 0) {
    content_.resize(size, value);
    return *this;
  }

  /// \private
  LIEF_LOCAL vector_iostream edit() {
    return vector_iostream(content_);
  }

  span<uint8_t> writable_content() {
    return content_;
  }

  void accept(Visitor& visitor) const override;

  LIEF_API friend std::ostream& operator<<(std::ostream& os, const Section& section);

  static std::vector<CHARACTERISTICS> characteristics_to_list(uint32_t value);

  private:
  std::vector<uint8_t> content_;
  std::vector<uint8_t> padding_;
  uint32_t virtual_size_           = 0;
  uint32_t pointer_to_relocations_ = 0;
  uint32_t pointer_to_linenumbers_ = 0;
  uint16_t number_of_relocations_  = 0;
  uint16_t number_of_linenumbers_  = 0;
  uint32_t characteristics_        = 0;

  COFF::String* coff_string_ = nullptr;
};

LIEF_API const char* to_string(Section::CHARACTERISTICS e);

} // namespace PE
} // namespace LIEF

ENABLE_BITMASK_OPERATORS(LIEF::PE::Section::CHARACTERISTICS);

#endif
