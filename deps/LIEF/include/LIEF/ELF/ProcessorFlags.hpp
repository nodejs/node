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
#ifndef LIEF_ELF_PROCESSOR_FLAGS_H
#define LIEF_ELF_PROCESSOR_FLAGS_H
#include <cstdint>
#include "LIEF/visibility.h"

namespace LIEF {
namespace ELF {

static constexpr uint64_t PFLAGS_BIT = 43;
static constexpr uint64_t PFLAGS_MASK = (1LLU << PFLAGS_BIT) - 1;
static constexpr uint64_t PF_ARM_ID = 1;
static constexpr uint64_t PF_HEX_ID = 2;
static constexpr uint64_t PF_LOONGARCH_ID = 3;
static constexpr uint64_t PF_MIPS_ID = 4;
static constexpr uint64_t PF_RISCV_ID = 5;

enum class PROCESSOR_FLAGS : uint64_t {
  ARM_EABI_UNKNOWN = 0x00000000 | (PF_ARM_ID << PFLAGS_BIT),
  ARM_SOFT_FLOAT   = 0x00000200 | (PF_ARM_ID << PFLAGS_BIT),
  ARM_VFP_FLOAT    = 0x00000400 | (PF_ARM_ID << PFLAGS_BIT),
  ARM_EABI_VER1    = 0x01000000 | (PF_ARM_ID << PFLAGS_BIT),
  ARM_EABI_VER2    = 0x02000000 | (PF_ARM_ID << PFLAGS_BIT),
  ARM_EABI_VER3    = 0x03000000 | (PF_ARM_ID << PFLAGS_BIT),
  ARM_EABI_VER4    = 0x04000000 | (PF_ARM_ID << PFLAGS_BIT),
  ARM_EABI_VER5    = 0x05000000 | (PF_ARM_ID << PFLAGS_BIT),

  HEXAGON_MACH_V2   = 0x00000001 | (PF_HEX_ID << PFLAGS_BIT),  // Hexagon V2
  HEXAGON_MACH_V3   = 0x00000002 | (PF_HEX_ID << PFLAGS_BIT),  // Hexagon V3
  HEXAGON_MACH_V4   = 0x00000003 | (PF_HEX_ID << PFLAGS_BIT),  // Hexagon V4
  HEXAGON_MACH_V5   = 0x00000004 | (PF_HEX_ID << PFLAGS_BIT),  // Hexagon V5

  HEXAGON_ISA_V2    = 0x00000010 | (PF_HEX_ID << PFLAGS_BIT),  // Hexagon V2 ISA
  HEXAGON_ISA_V3    = 0x00000020 | (PF_HEX_ID << PFLAGS_BIT),  // Hexagon V3 ISA
  HEXAGON_ISA_V4    = 0x00000030 | (PF_HEX_ID << PFLAGS_BIT),  // Hexagon V4 ISA
  HEXAGON_ISA_V5    = 0x00000040 | (PF_HEX_ID << PFLAGS_BIT),  // Hexagon V5 ISA

  LOONGARCH_ABI_SOFT_FLOAT    = 0x1 | (PF_LOONGARCH_ID << PFLAGS_BIT),
  LOONGARCH_ABI_SINGLE_FLOAT  = 0x2 | (PF_LOONGARCH_ID << PFLAGS_BIT),
  LOONGARCH_ABI_DOUBLE_FLOAT  = 0x3 | (PF_LOONGARCH_ID << PFLAGS_BIT),

  MIPS_NOREORDER     = 0x00000001 | (PF_MIPS_ID << PFLAGS_BIT), /* Don't reorder instructions */
  MIPS_PIC           = 0x00000002 | (PF_MIPS_ID << PFLAGS_BIT), /* Position independent code */
  MIPS_CPIC          = 0x00000004 | (PF_MIPS_ID << PFLAGS_BIT), /* Call object with Position independent code */
  MIPS_ABI2          = 0x00000020 | (PF_MIPS_ID << PFLAGS_BIT), /* File uses N32 ABI */
  MIPS_32BITMODE     = 0x00000100 | (PF_MIPS_ID << PFLAGS_BIT), /* Code compiled for a 64-bit machine */
  /* in 32-bit mode */
  MIPS_FP64          = 0x00000200 | (PF_MIPS_ID << PFLAGS_BIT), /* Code compiled for a 32-bit machine */
  /* but uses 64-bit FP registers */
  MIPS_NAN2008       = 0x00000400 | (PF_MIPS_ID << PFLAGS_BIT), /* Uses IEE 754-2008 NaN encoding */

  /* ABI flags */
  MIPS_ABI_O32       = 0x00001000 | (PF_MIPS_ID << PFLAGS_BIT), /* This file follows the first MIPS 32 bit ABI */
  MIPS_ABI_O64       = 0x00002000 | (PF_MIPS_ID << PFLAGS_BIT), /* O32 ABI extended for 64-bit architecture. */
  MIPS_ABI_EABI32    = 0x00003000 | (PF_MIPS_ID << PFLAGS_BIT), /* EABI in 32 bit mode. */
  MIPS_ABI_EABI64    = 0x00004000 | (PF_MIPS_ID << PFLAGS_BIT), /* EABI in 64 bit mode. */

