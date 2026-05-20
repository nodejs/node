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
#ifndef LIEF_PE_OPTIONAL_HEADER_H
#define LIEF_PE_OPTIONAL_HEADER_H
#include <ostream>
#include <vector>
#include <cstdint>

#include "LIEF/Object.hpp"
#include "LIEF/visibility.h"

#include "LIEF/enums.hpp"
#include "LIEF/PE/enums.hpp"

namespace LIEF {
namespace PE {
class Parser;
class Binary;

namespace details {
struct pe32_optional_header;
struct pe64_optional_header;
}

/// Class which represents the PE OptionalHeader structure
///
/// Note that the term *optional* comes from the COFF specifications but this
/// header is **mandatory** for a PE binary.
class LIEF_API OptionalHeader : public Object {
  friend class Parser;
  friend class Binary;
  public:

  enum class DLL_CHARACTERISTICS: size_t  {
    HIGH_ENTROPY_VA       = 0x0020, ///< ASLR with 64 bit address space.
    DYNAMIC_BASE          = 0x0040, ///< DLL can be relocated at load time.
    FORCE_INTEGRITY       = 0x0080, ///< Code integrity checks are enforced.
    NX_COMPAT             = 0x0100, ///< Image is NX compatible.
    NO_ISOLATION          = 0x0200, ///< Isolation aware, but do not isolate the image.
    NO_SEH                = 0x0400, ///< Does not use structured exception handling (SEH). No SEH handler may be called in this image.
    NO_BIND               = 0x0800, ///< Do not bind the image.
    APPCONTAINER          = 0x1000, ///< Image should execute in an AppContainer.
    WDM_DRIVER            = 0x2000, ///< A WDM driver.
    GUARD_CF              = 0x4000, ///< Image supports Control Flow Guard.
    TERMINAL_SERVER_AWARE = 0x8000  ///< Terminal Server aware.
  };

  enum class SUBSYSTEM: size_t  {
    UNKNOWN                  = 0,  ///< An unknown subsystem.
    NATIVE                   = 1,  ///< Device drivers and native Windows processes
    WINDOWS_GUI              = 2,  ///< The Windows GUI subsystem.
    WINDOWS_CUI              = 3,  ///< The Windows character subsystem.
    OS2_CUI                  = 5,  ///< The OS/2 character subsytem.
    POSIX_CUI                = 7,  ///< The POSIX character subsystem.
    NATIVE_WINDOWS           = 8,  ///< Native Windows 9x driver.
    WINDOWS_CE_GUI           = 9,  ///< Windows CE.
    EFI_APPLICATION          = 10, ///< An EFI application.
    EFI_BOOT_SERVICE_DRIVER  = 11, ///< An EFI driver with boot services.
    EFI_RUNTIME_DRIVER       = 12, ///< An EFI driver with run-time services.
    EFI_ROM                  = 13, ///< An EFI ROM image.
    XBOX                     = 14, ///< XBOX.
    WINDOWS_BOOT_APPLICATION = 16  ///< A BCD application.
  };

  OptionalHeader(const details::pe32_optional_header& header);
  OptionalHeader(const details::pe64_optional_header& header);
  ~OptionalHeader() override = default;

  OptionalHeader& operator=(const OptionalHeader&) = default;
  OptionalHeader(const OptionalHeader&) = default;

  static OptionalHeader create(PE_TYPE type);

  /// Magic bytes: either ``PE32`` or ``PE32+`` for 64-bits PE files
  PE_TYPE magic() const {
    return magic_;
  }

  /// The linker major version
  uint8_t major_linker_version() const {
    return major_linker_version_;
  }

  /// The linker minor version
  uint8_t minor_linker_version() const {
    return minor_linker_version_;
  }

  /// The size of the code ``.text`` section or the sum of
  /// all the sections that contain code (i.e. PE::Section with the flag Section::CHARACTERISTICS::CNT_CODE)
  uint32_t sizeof_code() const {
    return sizeof_code_;
  }

  /// The size of the initialized data which are usually located in the ``.data`` section.
  /// If the initialized data are split across multiple sections, it is the sum of the sections.
  ///
  /// The sections associated with the initialized data are usually identified with the
  /// flag Section::CHARACTERISTICS::CNT_INITIALIZED_DATA
  uint32_t sizeof_initialized_data() const {
    return sizeof_initialized_data_;
  }

  /// The size of the uninitialized data which are usually located in the ``.bss`` section.
  /// If the uninitialized data are split across multiple sections, it is the sum of the sections.
  ///
  /// The sections associated with the uninitialized data are usually identified with the
  /// flag Section::CHARACTERISTICS::CNT_UNINITIALIZED_DATA
  uint32_t sizeof_uninitialized_data() const {
    return sizeof_uninitialized_data_;
  }

  /// The address of the entry point relative to the image base when the executable file is
  /// loaded into memory. For program images, this is the starting address. For device
  /// drivers, this is the address of the initialization function.
  ///
  /// An entry point is optional for DLLs. When no entry point is present, this field must be zero.
  uint32_t addressof_entrypoint() const {
    return entrypoint_;
  }

