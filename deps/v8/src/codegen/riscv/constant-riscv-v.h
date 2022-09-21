// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef V8_CODEGEN_RISCV_CONSTANT_RISCV_V_H_
#define V8_CODEGEN_RISCV_CONSTANT_RISCV_V_H_

#include "src/codegen/riscv/base-constants-riscv.h"
namespace v8 {
namespace internal {

enum OpcodeRISCVV : uint32_t {
  // RVV Extension
  OP_IVV = OP_V | (0b000 << kFunct3Shift),
  OP_FVV = OP_V | (0b001 << kFunct3Shift),
  OP_MVV = OP_V | (0b010 << kFunct3Shift),
  OP_IVI = OP_V | (0b011 << kFunct3Shift),
  OP_IVX = OP_V | (0b100 << kFunct3Shift),
  OP_FVF = OP_V | (0b101 << kFunct3Shift),
  OP_MVX = OP_V | (0b110 << kFunct3Shift),

  RO_V_VSETVLI = OP_V | (0b111 << kFunct3Shift) | 0b0 << 31,
  RO_V_VSETIVLI = OP_V | (0b111 << kFunct3Shift) | 0b11 << 30,
  RO_V_VSETVL = OP_V | (0b111 << kFunct3Shift) | 0b1 << 31,

  // RVV LOAD/STORE
  RO_V_VL = LOAD_FP | (0b00 << kRvvMopShift) | (0b000 << kRvvNfShift),
  RO_V_VLS = LOAD_FP | (0b10 << kRvvMopShift) | (0b000 << kRvvNfShift),
  RO_V_VLX = LOAD_FP | (0b11 << kRvvMopShift) | (0b000 << kRvvNfShift),

  RO_V_VS = STORE_FP | (0b00 << kRvvMopShift) | (0b000 << kRvvNfShift),
  RO_V_VSS = STORE_FP | (0b10 << kRvvMopShift) | (0b000 << kRvvNfShift),
  RO_V_VSX = STORE_FP | (0b11 << kRvvMopShift) | (0b000 << kRvvNfShift),
  RO_V_VSU = STORE_FP | (0b01 << kRvvMopShift) | (0b000 << kRvvNfShift),
  // THE kFunct6Shift is mop
  RO_V_VLSEG2 = LOAD_FP | (0b00 << kRvvMopShift) | (0b001 << kRvvNfShift),
  RO_V_VLSEG3 = LOAD_FP | (0b00 << kRvvMopShift) | (0b010 << kRvvNfShift),
  RO_V_VLSEG4 = LOAD_FP | (0b00 << kRvvMopShift) | (0b011 << kRvvNfShift),
  RO_V_VLSEG5 = LOAD_FP | (0b00 << kRvvMopShift) | (0b100 << kRvvNfShift),
  RO_V_VLSEG6 = LOAD_FP | (0b00 << kRvvMopShift) | (0b101 << kRvvNfShift),
  RO_V_VLSEG7 = LOAD_FP | (0b00 << kRvvMopShift) | (0b110 << kRvvNfShift),
  RO_V_VLSEG8 = LOAD_FP | (0b00 << kRvvMopShift) | (0b111 << kRvvNfShift),

  RO_V_VSSEG2 = STORE_FP | (0b00 << kRvvMopShift) | (0b001 << kRvvNfShift),
  RO_V_VSSEG3 = STORE_FP | (0b00 << kRvvMopShift) | (0b010 << kRvvNfShift),
  RO_V_VSSEG4 = STORE_FP | (0b00 << kRvvMopShift) | (0b011 << kRvvNfShift),
  RO_V_VSSEG5 = STORE_FP | (0b00 << kRvvMopShift) | (0b100 << kRvvNfShift),
  RO_V_VSSEG6 = STORE_FP | (0b00 << kRvvMopShift) | (0b101 << kRvvNfShift),
  RO_V_VSSEG7 = STORE_FP | (0b00 << kRvvMopShift) | (0b110 << kRvvNfShift),
  RO_V_VSSEG8 = STORE_FP | (0b00 << kRvvMopShift) | (0b111 << kRvvNfShift),

  RO_V_VLSSEG2 = LOAD_FP | (0b10 << kRvvMopShift) | (0b001 << kRvvNfShift),
  RO_V_VLSSEG3 = LOAD_FP | (0b10 << kRvvMopShift) | (0b010 << kRvvNfShift),
  RO_V_VLSSEG4 = LOAD_FP | (0b10 << kRvvMopShift) | (0b011 << kRvvNfShift),
  RO_V_VLSSEG5 = LOAD_FP | (0b10 << kRvvMopShift) | (0b100 << kRvvNfShift),
  RO_V_VLSSEG6 = LOAD_FP | (0b10 << kRvvMopShift) | (0b101 << kRvvNfShift),
  RO_V_VLSSEG7 = LOAD_FP | (0b10 << kRvvMopShift) | (0b110 << kRvvNfShift),
  RO_V_VLSSEG8 = LOAD_FP | (0b10 << kRvvMopShift) | (0b111 << kRvvNfShift),

