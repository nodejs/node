/* Copyright 2021 - 2025 R. Thomas
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
#ifndef LIEF_MACHO_ENUMS_H
#define LIEF_MACHO_ENUMS_H

#include <cstdint>

namespace LIEF {
namespace MachO {

enum class MACHO_TYPES: uint32_t {
  UNKNOWN    = 0,
  MAGIC      = 0xFEEDFACEu, ///< 32-bit big-endian magic
  CIGAM      = 0xCEFAEDFEu, ///< 32-bit little-endian magic
  MAGIC_64   = 0xFEEDFACFu, ///< 64-bit big-endian magic
  CIGAM_64   = 0xCFFAEDFEu, ///< 64-bit little-endian magic
  MAGIC_FAT  = 0xCAFEBABEu, ///< big-endian fat magic
  CIGAM_FAT  = 0xBEBAFECAu,  ///< little-endian fat magic

  NEURAL_MODEL = 0xbeeffaceu,
};

enum class X86_RELOCATION  {
  GENERIC_RELOC_VANILLA        = 0, /**< A generic relocation entry for both addresses contained in data and addresses contained in CPU instructions. */
  GENERIC_RELOC_PAIR           = 1, /**< The second relocation entry of a pair. */
  GENERIC_RELOC_SECTDIFF       = 2, /**< A relocation entry for an item that contains the difference of two section addresses. This is generally used for position-independent code generation. */
  GENERIC_RELOC_PB_LA_PTR      = 3, /**< contains the address from which to subtract; it must be followed by a X86_RELOCATION::GENERIC_RELOC_PAIR containing the address to subtract.*/
  GENERIC_RELOC_LOCAL_SECTDIFF = 4, /**< Similar to X86_RELOCATION::GENERIC_RELOC_SECTDIFF except that this entry refers specifically to the address in this item. If the address is that of a globally visible coalesced symbol, this relocation entry does not change if the symbol is overridden. This is used to associate stack unwinding information with the object code this relocation entry describes.*/
  GENERIC_RELOC_TLV            = 5, /**< A relocation entry for a prebound lazy pointer. This is always a scattered relocation entry. The MachO::Relocation::value field contains the non-prebound value of the lazy pointer.*/
};

enum class X86_64_RELOCATION  {
  X86_64_RELOC_UNSIGNED        = 0, /**< A CALL/JMP instruction with 32-bit displacement. */
  X86_64_RELOC_SIGNED          = 1, /**< A MOVQ load of a GOT entry. */
  X86_64_RELOC_BRANCH          = 2, /**< Other GOT references. */
  X86_64_RELOC_GOT_LOAD        = 3, /**< Signed 32-bit displacement. */
  X86_64_RELOC_GOT             = 4, /**< Absolute address. */
  X86_64_RELOC_SUBTRACTOR      = 5, /**< Must be followed by a X86_64_RELOCATION::X86_64_RELOC_UNSIGNED relocation. */
  X86_64_RELOC_SIGNED_1        = 6, /**< */
  X86_64_RELOC_SIGNED_2        = 7, /**< */
  X86_64_RELOC_SIGNED_4        = 8, /**< */
  X86_64_RELOC_TLV             = 9, /**< */
};


enum class PPC_RELOCATION  {
  PPC_RELOC_VANILLA            = 0,
  PPC_RELOC_PAIR               = 1,
  PPC_RELOC_BR14               = 2,
  PPC_RELOC_BR24               = 3,
  PPC_RELOC_HI16               = 4,
  PPC_RELOC_LO16               = 5,
  PPC_RELOC_HA16               = 6,
  PPC_RELOC_LO14               = 7,
  PPC_RELOC_SECTDIFF           = 8,
  PPC_RELOC_PB_LA_PTR          = 9,
  PPC_RELOC_HI16_SECTDIFF      = 10,
  PPC_RELOC_LO16_SECTDIFF      = 11,
  PPC_RELOC_HA16_SECTDIFF      = 12,
  PPC_RELOC_JBSR               = 13,
  PPC_RELOC_LO14_SECTDIFF      = 14,
  PPC_RELOC_LOCAL_SECTDIFF     = 15,
};


enum class ARM_RELOCATION  {
  ARM_RELOC_VANILLA            = 0,
  ARM_RELOC_PAIR               = 1,
  ARM_RELOC_SECTDIFF           = 2,
  ARM_RELOC_LOCAL_SECTDIFF     = 3,
  ARM_RELOC_PB_LA_PTR          = 4,
  ARM_RELOC_BR24               = 5,
  ARM_THUMB_RELOC_BR22         = 6,
  ARM_THUMB_32BIT_BRANCH       = 7, // obsolete
  ARM_RELOC_HALF               = 8,
  ARM_RELOC_HALF_SECTDIFF      = 9,
};


enum class ARM64_RELOCATION  {
  ARM64_RELOC_UNSIGNED            = 0,  /**< For pointers. */
  ARM64_RELOC_SUBTRACTOR          = 1,  /**< Must be followed by an ARM64_RELOCATION::ARM64_RELOC_UNSIGNED */
  ARM64_RELOC_BRANCH26            = 2,  /**< A B/BL instruction with 26-bit displacement. */
  ARM64_RELOC_PAGE21              = 3,  /**< PC-rel distance to page of target. */
  ARM64_RELOC_PAGEOFF12           = 4,  /**< Offset within page, scaled by MachO::Relocation::size. */
  ARM64_RELOC_GOT_LOAD_PAGE21     = 5,  /**< PC-rel distance to page of GOT slot */
  ARM64_RELOC_GOT_LOAD_PAGEOFF12  = 6,  /**< Offset within page of GOT slot, scaled by MachO::Relocation::size. */
  ARM64_RELOC_POINTER_TO_GOT      = 7,  /**< For pointers to GOT slots. */
  ARM64_RELOC_TLVP_LOAD_PAGE21    = 8,  /**< PC-rel distance to page of TLVP slot. */
  ARM64_RELOC_TLVP_LOAD_PAGEOFF12 = 9,  /**< Offset within page of TLVP slot, scaled by MachO::Relocation::size.*/
  ARM64_RELOC_ADDEND              = 10, /**< Must be followed by ARM64_RELOCATION::ARM64_RELOC_PAGE21 or ARM64_RELOCATION::ARM64_RELOC_PAGEOFF12. */
};

}
}
#endif