  /// Address relative to the imagebase where the binary's code starts.
  uint32_t baseof_code() const {
    return baseof_code_;
  }

  /// Address relative to the imagebase where the binary's data starts.
  ///
  /// @warning This value is not present for PE64 files
  uint32_t baseof_data() const {
    return baseof_data_;
  }

  /// The preferred base address when mapping the binary in memory
  uint64_t imagebase() const {
    return imagebase_;
  }

  /// The alignment (in bytes) of sections when they are loaded into memory.
  ///
  /// It must be greater than or equal to file_alignment and
  /// the default is the page size for the architecture.
  uint32_t section_alignment() const {
    return section_align_;
  }

  /// The section's file alignment. This value must be a power of 2 between 512 and 64K.
  /// The default value is usually 512
  uint32_t file_alignment() const {
    return file_align_;
  }

  /// The **major** version number of the required operating system
  uint16_t major_operating_system_version() const {
    return major_os_version_;
  }

  /// The **minor** version number of the required operating system
  uint16_t minor_operating_system_version() const {
    return minor_os_version_;
  }

  /// The major version number of the image
  uint16_t major_image_version() const {
    return major_image_version_;
  }

  /// The minor version number of the image
  uint16_t minor_image_version() const {
    return minor_image_version_;
  }

  /// The major version number of the subsystem
  uint16_t major_subsystem_version() const {
    return major_subsys_version_;
  }

  /// The minor version number of the subsystem
  uint16_t minor_subsystem_version() const {
    return minor_subsys_version_;
  }

  /// According to the official PE specifications, this value
  /// is reserved and **should** be 0.
  uint32_t win32_version_value() const {
    return win32_version_value_;
  }

  /// The size (in bytes) of the image, including all headers, as the image is loaded in memory.
  ///
  /// It must be a multiple of section_alignment and should match Binary::virtual_size
  uint32_t sizeof_image() const {
    return sizeof_image_;
  }

  /// Size of the DosHeader + PE Header + Section headers rounded up to a multiple of the file_alignment
  uint32_t sizeof_headers() const {
    return sizeof_headers_;
  }

  /// The image file checksum. The algorithm for computing the checksum is incorporated into ``IMAGHELP.DLL``.
  ///
  /// The following are checked for validation at load time all **drivers**, any **DLL loaded at boot**
  /// time, and any **DLL** that is loaded into a **critical** Windows process.
  uint32_t checksum() const {
    return checksum_;
  }

  /// Target subsystem like Driver, XBox, Windows GUI, ...
  SUBSYSTEM subsystem() const {
    return subsystem_;
  }

  /// Some characteristics of the underlying binary like the support of the PIE.
  /// The prefix ``dll`` comes from the official PE specifications but these characteristics
  /// are also used for **executables**
  uint32_t dll_characteristics() const {
    return dll_characteristics_;
  }

  /// Size of the stack to reserve when loading the PE binary
  ///
  /// Only OptionalHeader::sizeof_stack_commit is committed, the rest is made
  /// available one page at a time until the reserve size is reached.
  uint64_t sizeof_stack_reserve() const {
    return sizeof_stack_reserve_;
  }

  /// Size of the stack to commit
  uint64_t sizeof_stack_commit() const {
    return sizeof_stack_commit_;
  }

  /// Size of the heap to reserve when loading the PE binary
  uint64_t sizeof_heap_reserve() const {
    return sizeof_heap_reserve_;
  }

  /// Size of the heap to commit
  uint64_t sizeof_heap_commit() const {
    return sizeof_heap_commit_;
  }

  /// According to the PE specifications, this value is *reserved* and **should** be 0.
  uint32_t loader_flags() const {
    return loader_flags_;
  }

  /// The number of DataDirectory that follow this header.
  uint32_t numberof_rva_and_size() const {
    return nb_rva_size_;
  }

  /// Check if the given DLL_CHARACTERISTICS is included in the dll_characteristics
  bool has(DLL_CHARACTERISTICS c) const {
    return (dll_characteristics() & static_cast<uint32_t>(c)) != 0;
  }

  /// Return the list of the dll_characteristics as an std::set of DLL_CHARACTERISTICS
  std::vector<DLL_CHARACTERISTICS> dll_characteristics_list() const;

  /// Add a DLL_CHARACTERISTICS to the current characteristics
  void add(DLL_CHARACTERISTICS c) {
    dll_characteristics(dll_characteristics() | static_cast<uint32_t>(c));
  }

  /// Remove a DLL_CHARACTERISTICS from the current characteristics
  void remove(DLL_CHARACTERISTICS c) {
    dll_characteristics(dll_characteristics() & (~ static_cast<uint32_t>(c)));
  }

  void magic(PE_TYPE magic) {
    magic_ = magic;
  }