  RO_V_VSSSEG2 = STORE_FP | (0b10 << kRvvMopShift) | (0b001 << kRvvNfShift),
  RO_V_VSSSEG3 = STORE_FP | (0b10 << kRvvMopShift) | (0b010 << kRvvNfShift),
  RO_V_VSSSEG4 = STORE_FP | (0b10 << kRvvMopShift) | (0b011 << kRvvNfShift),
  RO_V_VSSSEG5 = STORE_FP | (0b10 << kRvvMopShift) | (0b100 << kRvvNfShift),
  RO_V_VSSSEG6 = STORE_FP | (0b10 << kRvvMopShift) | (0b101 << kRvvNfShift),
  RO_V_VSSSEG7 = STORE_FP | (0b10 << kRvvMopShift) | (0b110 << kRvvNfShift),
  RO_V_VSSSEG8 = STORE_FP | (0b10 << kRvvMopShift) | (0b111 << kRvvNfShift),

  RO_V_VLXSEG2 = LOAD_FP | (0b11 << kRvvMopShift) | (0b001 << kRvvNfShift),
  RO_V_VLXSEG3 = LOAD_FP | (0b11 << kRvvMopShift) | (0b010 << kRvvNfShift),
  RO_V_VLXSEG4 = LOAD_FP | (0b11 << kRvvMopShift) | (0b011 << kRvvNfShift),
  RO_V_VLXSEG5 = LOAD_FP | (0b11 << kRvvMopShift) | (0b100 << kRvvNfShift),
  RO_V_VLXSEG6 = LOAD_FP | (0b11 << kRvvMopShift) | (0b101 << kRvvNfShift),
  RO_V_VLXSEG7 = LOAD_FP | (0b11 << kRvvMopShift) | (0b110 << kRvvNfShift),
  RO_V_VLXSEG8 = LOAD_FP | (0b11 << kRvvMopShift) | (0b111 << kRvvNfShift),

  RO_V_VSXSEG2 = STORE_FP | (0b11 << kRvvMopShift) | (0b001 << kRvvNfShift),
  RO_V_VSXSEG3 = STORE_FP | (0b11 << kRvvMopShift) | (0b010 << kRvvNfShift),
  RO_V_VSXSEG4 = STORE_FP | (0b11 << kRvvMopShift) | (0b011 << kRvvNfShift),
  RO_V_VSXSEG5 = STORE_FP | (0b11 << kRvvMopShift) | (0b100 << kRvvNfShift),
  RO_V_VSXSEG6 = STORE_FP | (0b11 << kRvvMopShift) | (0b101 << kRvvNfShift),
  RO_V_VSXSEG7 = STORE_FP | (0b11 << kRvvMopShift) | (0b110 << kRvvNfShift),
  RO_V_VSXSEG8 = STORE_FP | (0b11 << kRvvMopShift) | (0b111 << kRvvNfShift),

  // RVV Vector Arithmetic Instruction
  VADD_FUNCT6 = 0b000000,
  RO_V_VADD_VI = OP_IVI | (VADD_FUNCT6 << kRvvFunct6Shift),
  RO_V_VADD_VV = OP_IVV | (VADD_FUNCT6 << kRvvFunct6Shift),
  RO_V_VADD_VX = OP_IVX | (VADD_FUNCT6 << kRvvFunct6Shift),

  VSUB_FUNCT6 = 0b000010,
  RO_V_VSUB_VX = OP_IVX | (VSUB_FUNCT6 << kRvvFunct6Shift),
  RO_V_VSUB_VV = OP_IVV | (VSUB_FUNCT6 << kRvvFunct6Shift),

  VDIVU_FUNCT6 = 0b100000,
  RO_V_VDIVU_VX = OP_MVX | (VDIVU_FUNCT6 << kRvvFunct6Shift),
  RO_V_VDIVU_VV = OP_MVV | (VDIVU_FUNCT6 << kRvvFunct6Shift),

  VDIV_FUNCT6 = 0b100001,
  RO_V_VDIV_VX = OP_MVX | (VDIV_FUNCT6 << kRvvFunct6Shift),
  RO_V_VDIV_VV = OP_MVV | (VDIV_FUNCT6 << kRvvFunct6Shift),

  VREMU_FUNCT6 = 0b100010,
  RO_V_VREMU_VX = OP_MVX | (VREMU_FUNCT6 << kRvvFunct6Shift),
  RO_V_VREMU_VV = OP_MVV | (VREMU_FUNCT6 << kRvvFunct6Shift),

  VREM_FUNCT6 = 0b100011,
  RO_V_VREM_VX = OP_MVX | (VREM_FUNCT6 << kRvvFunct6Shift),
  RO_V_VREM_VV = OP_MVV | (VREM_FUNCT6 << kRvvFunct6Shift),

