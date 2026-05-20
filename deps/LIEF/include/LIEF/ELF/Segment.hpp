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
#ifndef LIEF_ELF_SEGMENT_H
#define LIEF_ELF_SEGMENT_H

#include <string>
#include <vector>
#include <ostream>
#include <memory>

#include "LIEF/Object.hpp"
#include "LIEF/visibility.h"
#include "LIEF/errors.hpp"
#include "LIEF/iterators.hpp"
#include "LIEF/span.hpp"

#include "LIEF/ELF/Header.hpp"

#include "LIEF/ELF/enums.hpp"

namespace LIEF {
class SpanStream;

namespace ELF {
namespace DataHandler {
class Handler;
}

class Parser;
class Binary;
class Section;
class Builder;

/// Class which represents the ELF segments
class LIEF_API Segment : public Object {

  friend class Parser;
  friend class Section;
  friend class Binary;
  friend class Builder;

  public:
  using sections_t        = std::vector<Section*>;
  using it_sections       = ref_iterator<sections_t&>;
  using it_const_sections = const_ref_iterator<const sections_t&>;

  static constexpr uint64_t PT_BIT = 33;
  static constexpr uint64_t PT_OS_BIT = 53;
  static constexpr uint64_t PT_MASK = (uint64_t(1) << PT_BIT) - 1;

  static constexpr uint64_t PT_ARM      = uint64_t(1) << PT_BIT;
  static constexpr uint64_t PT_AARCH64  = uint64_t(2) << PT_BIT;
  static constexpr uint64_t PT_MIPS     = uint64_t(3) << PT_BIT;
  static constexpr uint64_t PT_RISCV    = uint64_t(4) << PT_BIT;
  static constexpr uint64_t PT_IA_64    = uint64_t(5) << PT_BIT;

  static constexpr uint64_t PT_HPUX     = uint64_t(1) << PT_OS_BIT;

  enum class TYPE : uint64_t {
    UNKNOWN       = uint64_t(-1),
    PT_NULL_      = 0, /**< Unused segment. */
    LOAD          = 1, /**< Loadable segment. */
    DYNAMIC       = 2, /**< Dynamic linking information. */
    INTERP        = 3, /**< Interpreter pathname. */
    NOTE          = 4, /**< Auxiliary information. */
    SHLIB         = 5, /**< Reserved. */
    PHDR          = 6, /**< The program header table itself. */
    TLS           = 7, /**< The thread-local storage template. */

    GNU_EH_FRAME  = 0x6474e550,

    GNU_STACK     = 0x6474e551, /**< Indicates stack executability. */
    GNU_PROPERTY  = 0x6474e553, /**< GNU property */
    GNU_RELRO     = 0x6474e552, /**< Read-only after relocation. */
    PAX_FLAGS     = 0x65041580,

    ARM_ARCHEXT   = 0x70000000 | PT_ARM, /**< Platform architecture compatibility info */
    ARM_EXIDX     = 0x70000001 | PT_ARM,

    AARCH64_MEMTAG_MTE = 0x70000002 | PT_AARCH64,

    MIPS_REGINFO  = 0x70000000 | PT_MIPS,  /**< Register usage information. */
    MIPS_RTPROC   = 0x70000001 | PT_MIPS,  /**< Runtime procedure table. */
    MIPS_OPTIONS  = 0x70000002 | PT_MIPS,  /**< Options segment. */
    MIPS_ABIFLAGS = 0x70000003 | PT_MIPS,  /**< Abiflags segment. */

    RISCV_ATTRIBUTES = 0x70000003 | PT_RISCV,

    IA_64_EXT         = (0x70000000 + 0x0) | PT_IA_64,
    IA_64_UNWIND      = (0x70000000 + 0x1) | PT_IA_64,

    HP_TLS            = (0x60000000 + 0x0)  | PT_HPUX,
    HP_CORE_NONE      = (0x60000000 + 0x1)  | PT_HPUX,
    HP_CORE_VERSION   = (0x60000000 + 0x2)  | PT_HPUX,
    HP_CORE_KERNEL    = (0x60000000 + 0x3)  | PT_HPUX,
    HP_CORE_COMM      = (0x60000000 + 0x4)  | PT_HPUX,
    HP_CORE_PROC      = (0x60000000 + 0x5)  | PT_HPUX,
    HP_CORE_LOADABLE  = (0x60000000 + 0x6)  | PT_HPUX,
    HP_CORE_STACK     = (0x60000000 + 0x7)  | PT_HPUX,
    HP_CORE_SHM       = (0x60000000 + 0x8)  | PT_HPUX,
    HP_CORE_MMF       = (0x60000000 + 0x9)  | PT_HPUX,
    HP_PARALLEL       = (0x60000000 + 0x10) | PT_HPUX,
    HP_FASTBIND       = (0x60000000 + 0x11) | PT_HPUX,
    HP_OPT_ANNOT      = (0x60000000 + 0x12) | PT_HPUX,
    HP_HSL_ANNOT      = (0x60000000 + 0x13) | PT_HPUX,
    HP_STACK          = (0x60000000 + 0x14) | PT_HPUX,
    HP_CORE_UTSNAME   = (0x60000000 + 0x15) | PT_HPUX,
  };

  enum class FLAGS {
    NONE = 0,
    X    = 1,
    W    = 2,
    R    = 4,
  };

