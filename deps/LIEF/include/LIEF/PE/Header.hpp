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
#ifndef LIEF_PE_HEADER_H
#define LIEF_PE_HEADER_H
#include <array>
#include <vector>
#include <ostream>
#include <cstdint>

#include "LIEF/Object.hpp"
#include "LIEF/visibility.h"
#include "LIEF/enums.hpp"
#include "LIEF/PE/enums.hpp"

namespace LIEF {
namespace PE {

namespace details {
struct pe_header;
}

/// Class that represents the PE header (which follows the DosHeader)
class LIEF_API Header : public Object {
  public:
  using signature_t = std::array<uint8_t, /* PE Magic */ 4>;

  enum class MACHINE_TYPES {
    UNKNOWN     = 0x0,
    ALPHA       = 0x184 , /**< Alpha AXP, 32-bit address space */
    ALPHA64     = 0x284 , /**< Alpha AXP, 64-bit address space */
    AM33        = 0x1D3,  /**< Matsushita AM33                */
    AMD64       = 0x8664, /**< AMD x64                        */
    ARM         = 0x1C0,  /**< ARM little endian              */
    ARMNT       = 0x1C4,  /**< ARMv7 Thumb mode only          */
    ARM64       = 0xAA64, /**< ARMv8 in 64-bits mode          */
    EBC         = 0xEBC,  /**< EFI byte code                  */
    I386        = 0x14C,  /**< Intel 386 or later             */
    IA64        = 0x200,  /**< Intel Itanium processor family */
    LOONGARCH32 = 0x6232, /**< LoongArch 32-bit processor family  */
    LOONGARCH64 = 0x6264, /**< LoongArch 64-bit processor family  */
    M32R        = 0x9041, /**< Mitsubishi M32R little endian  */
    MIPS16      = 0x266,  /**< MIPS16                         */
    MIPSFPU     = 0x366,  /**< MIPS with FPU                  */
    MIPSFPU16   = 0x466,  /**< MIPS16 with FPU                */
    POWERPC     = 0x1F0,  /**< Power PC little endian         */
    POWERPCFP   = 0x1F1,  /**< Power PC with floating point   */
    POWERPCBE   = 0x1F2,  /**< Power PC big endian            */
    R4000       = 0x166,  /**< MIPS with little endian        */
    RISCV32     = 0x5032, /**< RISC-V 32-bit address space    */
    RISCV64     = 0x5064, /**< RISC-V 64-bit address space    */
    RISCV128    = 0x5128, /**< RISC-V 128-bit address space   */
    SH3         = 0x1A2,  /**< Hitachi SH3                    */
    SH3DSP      = 0x1A3,  /**< Hitachi SH3 DSP                */
    SH4         = 0x1A6,  /**< Hitachi SH4                    */
    SH5         = 0x1A8,  /**< Hitachi SH5                    */
    THUMB       = 0x1C2,  /**< ARM or Thumb                   */
    WCEMIPSV2   = 0x169,   /**< MIPS little-endian WCE v2      */
    ARM64EC     = 0xa641,
    ARM64X      = 0xa64e,
    CHPE_X86    = 0x3a64,
  };

  static bool is_known_machine(uint16_t machine);

  static bool is_arm(MACHINE_TYPES ty) {
    switch (ty) {
      default:
        return false;
      case MACHINE_TYPES::ARM:
      case MACHINE_TYPES::THUMB:
      case MACHINE_TYPES::ARMNT:
        return true;
    }
    return false;
  }

  static bool is_riscv(MACHINE_TYPES ty) {
    switch (ty) {
      default:
        return false;
      case MACHINE_TYPES::RISCV32:
      case MACHINE_TYPES::RISCV64:
      case MACHINE_TYPES::RISCV128:
        return true;
    }
    return false;
  }

  static bool is_loonarch(MACHINE_TYPES ty) {
    switch (ty) {
      default:
        return false;
      case MACHINE_TYPES::LOONGARCH32:
      case MACHINE_TYPES::LOONGARCH64:
        return true;
    }
    return false;
  }


  static bool is_arm64(MACHINE_TYPES ty) {
    return ty == MACHINE_TYPES::ARM64;
  }

  static bool is_thumb(MACHINE_TYPES ty) {
    return ty == MACHINE_TYPES::THUMB;
  }

  static bool x86(MACHINE_TYPES ty) {
    return ty == MACHINE_TYPES::I386;
  }

  static bool x86_64(MACHINE_TYPES ty) {
    return ty == MACHINE_TYPES::AMD64;
  }

  static bool is_mips(MACHINE_TYPES ty) {
    switch (ty) {
      default:
        return false;
      case MACHINE_TYPES::MIPS16:
      case MACHINE_TYPES::MIPSFPU:
      case MACHINE_TYPES::MIPSFPU16:
      case MACHINE_TYPES::R4000:
      case MACHINE_TYPES::WCEMIPSV2:
        return true;
    }
    return false;
  }

  static bool is_ppc(MACHINE_TYPES ty) {
    switch (ty) {
      default:
        return false;
      case MACHINE_TYPES::POWERPC:
      case MACHINE_TYPES::POWERPCFP:
      case MACHINE_TYPES::POWERPCBE:
        return true;
    }
    return false;
  }