  VMULHU_FUNCT6 = 0b100100,
  RO_V_VMULHU_VX = OP_MVX | (VMULHU_FUNCT6 << kRvvFunct6Shift),
  RO_V_VMULHU_VV = OP_MVV | (VMULHU_FUNCT6 << kRvvFunct6Shift),

  VMUL_FUNCT6 = 0b100101,
  RO_V_VMUL_VX = OP_MVX | (VMUL_FUNCT6 << kRvvFunct6Shift),
  RO_V_VMUL_VV = OP_MVV | (VMUL_FUNCT6 << kRvvFunct6Shift),

  VWMUL_FUNCT6 = 0b111011,
  RO_V_VWMUL_VX = OP_MVX | (VWMUL_FUNCT6 << kRvvFunct6Shift),
  RO_V_VWMUL_VV = OP_MVV | (VWMUL_FUNCT6 << kRvvFunct6Shift),

  VWMULU_FUNCT6 = 0b111000,
  RO_V_VWMULU_VX = OP_MVX | (VWMULU_FUNCT6 << kRvvFunct6Shift),
  RO_V_VWMULU_VV = OP_MVV | (VWMULU_FUNCT6 << kRvvFunct6Shift),

  VMULHSU_FUNCT6 = 0b100110,
  RO_V_VMULHSU_VX = OP_MVX | (VMULHSU_FUNCT6 << kRvvFunct6Shift),
  RO_V_VMULHSU_VV = OP_MVV | (VMULHSU_FUNCT6 << kRvvFunct6Shift),

  VMULH_FUNCT6 = 0b100111,
  RO_V_VMULH_VX = OP_MVX | (VMULH_FUNCT6 << kRvvFunct6Shift),
  RO_V_VMULH_VV = OP_MVV | (VMULH_FUNCT6 << kRvvFunct6Shift),

  VWADD_FUNCT6 = 0b110001,
  RO_V_VWADD_VV = OP_MVV | (VWADD_FUNCT6 << kRvvFunct6Shift),
  RO_V_VWADD_VX = OP_MVX | (VWADD_FUNCT6 << kRvvFunct6Shift),

  VWADDU_FUNCT6 = 0b110000,
  RO_V_VWADDU_VV = OP_MVV | (VWADDU_FUNCT6 << kRvvFunct6Shift),
  RO_V_VWADDU_VX = OP_MVX | (VWADDU_FUNCT6 << kRvvFunct6Shift),

  VWADDUW_FUNCT6 = 0b110101,
  RO_V_VWADDUW_VX = OP_MVX | (VWADDUW_FUNCT6 << kRvvFunct6Shift),
  RO_V_VWADDUW_VV = OP_MVV | (VWADDUW_FUNCT6 << kRvvFunct6Shift),

  VCOMPRESS_FUNCT6 = 0b010111,
  RO_V_VCOMPRESS_VV = OP_MVV | (VCOMPRESS_FUNCT6 << kRvvFunct6Shift),

  VSADDU_FUNCT6 = 0b100000,
  RO_V_VSADDU_VI = OP_IVI | (VSADDU_FUNCT6 << kRvvFunct6Shift),
  RO_V_VSADDU_VV = OP_IVV | (VSADDU_FUNCT6 << kRvvFunct6Shift),
  RO_V_VSADDU_VX = OP_IVX | (VSADDU_FUNCT6 << kRvvFunct6Shift),

  VSADD_FUNCT6 = 0b100001,
  RO_V_VSADD_VI = OP_IVI | (VSADD_FUNCT6 << kRvvFunct6Shift),
  RO_V_VSADD_VV = OP_IVV | (VSADD_FUNCT6 << kRvvFunct6Shift),
  RO_V_VSADD_VX = OP_IVX | (VSADD_FUNCT6 << kRvvFunct6Shift),

  VSSUB_FUNCT6 = 0b100011,
  RO_V_VSSUB_VV = OP_IVV | (VSSUB_FUNCT6 << kRvvFunct6Shift),
  RO_V_VSSUB_VX = OP_IVX | (VSSUB_FUNCT6 << kRvvFunct6Shift),

  VSSUBU_FUNCT6 = 0b100010,
  RO_V_VSSUBU_VV = OP_IVV | (VSSUBU_FUNCT6 << kRvvFunct6Shift),
  RO_V_VSSUBU_VX = OP_IVX | (VSSUBU_FUNCT6 << kRvvFunct6Shift),

  VRSUB_FUNCT6 = 0b000011,
  RO_V_VRSUB_VX = OP_IVX | (VRSUB_FUNCT6 << kRvvFunct6Shift),
  RO_V_VRSUB_VI = OP_IVI | (VRSUB_FUNCT6 << kRvvFunct6Shift),

