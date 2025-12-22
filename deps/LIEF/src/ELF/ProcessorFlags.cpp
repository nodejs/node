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
#include "LIEF/ELF/ProcessorFlags.hpp"
#include "frozen.hpp"

namespace LIEF {
namespace ELF {
const char* to_string(PROCESSOR_FLAGS flag) {
  #define ENTRY(X) std::pair(PROCESSOR_FLAGS::X, #X)
  STRING_MAP enums2str {
    ENTRY(ARM_EABI_UNKNOWN),
    ENTRY(ARM_SOFT_FLOAT),
    ENTRY(ARM_VFP_FLOAT),
    ENTRY(ARM_EABI_VER1),
    ENTRY(ARM_EABI_VER2),
    ENTRY(ARM_EABI_VER3),
    ENTRY(ARM_EABI_VER4),
    ENTRY(ARM_EABI_VER5),
    ENTRY(HEXAGON_MACH_V2),
    ENTRY(HEXAGON_MACH_V3),
    ENTRY(HEXAGON_MACH_V4),
    ENTRY(HEXAGON_MACH_V5),
    ENTRY(HEXAGON_ISA_V2),
    ENTRY(HEXAGON_ISA_V3),
    ENTRY(HEXAGON_ISA_V4),
    ENTRY(HEXAGON_ISA_V5),
    ENTRY(LOONGARCH_ABI_SOFT_FLOAT),
    ENTRY(LOONGARCH_ABI_SINGLE_FLOAT),
    ENTRY(LOONGARCH_ABI_DOUBLE_FLOAT),
    ENTRY(MIPS_NOREORDER),
    ENTRY(MIPS_PIC),
    ENTRY(MIPS_CPIC),
    ENTRY(MIPS_ABI2),
    ENTRY(MIPS_32BITMODE),
    ENTRY(MIPS_FP64),
    ENTRY(MIPS_NAN2008),
    ENTRY(MIPS_ABI_O32),
    ENTRY(MIPS_ABI_O64),
    ENTRY(MIPS_ABI_EABI32),
    ENTRY(MIPS_ABI_EABI64),
    ENTRY(MIPS_MACH_3900),
    ENTRY(MIPS_MACH_4010),
    ENTRY(MIPS_MACH_4100),
    ENTRY(MIPS_MACH_4650),
    ENTRY(MIPS_MACH_4120),
    ENTRY(MIPS_MACH_4111),
    ENTRY(MIPS_MACH_SB1),
    ENTRY(MIPS_MACH_OCTEON),
    ENTRY(MIPS_MACH_XLR),
    ENTRY(MIPS_MACH_OCTEON2),
    ENTRY(MIPS_MACH_OCTEON3),
    ENTRY(MIPS_MACH_5400),
    ENTRY(MIPS_MACH_5900),
    ENTRY(MIPS_MACH_5500),
    ENTRY(MIPS_MACH_9000),
    ENTRY(MIPS_MACH_LS2E),
    ENTRY(MIPS_MACH_LS2F),
    ENTRY(MIPS_MACH_LS3A),
    ENTRY(MIPS_MICROMIPS),
    ENTRY(MIPS_ARCH_ASE_M16),
    ENTRY(MIPS_ARCH_ASE_MDMX),
    ENTRY(MIPS_ARCH_1),
    ENTRY(MIPS_ARCH_2),
    ENTRY(MIPS_ARCH_3),
    ENTRY(MIPS_ARCH_4),
    ENTRY(MIPS_ARCH_5),
    ENTRY(MIPS_ARCH_32),
    ENTRY(MIPS_ARCH_64),
    ENTRY(MIPS_ARCH_32R2),
    ENTRY(MIPS_ARCH_64R2),
    ENTRY(MIPS_ARCH_32R6),
    ENTRY(MIPS_ARCH_64R6),
    ENTRY(RISCV_RVC),
    ENTRY(RISCV_FLOAT_ABI_SOFT),
    ENTRY(RISCV_FLOAT_ABI_SINGLE),
    ENTRY(RISCV_FLOAT_ABI_DOUBLE),
    ENTRY(RISCV_FLOAT_ABI_QUAD),
    ENTRY(RISCV_FLOAT_ABI_RVE),
    ENTRY(RISCV_FLOAT_ABI_TSO),
  };
  #undef ENTRY

  if (auto it = enums2str.find(flag); it != enums2str.end()) {
    return it->second;
  }
  return "UNKNOWN";
}
}
}
