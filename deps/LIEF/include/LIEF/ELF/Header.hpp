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
#ifndef LIEF_ELF_HEADER_H
#define LIEF_ELF_HEADER_H

#include <ostream>
#include <array>
#include <vector>

#include "LIEF/Object.hpp"
#include "LIEF/visibility.h"

#include "LIEF/ELF/enums.hpp"
#include "LIEF/ELF/ProcessorFlags.hpp"

namespace LIEF {
namespace ELF {
class Parser;

/// Class which represents the ELF's header. This class mirrors the raw
/// ELF `Elfxx_Ehdr` structure
class LIEF_API Header : public Object {
  friend class Parser;
  public:
  using identity_t = std::array<uint8_t, 16>;

  public:
  /// e_ident size and indices.
  enum ELF_INDENT {
    ELI_MAG0       = 0,  ///< File identification index.
    ELI_MAG1       = 1,  ///< File identification index.
    ELI_MAG2       = 2,  ///< File identification index.
    ELI_MAG3       = 3,  ///< File identification index.
    ELI_CLASS      = 4,  ///< File class.
    ELI_DATA       = 5,  ///< Data encoding.
    ELI_VERSION    = 6,  ///< File version.
    ELI_OSABI      = 7,  ///< OS/ABI identification.
    ELI_ABIVERSION = 8,  ///< ABI version.
    ELI_PAD        = 9,  ///< Start of padding bytes.
    ELI_NIDENT     = 16  ///< Number of bytes in e_ident.
  };

  /// The type of the underlying ELF file. This enum matches
  /// the semantic of `ET_NONE`, `ET_REL`, ...
  enum class FILE_TYPE {
    NONE = 0, ///< Can't be determined
    REL  = 1, ///< Relocatable file (or object file)
    EXEC = 2, ///< non-pie executable
    DYN  = 3, ///< Shared library **or** a pie-executable
    CORE = 4, ///< Core dump file
  };

  /// Match the result of `Elfxx_Ehdr.e_version`
  enum class VERSION {
    NONE    = 0, ///< Invalid ELF version
    CURRENT = 1, ///< Current version (default)
  };

  /// Match the result of `Elfxx_Ehdr.e_ident[EI_CLASS]`
  enum class CLASS {
    NONE = 0, /// Invalid class
    ELF32,    /// 32-bit objects
    ELF64,    /// 64-bits objects
  };

  /// Match the result `Elfxx_Ehdr.e_ident[EI_OSABI]`
  enum class OS_ABI {
    SYSTEMV      = 0,  /// UNIX System V ABI
    HPUX         = 1,  /// HP-UX operating system
    NETBSD       = 2,  /// NetBSD
    GNU          = 3,  /// GNU/Linux
    LINUX        = 3,  /// Historical alias for ELFOSABI_GNU.
    HURD         = 4,  /// GNU/Hurd
    SOLARIS      = 6,  /// Solaris
    AIX          = 7,  /// AIX
    IRIX         = 8,  /// IRIX
    FREEBSD      = 9,  /// FreeBSD
    TRU64        = 10, /// TRU64 UNIX
    MODESTO      = 11, /// Novell Modesto
    OPENBSD      = 12, /// OpenBSD
    OPENVMS      = 13, /// OpenVMS
    NSK          = 14, /// Hewlett-Packard Non-Stop Kernel
    AROS         = 15, /// AROS
    FENIXOS      = 16, /// FenixOS
    CLOUDABI     = 17, /// Nuxi CloudABI
    C6000_ELFABI = 64, /// Bare-metal TMS320C6000
    AMDGPU_HSA   = 64, /// AMD HSA runtime
    C6000_LINUX  = 65, /// Linux TMS320C6000
    ARM          = 97, /// ARM
    STANDALONE   = 255 /// Standalone (embedded) application
  };

  /// Match the result `Elfxx_Ehdr.e_ident[EI_DATA]`
  enum class ELF_DATA {
    NONE = 0, /// Invalid data encoding
    LSB  = 1, /// 2's complement, little endian
    MSB  = 2  /// 2's complement, big endian
  };

  Header() = default;

  Header& operator=(const Header&) = default;
  Header(const Header&) = default;

  ~Header() override = default;

  /// Define the object file type. (e.g. executable, library...)
  FILE_TYPE file_type() const {
    return file_type_;
  }

  /// Target architecture
  ARCH machine_type() const {
    return machine_type_;
  }

  /// Version of the object file format
  VERSION object_file_version() const {
    return object_file_version_;
  }

  /// Executable entrypoint
  uint64_t entrypoint() const {
    return entrypoint_;
  }

  /// Offset of the programs table (also known as segments table)
  uint64_t program_headers_offset() const {
    return program_headers_offset_;
  }

  /// Offset of the sections table
  uint64_t section_headers_offset() const {
    return section_headers_offset_;
  }

  /// Processor-specific flags
  uint32_t processor_flag() const {
    return processor_flags_;
  }

  /// Size of the current header (i.e. `sizeof(Elfxx_Ehdr)`)
  /// This size should be 64 for an `ELF64` binary and 52 for an `ELF32`.
  uint32_t header_size() const {
    return header_size_;
  }