  VMINU_FUNCT6 = 0b000100,
  RO_V_VMINU_VX = OP_IVX | (VMINU_FUNCT6 << kRvvFunct6Shift),
  RO_V_VMINU_VV = OP_IVV | (VMINU_FUNCT6 << kRvvFunct6Shift),

  VMIN_FUNCT6 = 0b000101,
  RO_V_VMIN_VX = OP_IVX | (VMIN_FUNCT6 << kRvvFunct6Shift),
  RO_V_VMIN_VV = OP_IVV | (VMIN_FUNCT6 << kRvvFunct6Shift),

  VMAXU_FUNCT6 = 0b000110,
  RO_V_VMAXU_VX = OP_IVX | (VMAXU_FUNCT6 << kRvvFunct6Shift),
  RO_V_VMAXU_VV = OP_IVV | (VMAXU_FUNCT6 << kRvvFunct6Shift),

  VMAX_FUNCT6 = 0b000111,
  RO_V_VMAX_VX = OP_IVX | (VMAX_FUNCT6 << kRvvFunct6Shift),
  RO_V_VMAX_VV = OP_IVV | (VMAX_FUNCT6 << kRvvFunct6Shift),

  VAND_FUNCT6 = 0b001001,
  RO_V_VAND_VI = OP_IVI | (VAND_FUNCT6 << kRvvFunct6Shift),
  RO_V_VAND_VV = OP_IVV | (VAND_FUNCT6 << kRvvFunct6Shift),
  RO_V_VAND_VX = OP_IVX | (VAND_FUNCT6 << kRvvFunct6Shift),

  VOR_FUNCT6 = 0b001010,
  RO_V_VOR_VI = OP_IVI | (VOR_FUNCT6 << kRvvFunct6Shift),
  RO_V_VOR_VV = OP_IVV | (VOR_FUNCT6 << kRvvFunct6Shift),
  RO_V_VOR_VX = OP_IVX | (VOR_FUNCT6 << kRvvFunct6Shift),

  VXOR_FUNCT6 = 0b001011,
  RO_V_VXOR_VI = OP_IVI | (VXOR_FUNCT6 << kRvvFunct6Shift),
  RO_V_VXOR_VV = OP_IVV | (VXOR_FUNCT6 << kRvvFunct6Shift),
  RO_V_VXOR_VX = OP_IVX | (VXOR_FUNCT6 << kRvvFunct6Shift),

  VRGATHER_FUNCT6 = 0b001100,
  RO_V_VRGATHER_VI = OP_IVI | (VRGATHER_FUNCT6 << kRvvFunct6Shift),
  RO_V_VRGATHER_VV = OP_IVV | (VRGATHER_FUNCT6 << kRvvFunct6Shift),
  RO_V_VRGATHER_VX = OP_IVX | (VRGATHER_FUNCT6 << kRvvFunct6Shift),

  VMV_FUNCT6 = 0b010111,
  RO_V_VMV_VI = OP_IVI | (VMV_FUNCT6 << kRvvFunct6Shift),
  RO_V_VMV_VV = OP_IVV | (VMV_FUNCT6 << kRvvFunct6Shift),
  RO_V_VMV_VX = OP_IVX | (VMV_FUNCT6 << kRvvFunct6Shift),
  RO_V_VFMV_VF = OP_FVF | (VMV_FUNCT6 << kRvvFunct6Shift),

  RO_V_VMERGE_VI = RO_V_VMV_VI,
  RO_V_VMERGE_VV = RO_V_VMV_VV,
  RO_V_VMERGE_VX = RO_V_VMV_VX,

  VMSEQ_FUNCT6 = 0b011000,
  RO_V_VMSEQ_VI = OP_IVI | (VMSEQ_FUNCT6 << kRvvFunct6Shift),
  RO_V_VMSEQ_VV = OP_IVV | (VMSEQ_FUNCT6 << kRvvFunct6Shift),
  RO_V_VMSEQ_VX = OP_IVX | (VMSEQ_FUNCT6 << kRvvFunct6Shift),

  VMSNE_FUNCT6 = 0b011001,
  RO_V_VMSNE_VI = OP_IVI | (VMSNE_FUNCT6 << kRvvFunct6Shift),
  RO_V_VMSNE_VV = OP_IVV | (VMSNE_FUNCT6 << kRvvFunct6Shift),
  RO_V_VMSNE_VX = OP_IVX | (VMSNE_FUNCT6 << kRvvFunct6Shift),

  VMSLTU_FUNCT6 = 0b011010,
  RO_V_VMSLTU_VV = OP_IVV | (VMSLTU_FUNCT6 << kRvvFunct6Shift),
  RO_V_VMSLTU_VX = OP_IVX | (VMSLTU_FUNCT6 << kRvvFunct6Shift),

  VMSLT_FUNCT6 = 0b011011,
  RO_V_VMSLT_VV = OP_IVV | (VMSLT_FUNCT6 << kRvvFunct6Shift),
  RO_V_VMSLT_VX = OP_IVX | (VMSLT_FUNCT6 << kRvvFunct6Shift),