  /* MIPS machine variant */
  MIPS_MACH_3900     = 0x00810000 | (PF_MIPS_ID << PFLAGS_BIT), /* Toshiba R3900 */
  MIPS_MACH_4010     = 0x00820000 | (PF_MIPS_ID << PFLAGS_BIT), /* LSI R4010 */
  MIPS_MACH_4100     = 0x00830000 | (PF_MIPS_ID << PFLAGS_BIT), /* NEC VR4100 */
  MIPS_MACH_4650     = 0x00850000 | (PF_MIPS_ID << PFLAGS_BIT), /* MIPS R4650 */
  MIPS_MACH_4120     = 0x00870000 | (PF_MIPS_ID << PFLAGS_BIT), /* NEC VR4120 */
  MIPS_MACH_4111     = 0x00880000 | (PF_MIPS_ID << PFLAGS_BIT), /* NEC VR4111/VR4181 */
  MIPS_MACH_SB1      = 0x008a0000 | (PF_MIPS_ID << PFLAGS_BIT), /* Broadcom SB-1 */
  MIPS_MACH_OCTEON   = 0x008b0000 | (PF_MIPS_ID << PFLAGS_BIT), /* Cavium Networks Octeon */
  MIPS_MACH_XLR      = 0x008c0000 | (PF_MIPS_ID << PFLAGS_BIT), /* RMI Xlr */
  MIPS_MACH_OCTEON2  = 0x008d0000 | (PF_MIPS_ID << PFLAGS_BIT), /* Cavium Networks Octeon2 */
  MIPS_MACH_OCTEON3  = 0x008e0000 | (PF_MIPS_ID << PFLAGS_BIT), /* Cavium Networks Octeon3 */
  MIPS_MACH_5400     = 0x00910000 | (PF_MIPS_ID << PFLAGS_BIT), /* NEC VR5400 */
  MIPS_MACH_5900     = 0x00920000 | (PF_MIPS_ID << PFLAGS_BIT), /* MIPS R5900 */
  MIPS_MACH_5500     = 0x00980000 | (PF_MIPS_ID << PFLAGS_BIT), /* NEC VR5500 */
  MIPS_MACH_9000     = 0x00990000 | (PF_MIPS_ID << PFLAGS_BIT), /* Unknown */
  MIPS_MACH_LS2E     = 0x00a00000 | (PF_MIPS_ID << PFLAGS_BIT), /* ST Microelectronics Loongson 2E */
  MIPS_MACH_LS2F     = 0x00a10000 | (PF_MIPS_ID << PFLAGS_BIT), /* ST Microelectronics Loongson 2F */
  MIPS_MACH_LS3A     = 0x00a20000 | (PF_MIPS_ID << PFLAGS_BIT), /* Loongson 3A */

  /* ARCH_ASE */
  MIPS_MICROMIPS     = 0x02000000 | (PF_MIPS_ID << PFLAGS_BIT), /* microMIPS */
  MIPS_ARCH_ASE_M16  = 0x04000000 | (PF_MIPS_ID << PFLAGS_BIT), /* Has Mips-16 ISA extensions */
  MIPS_ARCH_ASE_MDMX = 0x08000000 | (PF_MIPS_ID << PFLAGS_BIT), /* Has MDMX multimedia extensions */

  /* ARCH */
  MIPS_ARCH_1        = 0x00000000 | (PF_MIPS_ID << PFLAGS_BIT), /* MIPS1 instruction set */
  MIPS_ARCH_2        = 0x10000000 | (PF_MIPS_ID << PFLAGS_BIT), /* MIPS2 instruction set */
  MIPS_ARCH_3        = 0x20000000 | (PF_MIPS_ID << PFLAGS_BIT), /* MIPS3 instruction set */
  MIPS_ARCH_4        = 0x30000000 | (PF_MIPS_ID << PFLAGS_BIT), /* MIPS4 instruction set */
  MIPS_ARCH_5        = 0x40000000 | (PF_MIPS_ID << PFLAGS_BIT), /* MIPS5 instruction set */
  MIPS_ARCH_32       = 0x50000000 | (PF_MIPS_ID << PFLAGS_BIT), /* MIPS32 instruction set per linux not elf.h */
  MIPS_ARCH_64       = 0x60000000 | (PF_MIPS_ID << PFLAGS_BIT), /* MIPS64 instruction set per linux not elf.h */
  MIPS_ARCH_32R2     = 0x70000000 | (PF_MIPS_ID << PFLAGS_BIT), /* mips32r2, mips32r3, mips32r5 */
  MIPS_ARCH_64R2     = 0x80000000 | (PF_MIPS_ID << PFLAGS_BIT), /* mips64r2, mips64r3, mips64r5 */
  MIPS_ARCH_32R6     = 0x90000000 | (PF_MIPS_ID << PFLAGS_BIT), /* mips32r6 */
  MIPS_ARCH_64R6     = 0xa0000000 | (PF_MIPS_ID << PFLAGS_BIT), /* mips64r6 */

  RISCV_RVC              = 0x00000001 | (PF_RISCV_ID << PFLAGS_BIT),

  RISCV_FLOAT_ABI_SOFT   = 0x00000000 | (PF_RISCV_ID << PFLAGS_BIT),
  RISCV_FLOAT_ABI_SINGLE = 0x00000002 | (PF_RISCV_ID << PFLAGS_BIT),
  RISCV_FLOAT_ABI_DOUBLE = 0x00000004 | (PF_RISCV_ID << PFLAGS_BIT),
  RISCV_FLOAT_ABI_QUAD   = 0x00000006 | (PF_RISCV_ID << PFLAGS_BIT),

  RISCV_FLOAT_ABI_RVE    = 0x00000008 | (PF_RISCV_ID << PFLAGS_BIT),
  RISCV_FLOAT_ABI_TSO    = 0x00000010 | (PF_RISCV_ID << PFLAGS_BIT),
};

LIEF_API const char* to_string(PROCESSOR_FLAGS flag);


}
}
#endif