  /// Return the size of a program header (i.e. `sizeof(Elfxx_Phdr)`)
  /// This size should be 56 for an `ELF64` binary and 32 for an `ELF32`.
  uint32_t program_header_size() const {
    return program_header_size_;
  }

  /// Return the number of segments
  uint32_t numberof_segments() const {
    return numberof_segments_;
  }

  /// Return the size of a section header (i.e. `sizeof(Elfxx_Shdr)`)
  /// This size should be 64 for a ``ELF64`` binary and 40 for an ``ELF32``.
  uint32_t section_header_size() const {
    return section_header_size_;
  }

  /// Return the number of sections
  ///
  /// @warning This value could differ from the real number of sections
  /// present in the binary. It must be taken as an *indication*
  uint32_t numberof_sections() const {
    return numberof_sections_;
  }

  /// Return the section's index which contains sections' names
  uint32_t section_name_table_idx() const {
    return section_string_table_idx_;
  }

  /// Return the ELF identity as an `std::array`
  identity_t& identity() {
    return identity_;
  }

  const identity_t& identity() const {
    return identity_;
  }

  /// Return the object's class. `ELF64` or `ELF32`
  CLASS identity_class() const {
    return CLASS(identity_[ELI_CLASS]);
  }

  /// Specify the data encoding
  ELF_DATA identity_data() const {
    return ELF_DATA(identity_[ELI_DATA]);
  }

  /// @see object_file_version
  VERSION identity_version() const {
    return VERSION(identity_[ELI_VERSION]);
  }

  /// Identifies the version of the ABI for which the object is prepared
  OS_ABI identity_os_abi() const {
    return OS_ABI(identity_[ELI_OSABI]);
  }

  /// ABI Version
  uint32_t identity_abi_version() const {
    return identity_[ELI_ABIVERSION];
  }

  bool has(PROCESSOR_FLAGS flag) const;

  std::vector<PROCESSOR_FLAGS> flags_list() const;

  void file_type(FILE_TYPE type) {
    file_type_ = type;
  }

  void machine_type(ARCH arch) {
    machine_type_ = arch;
  }

  void object_file_version(VERSION version) {
    object_file_version_ = version;
  }

  void entrypoint(uint64_t entry) {
    entrypoint_ = entry;
  }

  void program_headers_offset(uint64_t offset) {
    program_headers_offset_ = offset;
  }

  void section_headers_offset(uint64_t offset) {
    section_headers_offset_ = offset;
  }

  void processor_flag(uint32_t flags) {
    processor_flags_ = flags;
  }

  void header_size(uint32_t size) {
    header_size_ = size;
  }

  void program_header_size(uint32_t size) {
    program_header_size_ = size;
  }

  void numberof_segments(uint32_t n) {
    numberof_segments_ = n;
  }
  void section_header_size(uint32_t size) {
    section_header_size_ = size;
  }

  void numberof_sections(uint32_t n) {
    numberof_sections_ = n;
  }
  void section_name_table_idx(uint32_t idx) {
    section_string_table_idx_ = idx;
  }

  void identity(const std::string& identity);
  void identity(const identity_t& identity);

  void identity_class(CLASS cls) {
    identity_[ELI_CLASS] = static_cast<uint8_t>(cls);
  }

  void identity_data(ELF_DATA data) {
    identity_[ELI_DATA] = static_cast<uint8_t>(data);
  }

  void identity_version(VERSION version) {
    identity_[ELI_VERSION] = static_cast<uint8_t>(version);
  }

  void identity_os_abi(OS_ABI osabi) {
    identity_[ELI_OSABI] = static_cast<uint8_t>(osabi);
  }

  void identity_abi_version(uint8_t version) {
    identity_[ELI_ABIVERSION] = version;
  }

  void accept(Visitor& visitor) const override;

  LIEF_API friend std::ostream& operator<<(std::ostream& os, const Header& hdr);

  private:
  template<class T>
  LIEF_LOCAL Header(const T& header);

  identity_t identity_;
  FILE_TYPE file_type_ = FILE_TYPE::NONE;
  ARCH machine_type_ = ARCH::NONE;
  VERSION object_file_version_ = VERSION::NONE;
  uint64_t entrypoint_ = 0;
  uint64_t program_headers_offset_ = 0;
  uint64_t section_headers_offset_ = 0;
  uint32_t processor_flags_ = 0;
  uint32_t header_size_ = 0;
  uint32_t program_header_size_ = 0;
  uint32_t numberof_segments_ = 0;
  uint32_t section_header_size_ = 0;
  uint32_t numberof_sections_ = 0;
  uint32_t section_string_table_idx_ = 0;
};

LIEF_API const char* to_string(Header::FILE_TYPE type);
LIEF_API const char* to_string(Header::VERSION version);
LIEF_API const char* to_string(Header::CLASS version);
LIEF_API const char* to_string(Header::OS_ABI abi);
LIEF_API const char* to_string(Header::ELF_DATA abi);

}
}
#endif