  VMSLE_FUNCT6 = 0b011101,
  RO_V_VMSLE_VI = OP_IVI | (VMSLE_FUNCT6 << kRvvFunct6Shift),
  RO_V_VMSLE_VV = OP_IVV | (VMSLE_FUNCT6 << kRvvFunct6Shift),
  RO_V_VMSLE_VX = OP_IVX | (VMSLE_FUNCT6 << kRvvFunct6Shift),

  VMSLEU_FUNCT6 = 0b011100,
  RO_V_VMSLEU_VI = OP_IVI | (VMSLEU_FUNCT6 << kRvvFunct6Shift),
  RO_V_VMSLEU_VV = OP_IVV | (VMSLEU_FUNCT6 << kRvvFunct6Shift),
  RO_V_VMSLEU_VX = OP_IVX | (VMSLEU_FUNCT6 << kRvvFunct6Shift),

  VMSGTU_FUNCT6 = 0b011110,
  RO_V_VMSGTU_VI = OP_IVI | (VMSGTU_FUNCT6 << kRvvFunct6Shift),
  RO_V_VMSGTU_VX = OP_IVX | (VMSGTU_FUNCT6 << kRvvFunct6Shift),

  VMSGT_FUNCT6 = 0b011111,
  RO_V_VMSGT_VI = OP_IVI | (VMSGT_FUNCT6 << kRvvFunct6Shift),
  RO_V_VMSGT_VX = OP_IVX | (VMSGT_FUNCT6 << kRvvFunct6Shift),

  VSLIDEUP_FUNCT6 = 0b001110,
  RO_V_VSLIDEUP_VI = OP_IVI | (VSLIDEUP_FUNCT6 << kRvvFunct6Shift),
  RO_V_VSLIDEUP_VX = OP_IVX | (VSLIDEUP_FUNCT6 << kRvvFunct6Shift),

  VSLIDEDOWN_FUNCT6 = 0b001111,
  RO_V_VSLIDEDOWN_VI = OP_IVI | (VSLIDEDOWN_FUNCT6 << kRvvFunct6Shift),
  RO_V_VSLIDEDOWN_VX = OP_IVX | (VSLIDEDOWN_FUNCT6 << kRvvFunct6Shift),

  VSRL_FUNCT6 = 0b101000,
  RO_V_VSRL_VI = OP_IVI | (VSRL_FUNCT6 << kRvvFunct6Shift),
  RO_V_VSRL_VV = OP_IVV | (VSRL_FUNCT6 << kRvvFunct6Shift),
  RO_V_VSRL_VX = OP_IVX | (VSRL_FUNCT6 << kRvvFunct6Shift),

  VSRA_FUNCT6 = 0b101001,
  RO_V_VSRA_VI = OP_IVI | (VSRA_FUNCT6 << kRvvFunct6Shift),
  RO_V_VSRA_VV = OP_IVV | (VSRA_FUNCT6 << kRvvFunct6Shift),
  RO_V_VSRA_VX = OP_IVX | (VSRA_FUNCT6 << kRvvFunct6Shift),

  VSLL_FUNCT6 = 0b100101,
  RO_V_VSLL_VI = OP_IVI | (VSLL_FUNCT6 << kRvvFunct6Shift),
  RO_V_VSLL_VV = OP_IVV | (VSLL_FUNCT6 << kRvvFunct6Shift),
  RO_V_VSLL_VX = OP_IVX | (VSLL_FUNCT6 << kRvvFunct6Shift),

  VSMUL_FUNCT6 = 0b100111,
  RO_V_VSMUL_VV = OP_IVV | (VSMUL_FUNCT6 << kRvvFunct6Shift),
  RO_V_VSMUL_VX = OP_IVX | (VSMUL_FUNCT6 << kRvvFunct6Shift),

  VADC_FUNCT6 = 0b010000,
  RO_V_VADC_VI = OP_IVI | (VADC_FUNCT6 << kRvvFunct6Shift),
  RO_V_VADC_VV = OP_IVV | (VADC_FUNCT6 << kRvvFunct6Shift),
  RO_V_VADC_VX = OP_IVX | (VADC_FUNCT6 << kRvvFunct6Shift),

  VMADC_FUNCT6 = 0b010001,
  RO_V_VMADC_VI = OP_IVI | (VMADC_FUNCT6 << kRvvFunct6Shift),
  RO_V_VMADC_VV = OP_IVV | (VMADC_FUNCT6 << kRvvFunct6Shift),
  RO_V_VMADC_VX = OP_IVX | (VMADC_FUNCT6 << kRvvFunct6Shift),

  VWXUNARY0_FUNCT6 = 0b010000,
  VRXUNARY0_FUNCT6 = 0b010000,
  VMUNARY0_FUNCT6 = 0b010100,