  static TYPE type_from(uint64_t value, ARCH arch, Header::OS_ABI os);
  static uint64_t to_value(TYPE type) {
    return static_cast<uint64_t>(type) & PT_MASK;
  }

  static result<Segment> from_raw(const uint8_t* ptr, size_t size);
  static result<Segment> from_raw(const std::vector<uint8_t>& raw) {
    return from_raw(raw.data(), raw.size());
  }

  Segment() = default;

  ~Segment() override = default;

  Segment& operator=(Segment other);
  Segment(const Segment& other);

  Segment& operator=(Segment&&) = default;
  Segment(Segment&&) = default;

  void swap(Segment& other);

  bool is_load() const {
    return type() == TYPE::LOAD;
  }

  bool is_interpreter() const {
    return type() == TYPE::INTERP;
  }

  bool is_phdr() const {
    return type() == TYPE::PHDR;
  }

  /// The segment's type (LOAD, DYNAMIC, ...)
  TYPE type() const {
    return type_;
  }

  /// The flag permissions associated with this segment
  FLAGS flags() const {
    return FLAGS(flags_);
  }

  /// The file offset of the data associated with this segment
  uint64_t file_offset() const {
    return file_offset_;
  }

  /// The virtual address of the segment.
  uint64_t virtual_address() const {
    return virtual_address_;
  }

  /// The physical address of the segment.
  /// This value is not really relevant on systems like Linux or Android.
  /// On the other hand, Qualcomm trustlets might use this value.
  ///
  /// Usually this value matches virtual_address
  uint64_t physical_address() const {
    return physical_address_;
  }

  /// The **file** size of the data associated with this segment
  uint64_t physical_size() const {
    return size_;
  }

  /// The in-memory size of this segment.
  /// Usually, if the `.bss` segment is wrapped by this segment
  /// then, virtual_size is larger than physical_size
  uint64_t virtual_size() const {
    return virtual_size_;
  }

  /// The offset alignment of the segment
  uint64_t alignment() const {
    return alignment_;
  }

  /// The raw data associated with this segment.
  span<const uint8_t> content() const;

  /// Check if the current segment has the given flag
  bool has(FLAGS flag) const {
    return (flags_ & static_cast<uint64_t>(flag)) != 0;
  }

  /// Check if the current segment wraps the given ELF::Section
  bool has(const Section& section) const;

  /// Check if the current segment wraps the given section's name
  bool has(const std::string& section_name) const;

  /// Append the given ELF_SEGMENT_FLAGS
  void add(FLAGS flag);

  /// Remove the given ELF_SEGMENT_FLAGS
  void remove(FLAGS flag);

  void type(TYPE type) {
    type_ = type;
  }

  void flags(FLAGS flags) {
    flags_ = static_cast<uint32_t>(flags);
  }

  void flags(uint32_t flags) {
    flags_ = flags;
  }

  void clear_flags() {
    flags_ = 0;
  }

  void file_offset(uint64_t file_offset);

  void virtual_address(uint64_t virtual_address) {
    virtual_address_ = virtual_address;
  }

  void physical_address(uint64_t physical_address) {
    physical_address_ = physical_address;
  }

  void physical_size(uint64_t physical_size);

  void virtual_size(uint64_t virtual_size) {
    virtual_size_ = virtual_size;
  }

  void alignment(uint64_t alignment) {
    alignment_ = alignment;
  }

  void content(std::vector<uint8_t> content);

  /// Fill the content of this segment with the value provided in parameter
  void fill(char c) {
    span<uint8_t> buffer = writable_content();
    std::fill(buffer.begin(), buffer.end(), c);
  }

  /// Clear the content of this segment
  void clear() { fill('\0'); }

  template<typename T> T get_content_value(size_t offset) const;
  template<typename T> void set_content_value(size_t offset, T value);
  size_t get_content_size() const;

  /// Iterator over the sections wrapped by this segment
  it_sections sections() {
    return sections_;
  }

  it_const_sections sections() const {
    return sections_;
  }

  std::unique_ptr<SpanStream> stream() const;

  void accept(Visitor& visitor) const override;

  Segment& operator+=(FLAGS flag) {
    add(flag);
    return *this;
  }

  Segment& operator-=(FLAGS flag) {
    remove(flag);
    return *this;
  }

  LIEF_API friend std::ostream& operator<<(std::ostream& os, const Segment& segment);

  private:
  template<class T>
  LIEF_LOCAL Segment(const T& header, ARCH arch = ARCH::NONE,
                     Header::OS_ABI os = Header::OS_ABI::SYSTEMV);

  LIEF_LOCAL uint64_t handler_size() const;
  span<uint8_t> writable_content();

  TYPE type_ = TYPE::PT_NULL_;
  ARCH arch_ = ARCH::NONE;
  uint32_t flags_ = 0;
  uint64_t file_offset_ = 0;
  uint64_t virtual_address_ = 0;
  uint64_t physical_address_ = 0;
  uint64_t size_ = 0;
  uint64_t virtual_size_ = 0;
  uint64_t alignment_ = 0;
  uint64_t handler_size_ = 0;
  sections_t sections_;
  DataHandler::Handler* datahandler_ = nullptr;
  std::vector<uint8_t>  content_c_;
};

LIEF_API const char* to_string(Segment::TYPE e);
LIEF_API const char* to_string(Segment::FLAGS e);
}
}


ENABLE_BITMASK_OPERATORS(LIEF::ELF::Segment::FLAGS);

#endif /* _ELF_SEGMENT_H */
