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
#ifndef LIEF_ELF_ENUMS_H
#define LIEF_ELF_ENUMS_H
#include "LIEF/enums.hpp"
#include <cstdint>
#include <cstddef>

namespace LIEF {
namespace ELF {

/**
 * Machine architectures
 * See current registered ELF machine architectures at:
 * http://www.sco.com/developers/gabi/latest/ch4.eheader.html
 */
enum class ARCH {
  NONE          = 0,  /**< No machine */
  M32           = 1,  /**< AT&T WE 32100 */
  SPARC         = 2,  /**< SPARC */
  I386          = 3,  /**< Intel 386 */
  M68K          = 4,  /**< Motorola 68000 */
  M88K          = 5,  /**< Motorola 88000 */
  IAMCU         = 6,  /**< Intel MCU */
  I860          = 7,  /**< Intel 80860 */
  MIPS          = 8,  /**< MIPS R3000 */
  S370          = 9,  /**< IBM System/370 */
  MIPS_RS3_LE   = 10, /**< MIPS RS3000 Little-endian */
  PARISC        = 15, /**< Hewlett-Packard PA-RISC */
  VPP500        = 17, /**< Fujitsu VPP500 */
  SPARC32PLUS   = 18, /**< Enhanced instruction set SPARC */
  I60           = 19, /**< Intel 80960 */
  PPC           = 20, /**< PowerPC */
  PPC64         = 21, /**< PowerPC64 */
  S390          = 22, /**< IBM System/390 */
  SPU           = 23, /**< IBM SPU/SPC */
  V800          = 36, /**< NEC V800 */
  FR20          = 37, /**< Fujitsu FR20 */
  RH32          = 38, /**< TRW RH-32 */
  RCE           = 39, /**< Motorola RCE */
  ARM           = 40, /**< ARM */
  ALPHA         = 41, /**< DEC Alpha */
  SH            = 42, /**< Hitachi SH */
  SPARCV9       = 43, /**< SPARC V9 */
  TRICORE       = 44, /**< Siemens TriCore */
  ARC           = 45, /**< Argonaut RISC Core */
  H8_300        = 46, /**< Hitachi H8/300 */
  H8_300H       = 47, /**< Hitachi H8/300H */
  H8S           = 48, /**< Hitachi H8S */
  H8_500        = 49, /**< Hitachi H8/500 */
  IA_64         = 50, /**< Intel IA-64 processor architecture */
  MIPS_X        = 51, /**< Stanford MIPS-X */
  COLDFIRE      = 52, /**< Motorola ColdFire */
  M68HC12       = 53, /**< Motorola M68HC12 */
  MMA           = 54, /**< Fujitsu MMA Multimedia Accelerator */
  PCP           = 55, /**< Siemens PCP */
  NCPU          = 56, /**< Sony nCPU embedded RISC processor */
  NDR1          = 57, /**< Denso NDR1 microprocessor */
  STARCORE      = 58, /**< Motorola Star*Core processor */
  ME16          = 59, /**< Toyota ME16 processor */
  ST100         = 60, /**< STMicroelectronics ST100 processor */
  TINYJ         = 61, /**< Advanced Logic Corp. TinyJ embedded processor family */
  X86_64        = 62, /**< AMD x86-64 architecture */
  PDSP          = 63, /**< Sony DSP Processor */
  PDP10         = 64, /**< Digital Equipment Corp. PDP-10 */
  PDP11         = 65, /**< Digital Equipment Corp. PDP-11 */
  FX66          = 66, /**< Siemens FX66 microcontroller */
  ST9PLUS       = 67, /**< STMicroelectronics ST9+ 8/16 bit microcontroller */
  ST7           = 68, /**< STMicroelectronics ST7 8-bit microcontroller */
  M68HC16       = 69, /**< Motorola MC68HC16 Microcontroller */
  M68HC11       = 70, /**< Motorola MC68HC11 Microcontroller */
  M68HC08       = 71, /**< Motorola MC68HC08 Microcontroller */
  M68HC05       = 72, /**< Motorola MC68HC05 Microcontroller */
  SVX           = 73, /**< Silicon Graphics SVx */
  ST19          = 74, /**< STMicroelectronics ST19 8-bit microcontroller */
  VAX           = 75, /**< Digital VAX */
  CRIS          = 76, /**< Axis Communications 32-bit embedded processor */
  JAVELIN       = 77, /**< Infineon Technologies 32-bit embedded processor */
  FIREPATH      = 78, /**< Element 14 64-bit DSP Processor */
  ZSP           = 79, /**< LSI Logic 16-bit DSP Processor */
  MMIX          = 80, /**< Donald Knuth's educational 64-bit processor */
  HUANY         = 81, /**< Harvard University machine-independent object files */
  PRISM         = 82, /**< SiTera Prism */
  AVR           = 83, /**< Atmel AVR 8-bit microcontroller */
  FR30          = 84, /**< Fujitsu FR30 */
  D10V          = 85, /**< Mitsubishi D10V */
  D30V          = 86, /**< Mitsubishi D30V */
  V850          = 87, /**< NEC v850 */
  M32R          = 88, /**< Mitsubishi M32R */
  MN10300       = 89, /**< Matsushita MN10300 */
  MN10200       = 90, /**< Matsushita MN10200 */
  PJ            = 91, /**< picoJava */
  OPENRISC      = 92, /**< OpenRISC 32-bit embedded processor */
  ARC_COMPACT   = 93, /**< ARC International ARCompact processor (old spelling/synonym: EM_ARC_A5) */
  XTENSA        = 94,  /**< Tensilica Xtensa Architecture */
  VIDEOCORE     = 95,  /**< Alphamosaic VideoCore processor */
  TMM_GPP       = 96,  /**< Thompson Multimedia General Purpose Processor */
  NS32K         = 97,  /**< National Semiconductor 32000 series */
  TPC           = 98,  /**< Tenor Network TPC processor */
  SNP1K         = 99,  /**< Trebia SNP 1000 processor */
  ST200         = 100, /**< STMicroelectronics (www.st.com) ST200 */
  IP2K          = 101, /**< Ubicom IP2xxx microcontroller family */
  MAX           = 102, /**< MAX Processor */
  CR            = 103, /**< National Semiconductor CompactRISC microprocessor */
  F2MC16        = 104, /**< Fujitsu F2MC16 */
  MSP430        = 105, /**< Texas Instruments embedded microcontroller msp430 */
  BLACKFIN      = 106, /**< Analog Devices Blackfin (DSP) processor */
  SE_C33        = 107, /**< S1C33 Family of Seiko Epson processors */
  SEP           = 108, /**< Sharp embedded microprocessor */
  ARCA          = 109, /**< Arca RISC Microprocessor */
  UNICORE       = 110, /**< Microprocessor series from PKU-Unity Ltd. and MPRC of Peking University */
  EXCESS        = 111, /**< eXcess: 16/32/64-bit configurable embedded CPU */
  DXP           = 112, /**< Icera Semiconductor Inc. Deep Execution Processor */
  ALTERA_NIOS2  = 113, /**< Altera Nios II soft-core processor */
  CRX           = 114, /**< National Semiconductor CompactRISC CRX */
  XGATE         = 115, /**< Motorola XGATE embedded processor */
  C166          = 116, /**< Infineon C16x/XC16x processor */
  M16C          = 117, /**< Renesas M16C series microprocessors */
  DSPIC30F      = 118, /**< Microchip Technology dsPIC30F Digital Signal */
  CE            = 119, /**< Freescale Communication Engine RISC core */
  M32C          = 120, /**< Renesas M32C series microprocessors */
  TSK3000       = 131, /**< Altium TSK3000 core */
  RS08          = 132, /**< Freescale RS08 embedded processor */
  SHARC         = 133, /**< Analog Devices SHARC family of 32-bit DSP */
  ECOG2         = 134, /**< Cyan Technology eCOG2 microprocessor */
  SCORE7        = 135, /**< Sunplus S+core7 RISC processor */
  DSP24         = 136, /**< New Japan Radio (NJR) 24-bit DSP Processor */
  VIDEOCORE3    = 137, /**< Broadcom VideoCore III processor */
  LATTICEMICO32 = 138, /**< RISC processor for Lattice FPGA architecture */
  SE_C17        = 139, /**< Seiko Epson C17 family */
  TI_C6000      = 140, /**< The Texas Instruments TMS320C6000 DSP family */
  TI_C2000      = 141, /**< The Texas Instruments TMS320C2000 DSP family */
  TI_C5500      = 142, /**< The Texas Instruments TMS320C55x DSP family */
  MMDSP_PLUS    = 160, /**< STMicroelectronics 64bit VLIW Data Signal Processor */
  CYPRESS_M8C   = 161, /**< Cypress M8C microprocessor */
  R32C          = 162, /**< Renesas R32C series microprocessors */
  TRIMEDIA      = 163, /**< NXP Semiconductors TriMedia architecture family */
  HEXAGON       = 164, /**< Qualcomm Hexagon processor */
  M8051          = 165, /**< Intel 8051 and variants */
  STXP7X        = 166, /**< STMicroelectronics STxP7x family of configurable */
  NDS32         = 167, /* Andes Technology compact code size embedded RISC */
  ECOG1         = 168, /**< Cyan Technology eCOG1X family */
  ECOG1X        = 168, /**< Cyan Technology eCOG1X family */
  MAXQ30        = 169, /**< Dallas Semiconductor MAXQ30 Core Micro-controllers */
  XIMO16        = 170, /**< New Japan Radio (NJR) 16-bit DSP Processor */
  MANIK         = 171, /**< M2000 Reconfigurable RISC Microprocessor */
  CRAYNV2       = 172, /**< Cray Inc. NV2 vector architecture */
  RX            = 173, /**< Renesas RX family */
  METAG         = 174, /**< Imagination Technologies META processor */
  MCST_ELBRUS   = 175, /**< MCST Elbrus general purpose hardware architecture */
  ECOG16        = 176, /**< Cyan Technology eCOG16 family */
  CR16          = 177, /**< National Semiconductor CompactRISC CR16 16-bit */
  ETPU          = 178, /**< Freescale Extended Time Processing Unit */
  SLE9X         = 179, /**< Infineon Technologies SLE9X core */
  L10M          = 180, /**< Intel L10M */
  K10M          = 181, /**< Intel K10M */
  AARCH64       = 183, /**< ARM AArch64 */
  AVR32         = 185, /**< Atmel Corporation 32-bit microprocessor family */
  STM8          = 186, /**< STMicroeletronics STM8 8-bit microcontroller */
  TILE64        = 187, /**< Tilera TILE64 multicore architecture family */
  TILEPRO       = 188, /**< Tilera TILEPro multicore architecture family */
  CUDA          = 190, /**< NVIDIA CUDA architecture */
  TILEGX        = 191, /**< Tilera TILE-Gx multicore architecture family */
  CLOUDSHIELD   = 192, /**< CloudShield architecture family */
  COREA_1ST     = 193, /**< KIPO-KAIST Core-A 1st generation processor family */
  COREA_2ND     = 194, /**< KIPO-KAIST Core-A 2nd generation processor family */
  ARC_COMPACT2  = 195, /**< Synopsys ARCompact V2 */
  OPEN8         = 196, /**< Open8 8-bit RISC soft processor core */
  RL78          = 197, /**< Renesas RL78 family */
  VIDEOCORE5    = 198, /**< Broadcom VideoCore V processor */
  M78KOR        = 199, /**< Renesas 78KOR family */
  M56800EX      = 200, /**< Freescale 56800EX Digital Signal Controller (DSC) */
  BA1           = 201, /**< Beyond BA1 CPU architecture */
  BA2           = 202, /**< Beyond BA2 CPU architecture */
  XCORE         = 203, /**< XMOS xCORE processor family */
  MCHP_PIC      = 204, /**< Microchip 8-bit PIC(r) family */
  INTEL205      = 205, /**< Reserved by Intel */
  INTEL206      = 206, /**< Reserved by Intel */
  INTEL207      = 207, /**< Reserved by Intel */
  INTEL208      = 208, /**< Reserved by Intel */
  INTEL209      = 209, /**< Reserved by Intel */
  KM32          = 210, /**< KM211 KM32 32-bit processor */
  KMX32         = 211, /**< KM211 KMX32 32-bit processor */
  KMX16         = 212, /**< KM211 KMX16 16-bit processor */
  KMX8          = 213, /**< KM211 KMX8 8-bit processor */
  KVARC         = 214, /**< KM211 KVARC processor */
  CDP           = 215, /**< Paneve CDP architecture family */
  COGE          = 216, /**< Cognitive Smart Memory Processor */
  COOL          = 217, /**< iCelero CoolEngine */
  NORC          = 218, /**< Nanoradio Optimized RISC */
  CSR_KALIMBA   = 219, /**< CSR Kalimba architecture family */
  AMDGPU        = 224, /**< AMD GPU architecture */
  RISCV         = 243, /**< RISC-V */
  BPF           = 247, /**< eBPF Filter */
  CSKY          = 252, /**< C-SKY */
  LOONGARCH     = 258,  /**< LoongArch */

  ALPHA_ALT     = 0x9026
};

}
}

#endif