  RO_V_VWXUNARY0 = OP_MVV | (VWXUNARY0_FUNCT6 << kRvvFunct6Shift),
  RO_V_VRXUNARY0 = OP_MVX | (VRXUNARY0_FUNCT6 << kRvvFunct6Shift),
  RO_V_VMUNARY0 = OP_MVV | (VMUNARY0_FUNCT6 << kRvvFunct6Shift),

  VID_V = 0b10001,

  VXUNARY0_FUNCT6 = 0b010010,
  RO_V_VXUNARY0 = OP_MVV | (VXUNARY0_FUNCT6 << kRvvFunct6Shift),

  VWFUNARY0_FUNCT6 = 0b010000,
  RO_V_VFMV_FS = OP_FVV | (VWFUNARY0_FUNCT6 << kRvvFunct6Shift),

  VRFUNARY0_FUNCT6 = 0b010000,
  RO_V_VFMV_SF = OP_FVF | (VRFUNARY0_FUNCT6 << kRvvFunct6Shift),

  VREDMAXU_FUNCT6 = 0b000110,
  RO_V_VREDMAXU = OP_MVV | (VREDMAXU_FUNCT6 << kRvvFunct6Shift),
  VREDMAX_FUNCT6 = 0b000111,
  RO_V_VREDMAX = OP_MVV | (VREDMAX_FUNCT6 << kRvvFunct6Shift),

  VREDMINU_FUNCT6 = 0b000100,
  RO_V_VREDMINU = OP_MVV | (VREDMINU_FUNCT6 << kRvvFunct6Shift),
  VREDMIN_FUNCT6 = 0b000101,
  RO_V_VREDMIN = OP_MVV | (VREDMIN_FUNCT6 << kRvvFunct6Shift),

  VFUNARY0_FUNCT6 = 0b010010,
  RO_V_VFUNARY0 = OP_FVV | (VFUNARY0_FUNCT6 << kRvvFunct6Shift),
  VFUNARY1_FUNCT6 = 0b010011,
  RO_V_VFUNARY1 = OP_FVV | (VFUNARY1_FUNCT6 << kRvvFunct6Shift),

  VFCVT_XU_F_V = 0b00000,
  VFCVT_X_F_V = 0b00001,
  VFCVT_F_XU_V = 0b00010,
  VFCVT_F_X_V = 0b00011,
  VFWCVT_XU_F_V = 0b01000,
  VFWCVT_X_F_V = 0b01001,
  VFWCVT_F_XU_V = 0b01010,
  VFWCVT_F_X_V = 0b01011,
  VFWCVT_F_F_V = 0b01100,
  VFNCVT_F_F_W = 0b10100,
  VFNCVT_X_F_W = 0b10001,
  VFNCVT_XU_F_W = 0b10000,

  VFCLASS_V = 0b10000,
  VFSQRT_V = 0b00000,
  VFRSQRT7_V = 0b00100,
  VFREC7_V = 0b00101,

  VFADD_FUNCT6 = 0b000000,
  RO_V_VFADD_VV = OP_FVV | (VFADD_FUNCT6 << kRvvFunct6Shift),
  RO_V_VFADD_VF = OP_FVF | (VFADD_FUNCT6 << kRvvFunct6Shift),

  VFSUB_FUNCT6 = 0b000010,
  RO_V_VFSUB_VV = OP_FVV | (VFSUB_FUNCT6 << kRvvFunct6Shift),
  RO_V_VFSUB_VF = OP_FVF | (VFSUB_FUNCT6 << kRvvFunct6Shift),

  VFDIV_FUNCT6 = 0b100000,
  RO_V_VFDIV_VV = OP_FVV | (VFDIV_FUNCT6 << kRvvFunct6Shift),
  RO_V_VFDIV_VF = OP_FVF | (VFDIV_FUNCT6 << kRvvFunct6Shift),

  VFMUL_FUNCT6 = 0b100100,
  RO_V_VFMUL_VV = OP_FVV | (VFMUL_FUNCT6 << kRvvFunct6Shift),
  RO_V_VFMUL_VF = OP_FVF | (VFMUL_FUNCT6 << kRvvFunct6Shift),

  // Vector Widening Floating-Point Add/Subtract Instructions
  VFWADD_FUNCT6 = 0b110000,
  RO_V_VFWADD_VV = OP_FVV | (VFWADD_FUNCT6 << kRvvFunct6Shift),
  RO_V_VFWADD_VF = OP_FVF | (VFWADD_FUNCT6 << kRvvFunct6Shift),

  VFWSUB_FUNCT6 = 0b110010,
  RO_V_VFWSUB_VV = OP_FVV | (VFWSUB_FUNCT6 << kRvvFunct6Shift),
  RO_V_VFWSUB_VF = OP_FVF | (VFWSUB_FUNCT6 << kRvvFunct6Shift),