  void major_linker_version(uint8_t value) {
    major_linker_version_ = value;
  }

  void minor_linker_version(uint8_t value) {
    minor_linker_version_ = value;
  }

  void sizeof_code(uint32_t value) {
    sizeof_code_ = value;
  }

  void sizeof_initialized_data(uint32_t value) {
    sizeof_initialized_data_ = value;
  }

  void sizeof_uninitialized_data(uint32_t value) {
    sizeof_uninitialized_data_ = value;
  }

  void addressof_entrypoint(uint32_t value) {
    entrypoint_ = value;
  }

  void baseof_code(uint32_t value) {
    baseof_code_ = value;
  }

  void baseof_data(uint32_t value) {
    baseof_data_ = value;
  }

  void imagebase(uint64_t value) {
    imagebase_ = value;
  }

  void section_alignment(uint32_t value) {
    section_align_ = value;
  }

  void file_alignment(uint32_t value) {
    file_align_ = value;
  }

  void major_operating_system_version(uint16_t value) {
    major_os_version_ = value;
  }

  void minor_operating_system_version(uint16_t value) {
    minor_os_version_ = value;
  }

  void major_image_version(uint16_t value) {
    major_image_version_ = value;
  }

  void minor_image_version(uint16_t value) {
    minor_image_version_ = value;
  }

  void major_subsystem_version(uint16_t value) {
    major_subsys_version_ = value;
  }

  void minor_subsystem_version(uint16_t value) {
    minor_subsys_version_ = value;
  }

  void win32_version_value(uint32_t value) {
    win32_version_value_ = value;
  }

  void sizeof_image(uint32_t value) {
    sizeof_image_ = value;
  }

  void sizeof_headers(uint32_t value) {
    sizeof_headers_ = value;
  }

  void checksum(uint32_t value) {
    checksum_ = value;
  }

  void subsystem(SUBSYSTEM value) {
    subsystem_ = value;
  }

  void dll_characteristics(uint32_t value) {
    dll_characteristics_ = value;
  }

  void sizeof_stack_reserve(uint64_t value) {
    sizeof_stack_reserve_ = value;
  }

  void sizeof_stack_commit(uint64_t value) {
    sizeof_stack_commit_ = value;
  }

  void sizeof_heap_reserve(uint64_t value) {
    sizeof_heap_reserve_ = value;
  }

  void sizeof_heap_commit(uint64_t value) {
    sizeof_heap_commit_ = value;
  }

  void loader_flags(uint32_t value) {
    loader_flags_ = value;
  }

  void numberof_rva_and_size(uint32_t value) {
    nb_rva_size_ = value;
  }

  void accept(Visitor& visitor) const override;

  OptionalHeader& operator+=(DLL_CHARACTERISTICS c) {
    add(c);
    return *this;
  }
  OptionalHeader& operator-=(DLL_CHARACTERISTICS c) {
    remove(c);
    return *this;
  }

  LIEF_API friend std::ostream& operator<<(std::ostream& os, const OptionalHeader& entry);

  private:
  OptionalHeader() = default;

  PE_TYPE   magic_ = PE_TYPE::PE32;
  uint8_t   major_linker_version_ = 0;
  uint8_t   minor_linker_version_ = 0;
  uint32_t  sizeof_code_ = 0;
  uint32_t  sizeof_initialized_data_ = 0;
  uint32_t  sizeof_uninitialized_data_ = 0;
  uint32_t  entrypoint_ = 0;
  uint32_t  baseof_code_ = 0;
  uint32_t  baseof_data_ = 0;
  uint64_t  imagebase_ = 0;
  uint32_t  section_align_ = 0;
  uint32_t  file_align_ = 0;
  uint16_t  major_os_version_ = 0;
  uint16_t  minor_os_version_ = 0;
  uint16_t  major_image_version_ = 0;
  uint16_t  minor_image_version_ = 0;
  uint16_t  major_subsys_version_ = 0;
  uint16_t  minor_subsys_version_ = 0;
  uint32_t  win32_version_value_ = 0;
  uint32_t  sizeof_image_ = 0;
  uint32_t  sizeof_headers_ = 0;
  uint32_t  checksum_ = 0;
  SUBSYSTEM subsystem_ = SUBSYSTEM::UNKNOWN;
  uint32_t  dll_characteristics_ = 0;
  uint64_t  sizeof_stack_reserve_ = 0;
  uint64_t  sizeof_stack_commit_ = 0;
  uint64_t  sizeof_heap_reserve_ = 0;
  uint64_t  sizeof_heap_commit_ = 0;
  uint32_t  loader_flags_ = 0;
  uint32_t  nb_rva_size_ = 0;
};

LIEF_API const char* to_string(OptionalHeader::DLL_CHARACTERISTICS);
LIEF_API const char* to_string(OptionalHeader::SUBSYSTEM);

}
}

ENABLE_BITMASK_OPERATORS(LIEF::PE::OptionalHeader::DLL_CHARACTERISTICS);

#endif