  enum class CHARACTERISTICS {
    NONE                    = 0x0000,
    RELOCS_STRIPPED         = 0x0001, /**< The file does not contain base relocations and must be loaded at its preferred base. If this cannot be done, the loader will error.*/
    EXECUTABLE_IMAGE        = 0x0002, /**< File is executable (i.e. no unresolved externel references). */
    LINE_NUMS_STRIPPED      = 0x0004, /**< COFF line numbers have been stripped. This is deprecated and should be 0 */
    LOCAL_SYMS_STRIPPED     = 0x0008, /**< COFF symbol table entries for local symbols have been removed. This is deprecated and should be 0.*/
    AGGRESSIVE_WS_TRIM      = 0x0010, /**< Aggressively trim working set. This is deprecated and must be 0. */
    LARGE_ADDRESS_AWARE     = 0x0020, /**< Image can handle > 2GiB addresses. */
    BYTES_REVERSED_LO       = 0x0080, /**< Little endian: the LSB precedes the MSB in memory. This is deprecated and should be 0.*/
    NEED_32BIT_MACHINE      = 0x0100, /**< Machine is based on a 32bit word architecture. */
    DEBUG_STRIPPED          = 0x0200, /**< Debugging info has been removed. */
    REMOVABLE_RUN_FROM_SWAP = 0x0400, /**< If the image is on removable media, fully load it and copy it to swap. */
    NET_RUN_FROM_SWAP       = 0x0800, /**< If the image is on network media, fully load it and copy it to swap. */
    SYSTEM                  = 0x1000, /**< The image file is a system file, not a user program.*/
    DLL                     = 0x2000, /**< The image file is a DLL. */
    UP_SYSTEM_ONLY          = 0x4000, /**< This file should only be run on a uniprocessor machine. */
    BYTES_REVERSED_HI       = 0x8000  /**< Big endian: the MSB precedes the LSB in memory. This is deprecated */
  };
  static Header create(PE_TYPE type);

  Header(const details::pe_header& header);
  ~Header() override = default;

  Header& operator=(const Header&) = default;
  Header(const Header&) = default;

  /// Signature (or magic byte) of the header. It must be: ``PE\0\0``
  const signature_t& signature() const {
    return signature_;
  }

  /// The targeted machine architecture like ARM, x86, AMD64, ...
  MACHINE_TYPES machine() const {
    return machine_;
  }

  /// The number of sections in the binary.
  uint16_t numberof_sections() const {
    return nb_sections_;
  }

  /// The low 32 bits of the number of seconds since
  /// January 1, 1970. It **indicates** when the file was created.
  uint32_t time_date_stamp() const {
    return timedatestamp_;
  }

  /// The offset of the **COFF** symbol table.
  ///
  /// This value should be zero for an image because COFF debugging information is deprecated.
  uint32_t pointerto_symbol_table() const {
    return pointerto_symtab_;
  }

  /// The number of entries in the symbol table. This data can be used to locate the string table
  /// which immediately follows the symbol table.
  ///
  /// This value should be zero for an image because COFF debugging information is deprecated.
  uint32_t numberof_symbols() const {
    return nb_symbols_;
  }

  /// Size of the OptionalHeader **AND** the data directories which follows this header.
  ///
  /// This value is equivalent to: ``sizeof(pe_optional_header) + NB_DATA_DIR * sizeof(data_directory)``
  ///
  /// This size **should** be either:
  /// * 0xE0 (224) for a PE32  (32 bits)
  /// * 0xF0 (240) for a PE32+ (64 bits)
  uint16_t sizeof_optional_header() const {
    return sizeof_opt_header_;
  }

  /// Characteristics of the binary like whether it is a DLL or an executable
  uint32_t characteristics() const {
    return characteristics_;
  }

  /// Check if the given CHARACTERISTICS is present
  bool has_characteristic(CHARACTERISTICS c) const {
    return (characteristics() & static_cast<uint32_t>(c)) > 0;
  }

  /// The list of the CHARACTERISTICS
  std::vector<CHARACTERISTICS> characteristics_list() const;

  void machine(MACHINE_TYPES type) {
    machine_ = type;
  }

  void numberof_sections(uint16_t nb) {
    nb_sections_ = nb;
  }

  void time_date_stamp(uint32_t timestamp) {
    timedatestamp_ = timestamp;
  }

  void pointerto_symbol_table(uint32_t ptr) {
    pointerto_symtab_ = ptr;
  }

  void numberof_symbols(uint32_t nb) {
    nb_symbols_ = nb;
  }

  void sizeof_optional_header(uint16_t size) {
    sizeof_opt_header_ = size;
  }

  void characteristics(uint32_t characteristics) {
    characteristics_ = characteristics;
  }

  void signature(const signature_t& sig) {
    signature_ = sig;
  }

  void add_characteristic(CHARACTERISTICS c) {
    characteristics_ |= static_cast<uint32_t>(c);
  }

  void remove_characteristic(CHARACTERISTICS c) {
    characteristics_ &= ~static_cast<uint32_t>(c);
  }

  void accept(Visitor& visitor) const override;

  LIEF_API friend std::ostream& operator<<(std::ostream& os, const Header& entry);

  /// \private
  LIEF_LOCAL Header() = default;

  private:
  signature_t signature_;
  MACHINE_TYPES machine_ = MACHINE_TYPES::UNKNOWN;
  uint16_t nb_sections_ = 0;
  uint32_t timedatestamp_ = 0;
  uint32_t pointerto_symtab_ = 0;
  uint32_t nb_symbols_ = 0;
  uint16_t sizeof_opt_header_ = 0;
  uint32_t characteristics_ = 0;
};

LIEF_API const char* to_string(Header::CHARACTERISTICS c);
LIEF_API const char* to_string(Header::MACHINE_TYPES c);
}
}

ENABLE_BITMASK_OPERATORS(LIEF::PE::Header::CHARACTERISTICS);
#endif