  VFWADD_W_FUNCT6 = 0b110100,
  RO_V_VFWADD_W_VV = OP_FVV | (VFWADD_W_FUNCT6 << kRvvFunct6Shift),
  RO_V_VFWADD_W_VF = OP_FVF | (VFWADD_W_FUNCT6 << kRvvFunct6Shift),

  VFWSUB_W_FUNCT6 = 0b110110,
  RO_V_VFWSUB_W_VV = OP_FVV | (VFWSUB_W_FUNCT6 << kRvvFunct6Shift),
  RO_V_VFWSUB_W_VF = OP_FVF | (VFWSUB_W_FUNCT6 << kRvvFunct6Shift),

  // Vector Widening Floating-Point Reduction Instructions
  VFWREDUSUM_FUNCT6 = 0b110001,
  RO_V_VFWREDUSUM_VV = OP_FVV | (VFWREDUSUM_FUNCT6 << kRvvFunct6Shift),

  VFWREDOSUM_FUNCT6 = 0b110011,
  RO_V_VFWREDOSUM_VV = OP_FVV | (VFWREDOSUM_FUNCT6 << kRvvFunct6Shift),

  // Vector Widening Floating-Point Multiply
  VFWMUL_FUNCT6 = 0b111000,
  RO_V_VFWMUL_VV = OP_FVV | (VFWMUL_FUNCT6 << kRvvFunct6Shift),
  RO_V_VFWMUL_VF = OP_FVF | (VFWMUL_FUNCT6 << kRvvFunct6Shift),

  VMFEQ_FUNCT6 = 0b011000,
  RO_V_VMFEQ_VV = OP_FVV | (VMFEQ_FUNCT6 << kRvvFunct6Shift),
  RO_V_VMFEQ_VF = OP_FVF | (VMFEQ_FUNCT6 << kRvvFunct6Shift),

  VMFNE_FUNCT6 = 0b011100,
  RO_V_VMFNE_VV = OP_FVV | (VMFNE_FUNCT6 << kRvvFunct6Shift),
  RO_V_VMFNE_VF = OP_FVF | (VMFNE_FUNCT6 << kRvvFunct6Shift),

  VMFLT_FUNCT6 = 0b011011,
  RO_V_VMFLT_VV = OP_FVV | (VMFLT_FUNCT6 << kRvvFunct6Shift),
  RO_V_VMFLT_VF = OP_FVF | (VMFLT_FUNCT6 << kRvvFunct6Shift),

  VMFLE_FUNCT6 = 0b011001,
  RO_V_VMFLE_VV = OP_FVV | (VMFLE_FUNCT6 << kRvvFunct6Shift),
  RO_V_VMFLE_VF = OP_FVF | (VMFLE_FUNCT6 << kRvvFunct6Shift),

  VMFGE_FUNCT6 = 0b011111,
  RO_V_VMFGE_VF = OP_FVF | (VMFGE_FUNCT6 << kRvvFunct6Shift),

  VMFGT_FUNCT6 = 0b011101,
  RO_V_VMFGT_VF = OP_FVF | (VMFGT_FUNCT6 << kRvvFunct6Shift),

  VFMAX_FUNCT6 = 0b000110,
  RO_V_VFMAX_VV = OP_FVV | (VFMAX_FUNCT6 << kRvvFunct6Shift),
  RO_V_VFMAX_VF = OP_FVF | (VFMAX_FUNCT6 << kRvvFunct6Shift),

  VFREDMAX_FUNCT6 = 0b0001111,
  RO_V_VFREDMAX_VV = OP_FVV | (VFREDMAX_FUNCT6 << kRvvFunct6Shift),

  VFMIN_FUNCT6 = 0b000100,
  RO_V_VFMIN_VV = OP_FVV | (VFMIN_FUNCT6 << kRvvFunct6Shift),
  RO_V_VFMIN_VF = OP_FVF | (VFMIN_FUNCT6 << kRvvFunct6Shift),

  VFSGNJ_FUNCT6 = 0b001000,
  RO_V_VFSGNJ_VV = OP_FVV | (VFSGNJ_FUNCT6 << kRvvFunct6Shift),
  RO_V_VFSGNJ_VF = OP_FVF | (VFSGNJ_FUNCT6 << kRvvFunct6Shift),

  VFSGNJN_FUNCT6 = 0b001001,
  RO_V_VFSGNJN_VV = OP_FVV | (VFSGNJN_FUNCT6 << kRvvFunct6Shift),
  RO_V_VFSGNJN_VF = OP_FVF | (VFSGNJN_FUNCT6 << kRvvFunct6Shift),

  VFSGNJX_FUNCT6 = 0b001010,
  RO_V_VFSGNJX_VV = OP_FVV | (VFSGNJX_FUNCT6 << kRvvFunct6Shift),
  RO_V_VFSGNJX_VF = OP_FVF | (VFSGNJX_FUNCT6 << kRvvFunct6Shift),

  VFMADD_FUNCT6 = 0b101000,
  RO_V_VFMADD_VV = OP_FVV | (VFMADD_FUNCT6 << kRvvFunct6Shift),
  RO_V_VFMADD_VF = OP_FVF | (VFMADD_FUNCT6 << kRvvFunct6Shift),

  VFNMADD_FUNCT6 = 0b101001,
  RO_V_VFNMADD_VV = OP_FVV | (VFNMADD_FUNCT6 << kRvvFunct6Shift),
  RO_V_VFNMADD_VF = OP_FVF | (VFNMADD_FUNCT6 << kRvvFunct6Shift),

  VFMSUB_FUNCT6 = 0b101010,
  RO_V_VFMSUB_VV = OP_FVV | (VFMSUB_FUNCT6 << kRvvFunct6Shift),
  RO_V_VFMSUB_VF = OP_FVF | (VFMSUB_FUNCT6 << kRvvFunct6Shift),

  VFNMSUB_FUNCT6 = 0b101011,
  RO_V_VFNMSUB_VV = OP_FVV | (VFNMSUB_FUNCT6 << kRvvFunct6Shift),
  RO_V_VFNMSUB_VF = OP_FVF | (VFNMSUB_FUNCT6 << kRvvFunct6Shift),

  VFMACC_FUNCT6 = 0b101100,
  RO_V_VFMACC_VV = OP_FVV | (VFMACC_FUNCT6 << kRvvFunct6Shift),
  RO_V_VFMACC_VF = OP_FVF | (VFMACC_FUNCT6 << kRvvFunct6Shift),

  VFNMACC_FUNCT6 = 0b101101,
  RO_V_VFNMACC_VV = OP_FVV | (VFNMACC_FUNCT6 << kRvvFunct6Shift),
  RO_V_VFNMACC_VF = OP_FVF | (VFNMACC_FUNCT6 << kRvvFunct6Shift),

  VFMSAC_FUNCT6 = 0b101110,
  RO_V_VFMSAC_VV = OP_FVV | (VFMSAC_FUNCT6 << kRvvFunct6Shift),
  RO_V_VFMSAC_VF = OP_FVF | (VFMSAC_FUNCT6 << kRvvFunct6Shift),

  VFNMSAC_FUNCT6 = 0b101111,
  RO_V_VFNMSAC_VV = OP_FVV | (VFNMSAC_FUNCT6 << kRvvFunct6Shift),
  RO_V_VFNMSAC_VF = OP_FVF | (VFNMSAC_FUNCT6 << kRvvFunct6Shift),

  // Vector Widening Floating-Point Fused Multiply-Add Instructions
  VFWMACC_FUNCT6 = 0b111100,
  RO_V_VFWMACC_VV = OP_FVV | (VFWMACC_FUNCT6 << kRvvFunct6Shift),
  RO_V_VFWMACC_VF = OP_FVF | (VFWMACC_FUNCT6 << kRvvFunct6Shift),

  VFWNMACC_FUNCT6 = 0b111101,
  RO_V_VFWNMACC_VV = OP_FVV | (VFWNMACC_FUNCT6 << kRvvFunct6Shift),
  RO_V_VFWNMACC_VF = OP_FVF | (VFWNMACC_FUNCT6 << kRvvFunct6Shift),

  VFWMSAC_FUNCT6 = 0b111110,
  RO_V_VFWMSAC_VV = OP_FVV | (VFWMSAC_FUNCT6 << kRvvFunct6Shift),
  RO_V_VFWMSAC_VF = OP_FVF | (VFWMSAC_FUNCT6 << kRvvFunct6Shift),

  VFWNMSAC_FUNCT6 = 0b111111,
  RO_V_VFWNMSAC_VV = OP_FVV | (VFWNMSAC_FUNCT6 << kRvvFunct6Shift),
  RO_V_VFWNMSAC_VF = OP_FVF | (VFWNMSAC_FUNCT6 << kRvvFunct6Shift),

  VNCLIP_FUNCT6 = 0b101111,
  RO_V_VNCLIP_WV = OP_IVV | (VNCLIP_FUNCT6 << kRvvFunct6Shift),
  RO_V_VNCLIP_WX = OP_IVX | (VNCLIP_FUNCT6 << kRvvFunct6Shift),
  RO_V_VNCLIP_WI = OP_IVI | (VNCLIP_FUNCT6 << kRvvFunct6Shift),

  VNCLIPU_FUNCT6 = 0b101110,
  RO_V_VNCLIPU_WV = OP_IVV | (VNCLIPU_FUNCT6 << kRvvFunct6Shift),
  RO_V_VNCLIPU_WX = OP_IVX | (VNCLIPU_FUNCT6 << kRvvFunct6Shift),
  RO_V_VNCLIPU_WI = OP_IVI | (VNCLIPU_FUNCT6 << kRvvFunct6Shift),
};
}  // namespace internal
}  // namespace v8

#endif  // V8_CODEGEN_RISCV_CONSTANT_RISCV_V_H_
