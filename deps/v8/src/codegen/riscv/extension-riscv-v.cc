
// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/codegen/riscv/extension-riscv-v.h"

#include "src/codegen/assembler.h"
#include "src/codegen/riscv/constant-riscv-v.h"
#include "src/codegen/riscv/register-riscv.h"

namespace v8 {
namespace internal {

// RVV

void AssemblerRISCVV::vredmaxu_vs(VRegister vd, VRegister vs2, VRegister vs1,
                                  MaskType mask) {
  GenInstrV(VREDMAXU_FUNCT6, OP_MVV, vd, vs1, vs2, mask);
}

void AssemblerRISCVV::vredmax_vs(VRegister vd, VRegister vs2, VRegister vs1,
                                 MaskType mask) {
  GenInstrV(VREDMAX_FUNCT6, OP_MVV, vd, vs1, vs2, mask);
}

void AssemblerRISCVV::vredmin_vs(VRegister vd, VRegister vs2, VRegister vs1,
                                 MaskType mask) {
  GenInstrV(VREDMIN_FUNCT6, OP_MVV, vd, vs1, vs2, mask);
}

void AssemblerRISCVV::vredminu_vs(VRegister vd, VRegister vs2, VRegister vs1,
                                  MaskType mask) {
  GenInstrV(VREDMINU_FUNCT6, OP_MVV, vd, vs1, vs2, mask);
}

void AssemblerRISCVV::vmv_vv(VRegister vd, VRegister vs1) {
  GenInstrV(VMV_FUNCT6, OP_IVV, vd, vs1, v0, NoMask);
}

void AssemblerRISCVV::vmv_vx(VRegister vd, Register rs1) {
  GenInstrV(VMV_FUNCT6, OP_IVX, vd, rs1, v0, NoMask);
}

void AssemblerRISCVV::vmv_vi(VRegister vd, uint8_t simm5) {
  GenInstrV(VMV_FUNCT6, vd, simm5, v0, NoMask);
}

void AssemblerRISCVV::vmv_xs(Register rd, VRegister vs2) {
  GenInstrV(VWXUNARY0_FUNCT6, OP_MVV, rd, 0b00000, vs2, NoMask);
}

void AssemblerRISCVV::vmv_sx(VRegister vd, Register rs1) {
  GenInstrV(VRXUNARY0_FUNCT6, OP_MVX, vd, rs1, v0, NoMask);
}

void AssemblerRISCVV::vmerge_vv(VRegister vd, VRegister vs1, VRegister vs2) {
  GenInstrV(VMV_FUNCT6, OP_IVV, vd, vs1, vs2, Mask);
}

void AssemblerRISCVV::vmerge_vx(VRegister vd, Register rs1, VRegister vs2) {
  GenInstrV(VMV_FUNCT6, OP_IVX, vd, rs1, vs2, Mask);
}

void AssemblerRISCVV::vmerge_vi(VRegister vd, uint8_t imm5, VRegister vs2) {
  GenInstrV(VMV_FUNCT6, vd, imm5, vs2, Mask);
}

void AssemblerRISCVV::vadc_vv(VRegister vd, VRegister vs1, VRegister vs2) {
  GenInstrV(VADC_FUNCT6, OP_IVV, vd, vs1, vs2, Mask);
}

void AssemblerRISCVV::vadc_vx(VRegister vd, Register rs1, VRegister vs2) {
  GenInstrV(VADC_FUNCT6, OP_IVX, vd, rs1, vs2, Mask);
}

void AssemblerRISCVV::vadc_vi(VRegister vd, uint8_t imm5, VRegister vs2) {
  GenInstrV(VADC_FUNCT6, vd, imm5, vs2, Mask);
}

void AssemblerRISCVV::vmadc_vv(VRegister vd, VRegister vs1, VRegister vs2) {
  GenInstrV(VMADC_FUNCT6, OP_IVV, vd, vs1, vs2, Mask);
}

void AssemblerRISCVV::vmadc_vx(VRegister vd, Register rs1, VRegister vs2) {
  GenInstrV(VMADC_FUNCT6, OP_IVX, vd, rs1, vs2, Mask);
}

void AssemblerRISCVV::vmadc_vi(VRegister vd, uint8_t imm5, VRegister vs2) {
  GenInstrV(VMADC_FUNCT6, vd, imm5, vs2, Mask);
}

void AssemblerRISCVV::vrgather_vv(VRegister vd, VRegister vs2, VRegister vs1,
                                  MaskType mask) {
  DCHECK_NE(vd, vs1);
  DCHECK_NE(vd, vs2);
  GenInstrV(VRGATHER_FUNCT6, OP_IVV, vd, vs1, vs2, mask);
}

void AssemblerRISCVV::vrgather_vi(VRegister vd, VRegister vs2, int8_t imm5,
                                  MaskType mask) {
  DCHECK_NE(vd, vs2);
  GenInstrV(VRGATHER_FUNCT6, vd, imm5, vs2, mask);
}

void AssemblerRISCVV::vrgather_vx(VRegister vd, VRegister vs2, Register rs1,
                                  MaskType mask) {
  DCHECK_NE(vd, vs2);
  GenInstrV(VRGATHER_FUNCT6, OP_IVX, vd, rs1, vs2, mask);
}

void AssemblerRISCVV::vwaddu_wx(VRegister vd, VRegister vs2, Register rs1,
                                MaskType mask) {
  GenInstrV(VWADDUW_FUNCT6, OP_MVX, vd, rs1, vs2, mask);
}

void AssemblerRISCVV::vid_v(VRegister vd, MaskType mask) {
  GenInstrV(VMUNARY0_FUNCT6, OP_MVV, vd, VID_V, v0, mask);
}

#define DEFINE_OPIVV(name, funct6)                                            \
  void AssemblerRISCVV::name##_vv(VRegister vd, VRegister vs2, VRegister vs1, \
                                  MaskType mask) {                            \
    GenInstrV(funct6, OP_IVV, vd, vs1, vs2, mask);                            \
  }

#define DEFINE_OPFVV(name, funct6)                                            \
  void AssemblerRISCVV::name##_vv(VRegister vd, VRegister vs2, VRegister vs1, \
                                  MaskType mask) {                            \
    GenInstrV(funct6, OP_FVV, vd, vs1, vs2, mask);                            \
  }

#define DEFINE_OPFWV(name, funct6)                                            \
  void AssemblerRISCVV::name##_wv(VRegister vd, VRegister vs2, VRegister vs1, \
                                  MaskType mask) {                            \
    GenInstrV(funct6, OP_FVV, vd, vs1, vs2, mask);                            \
  }

#define DEFINE_OPFRED(name, funct6)                                           \
  void AssemblerRISCVV::name##_vs(VRegister vd, VRegister vs2, VRegister vs1, \
                                  MaskType mask) {                            \
    GenInstrV(funct6, OP_FVV, vd, vs1, vs2, mask);                            \
  }

#define DEFINE_OPIVX(name, funct6)                                           \
  void AssemblerRISCVV::name##_vx(VRegister vd, VRegister vs2, Register rs1, \
                                  MaskType mask) {                           \
    GenInstrV(funct6, OP_IVX, vd, rs1, vs2, mask);                           \
  }

#define DEFINE_OPIVI(name, funct6)                                          \
  void AssemblerRISCVV::name##_vi(VRegister vd, VRegister vs2, int8_t imm5, \
                                  MaskType mask) {                          \
    GenInstrV(funct6, vd, imm5, vs2, mask);                                 \
  }

#define DEFINE_OPMVV(name, funct6)                                            \
  void AssemblerRISCVV::name##_vv(VRegister vd, VRegister vs2, VRegister vs1, \
                                  MaskType mask) {                            \
    GenInstrV(funct6, OP_MVV, vd, vs1, vs2, mask);                            \
  }

// void GenInstrV(uint8_t funct6, Opcode opcode, VRegister vd, Register
// rs1,
//                  VRegister vs2, MaskType mask = NoMask);
#define DEFINE_OPMVX(name, funct6)                                           \
  void AssemblerRISCVV::name##_vx(VRegister vd, VRegister vs2, Register rs1, \
                                  MaskType mask) {                           \
    GenInstrV(funct6, OP_MVX, vd, rs1, vs2, mask);                           \
  }

#define DEFINE_OPFVF(name, funct6)                                  \
  void AssemblerRISCVV::name##_vf(VRegister vd, VRegister vs2,      \
                                  FPURegister fs1, MaskType mask) { \
    GenInstrV(funct6, OP_FVF, vd, fs1, vs2, mask);                  \
  }

#define DEFINE_OPFWF(name, funct6)                                  \
  void AssemblerRISCVV::name##_wf(VRegister vd, VRegister vs2,      \
                                  FPURegister fs1, MaskType mask) { \
    GenInstrV(funct6, OP_FVF, vd, fs1, vs2, mask);                  \
  }

#define DEFINE_OPFVV_FMA(name, funct6)                                        \
  void AssemblerRISCVV::name##_vv(VRegister vd, VRegister vs1, VRegister vs2, \
                                  MaskType mask) {                            \
    GenInstrV(funct6, OP_FVV, vd, vs1, vs2, mask);                            \
  }

#define DEFINE_OPFVF_FMA(name, funct6)                            \
  void AssemblerRISCVV::name##_vf(VRegister vd, FPURegister fs1,  \
                                  VRegister vs2, MaskType mask) { \
    GenInstrV(funct6, OP_FVF, vd, fs1, vs2, mask);                \
  }

// vector integer extension
#define DEFINE_OPMVV_VIE(name, vs1)                                        \
  void AssemblerRISCVV::name(VRegister vd, VRegister vs2, MaskType mask) { \
    GenInstrV(VXUNARY0_FUNCT6, OP_MVV, vd, vs1, vs2, mask);                \
  }

void AssemblerRISCVV::vfmv_vf(VRegister vd, FPURegister fs1) {
  GenInstrV(VMV_FUNCT6, OP_FVF, vd, fs1, v0, NoMask);
}

void AssemblerRISCVV::vfmv_fs(FPURegister fd, VRegister vs2) {
  GenInstrV(VWFUNARY0_FUNCT6, OP_FVV, fd, v0, vs2, NoMask);
}

void AssemblerRISCVV::vfmv_sf(VRegister vd, FPURegister fs) {
  GenInstrV(VRFUNARY0_FUNCT6, OP_FVF, vd, fs, v0, NoMask);
}

void AssemblerRISCVV::vfmerge_vf(VRegister vd, FPURegister fs1, VRegister vs2) {
  GenInstrV(VMV_FUNCT6, OP_FVF, vd, fs1, vs2, Mask);
}

DEFINE_OPIVV(vadd, VADD_FUNCT6)
DEFINE_OPIVX(vadd, VADD_FUNCT6)
DEFINE_OPIVI(vadd, VADD_FUNCT6)
DEFINE_OPIVV(vsub, VSUB_FUNCT6)
DEFINE_OPIVX(vsub, VSUB_FUNCT6)
DEFINE_OPMVX(vdiv, VDIV_FUNCT6)
DEFINE_OPMVX(vdivu, VDIVU_FUNCT6)
DEFINE_OPMVX(vmul, VMUL_FUNCT6)
DEFINE_OPMVX(vmulhu, VMULHU_FUNCT6)
DEFINE_OPMVX(vmulhsu, VMULHSU_FUNCT6)
DEFINE_OPMVX(vmulh, VMULH_FUNCT6)
DEFINE_OPMVV(vdiv, VDIV_FUNCT6)
DEFINE_OPMVV(vdivu, VDIVU_FUNCT6)
DEFINE_OPMVV(vmul, VMUL_FUNCT6)
DEFINE_OPMVV(vmulhu, VMULHU_FUNCT6)
DEFINE_OPMVV(vmulhsu, VMULHSU_FUNCT6)
DEFINE_OPMVV(vwmul, VWMUL_FUNCT6)
DEFINE_OPMVV(vwmulu, VWMULU_FUNCT6)
DEFINE_OPMVV(vmulh, VMULH_FUNCT6)
DEFINE_OPMVV(vwadd, VWADD_FUNCT6)
DEFINE_OPMVV(vwaddu, VWADDU_FUNCT6)
DEFINE_OPMVV(vcompress, VCOMPRESS_FUNCT6)
DEFINE_OPIVX(vsadd, VSADD_FUNCT6)
DEFINE_OPIVV(vsadd, VSADD_FUNCT6)
DEFINE_OPIVI(vsadd, VSADD_FUNCT6)
DEFINE_OPIVX(vsaddu, VSADDU_FUNCT6)
DEFINE_OPIVV(vsaddu, VSADDU_FUNCT6)
DEFINE_OPIVI(vsaddu, VSADDU_FUNCT6)
DEFINE_OPIVX(vssub, VSSUB_FUNCT6)
DEFINE_OPIVV(vssub, VSSUB_FUNCT6)
DEFINE_OPIVX(vssubu, VSSUBU_FUNCT6)
DEFINE_OPIVV(vssubu, VSSUBU_FUNCT6)
DEFINE_OPIVX(vrsub, VRSUB_FUNCT6)
DEFINE_OPIVI(vrsub, VRSUB_FUNCT6)
DEFINE_OPIVV(vminu, VMINU_FUNCT6)
DEFINE_OPIVX(vminu, VMINU_FUNCT6)
DEFINE_OPIVV(vmin, VMIN_FUNCT6)
DEFINE_OPIVX(vmin, VMIN_FUNCT6)
DEFINE_OPIVV(vmaxu, VMAXU_FUNCT6)
DEFINE_OPIVX(vmaxu, VMAXU_FUNCT6)
DEFINE_OPIVV(vmax, VMAX_FUNCT6)
DEFINE_OPIVX(vmax, VMAX_FUNCT6)
DEFINE_OPIVV(vand, VAND_FUNCT6)
DEFINE_OPIVX(vand, VAND_FUNCT6)
DEFINE_OPIVI(vand, VAND_FUNCT6)
DEFINE_OPIVV(vor, VOR_FUNCT6)
DEFINE_OPIVX(vor, VOR_FUNCT6)
DEFINE_OPIVI(vor, VOR_FUNCT6)
DEFINE_OPIVV(vxor, VXOR_FUNCT6)
DEFINE_OPIVX(vxor, VXOR_FUNCT6)
DEFINE_OPIVI(vxor, VXOR_FUNCT6)

DEFINE_OPIVX(vslidedown, VSLIDEDOWN_FUNCT6)
DEFINE_OPIVI(vslidedown, VSLIDEDOWN_FUNCT6)
DEFINE_OPMVX(vslide1down, VSLIDEDOWN_FUNCT6)
DEFINE_OPFVF(vfslide1down, VSLIDEDOWN_FUNCT6)
DEFINE_OPIVX(vslideup, VSLIDEUP_FUNCT6)
DEFINE_OPIVI(vslideup, VSLIDEUP_FUNCT6)
DEFINE_OPMVX(vslide1up, VSLIDEUP_FUNCT6)
DEFINE_OPFVF(vfslide1up, VSLIDEUP_FUNCT6)

DEFINE_OPIVV(vmseq, VMSEQ_FUNCT6)
DEFINE_OPIVX(vmseq, VMSEQ_FUNCT6)
DEFINE_OPIVI(vmseq, VMSEQ_FUNCT6)

DEFINE_OPIVV(vmsne, VMSNE_FUNCT6)
DEFINE_OPIVX(vmsne, VMSNE_FUNCT6)
DEFINE_OPIVI(vmsne, VMSNE_FUNCT6)

DEFINE_OPIVV(vmsltu, VMSLTU_FUNCT6)
DEFINE_OPIVX(vmsltu, VMSLTU_FUNCT6)

DEFINE_OPIVV(vmslt, VMSLT_FUNCT6)
DEFINE_OPIVX(vmslt, VMSLT_FUNCT6)

DEFINE_OPIVV(vmsle, VMSLE_FUNCT6)
DEFINE_OPIVX(vmsle, VMSLE_FUNCT6)
DEFINE_OPIVI(vmsle, VMSLE_FUNCT6)

DEFINE_OPIVV(vmsleu, VMSLEU_FUNCT6)
DEFINE_OPIVX(vmsleu, VMSLEU_FUNCT6)
DEFINE_OPIVI(vmsleu, VMSLEU_FUNCT6)

DEFINE_OPIVI(vmsgt, VMSGT_FUNCT6)
DEFINE_OPIVX(vmsgt, VMSGT_FUNCT6)

DEFINE_OPIVI(vmsgtu, VMSGTU_FUNCT6)
DEFINE_OPIVX(vmsgtu, VMSGTU_FUNCT6)

DEFINE_OPIVV(vsrl, VSRL_FUNCT6)
DEFINE_OPIVX(vsrl, VSRL_FUNCT6)
DEFINE_OPIVI(vsrl, VSRL_FUNCT6)

DEFINE_OPIVV(vsra, VSRA_FUNCT6)
DEFINE_OPIVX(vsra, VSRA_FUNCT6)
DEFINE_OPIVI(vsra, VSRA_FUNCT6)

DEFINE_OPIVV(vsll, VSLL_FUNCT6)
DEFINE_OPIVX(vsll, VSLL_FUNCT6)
DEFINE_OPIVI(vsll, VSLL_FUNCT6)

DEFINE_OPIVV(vsmul, VSMUL_FUNCT6)
DEFINE_OPIVX(vsmul, VSMUL_FUNCT6)

DEFINE_OPFVV(vfadd, VFADD_FUNCT6)
DEFINE_OPFVF(vfadd, VFADD_FUNCT6)
DEFINE_OPFVV(vfsub, VFSUB_FUNCT6)
DEFINE_OPFVF(vfsub, VFSUB_FUNCT6)
DEFINE_OPFVV(vfdiv, VFDIV_FUNCT6)
DEFINE_OPFVF(vfdiv, VFDIV_FUNCT6)
DEFINE_OPFVV(vfmul, VFMUL_FUNCT6)
DEFINE_OPFVF(vfmul, VFMUL_FUNCT6)
DEFINE_OPFVV(vmfeq, VMFEQ_FUNCT6)
DEFINE_OPFVV(vmfne, VMFNE_FUNCT6)
DEFINE_OPFVV(vmflt, VMFLT_FUNCT6)
DEFINE_OPFVV(vmfle, VMFLE_FUNCT6)
DEFINE_OPFVV(vfmax, VFMAX_FUNCT6)
DEFINE_OPFVV(vfmin, VFMIN_FUNCT6)

// Vector Widening Floating-Point Add/Subtract Instructions
DEFINE_OPFVV(vfwadd, VFWADD_FUNCT6)
DEFINE_OPFVF(vfwadd, VFWADD_FUNCT6)
DEFINE_OPFVV(vfwsub, VFWSUB_FUNCT6)
DEFINE_OPFVF(vfwsub, VFWSUB_FUNCT6)
DEFINE_OPFWV(vfwadd, VFWADD_W_FUNCT6)
DEFINE_OPFWF(vfwadd, VFWADD_W_FUNCT6)
DEFINE_OPFWV(vfwsub, VFWSUB_W_FUNCT6)
DEFINE_OPFWF(vfwsub, VFWSUB_W_FUNCT6)

// Vector Widening Floating-Point Reduction Instructions
DEFINE_OPFRED(vfwredusum, VFWREDUSUM_FUNCT6)
DEFINE_OPFRED(vfwredosum, VFWREDOSUM_FUNCT6)

// Vector Widening Floating-Point Multiply
DEFINE_OPFVV(vfwmul, VFWMUL_FUNCT6)
DEFINE_OPFVF(vfwmul, VFWMUL_FUNCT6)

DEFINE_OPFRED(vfredmax, VFREDMAX_FUNCT6)

DEFINE_OPFVV(vfsngj, VFSGNJ_FUNCT6)
DEFINE_OPFVF(vfsngj, VFSGNJ_FUNCT6)
DEFINE_OPFVV(vfsngjn, VFSGNJN_FUNCT6)
DEFINE_OPFVF(vfsngjn, VFSGNJN_FUNCT6)
DEFINE_OPFVV(vfsngjx, VFSGNJX_FUNCT6)
DEFINE_OPFVF(vfsngjx, VFSGNJX_FUNCT6)

// Vector Single-Width Floating-Point Fused Multiply-Add Instructions
DEFINE_OPFVV_FMA(vfmadd, VFMADD_FUNCT6)
DEFINE_OPFVF_FMA(vfmadd, VFMADD_FUNCT6)
DEFINE_OPFVV_FMA(vfmsub, VFMSUB_FUNCT6)
DEFINE_OPFVF_FMA(vfmsub, VFMSUB_FUNCT6)
DEFINE_OPFVV_FMA(vfmacc, VFMACC_FUNCT6)
DEFINE_OPFVF_FMA(vfmacc, VFMACC_FUNCT6)
DEFINE_OPFVV_FMA(vfmsac, VFMSAC_FUNCT6)
DEFINE_OPFVF_FMA(vfmsac, VFMSAC_FUNCT6)
DEFINE_OPFVV_FMA(vfnmadd, VFNMADD_FUNCT6)
DEFINE_OPFVF_FMA(vfnmadd, VFNMADD_FUNCT6)
DEFINE_OPFVV_FMA(vfnmsub, VFNMSUB_FUNCT6)
DEFINE_OPFVF_FMA(vfnmsub, VFNMSUB_FUNCT6)
DEFINE_OPFVV_FMA(vfnmacc, VFNMACC_FUNCT6)
DEFINE_OPFVF_FMA(vfnmacc, VFNMACC_FUNCT6)
DEFINE_OPFVV_FMA(vfnmsac, VFNMSAC_FUNCT6)
DEFINE_OPFVF_FMA(vfnmsac, VFNMSAC_FUNCT6)

// Vector Widening Floating-Point Fused Multiply-Add Instructions
DEFINE_OPFVV_FMA(vfwmacc, VFWMACC_FUNCT6)
DEFINE_OPFVF_FMA(vfwmacc, VFWMACC_FUNCT6)
DEFINE_OPFVV_FMA(vfwnmacc, VFWNMACC_FUNCT6)
DEFINE_OPFVF_FMA(vfwnmacc, VFWNMACC_FUNCT6)
DEFINE_OPFVV_FMA(vfwmsac, VFWMSAC_FUNCT6)
DEFINE_OPFVF_FMA(vfwmsac, VFWMSAC_FUNCT6)
DEFINE_OPFVV_FMA(vfwnmsac, VFWNMSAC_FUNCT6)
DEFINE_OPFVF_FMA(vfwnmsac, VFWNMSAC_FUNCT6)

// Vector Narrowing Fixed-Point Clip Instructions
DEFINE_OPIVV(vnclip, VNCLIP_FUNCT6)
DEFINE_OPIVX(vnclip, VNCLIP_FUNCT6)
DEFINE_OPIVI(vnclip, VNCLIP_FUNCT6)
DEFINE_OPIVV(vnclipu, VNCLIPU_FUNCT6)
DEFINE_OPIVX(vnclipu, VNCLIPU_FUNCT6)
DEFINE_OPIVI(vnclipu, VNCLIPU_FUNCT6)

// Vector Integer Extension
DEFINE_OPMVV_VIE(vzext_vf8, 0b00010)
DEFINE_OPMVV_VIE(vsext_vf8, 0b00011)
DEFINE_OPMVV_VIE(vzext_vf4, 0b00100)
DEFINE_OPMVV_VIE(vsext_vf4, 0b00101)
DEFINE_OPMVV_VIE(vzext_vf2, 0b00110)
DEFINE_OPMVV_VIE(vsext_vf2, 0b00111)

#undef DEFINE_OPIVI
#undef DEFINE_OPIVV
#undef DEFINE_OPIVX
#undef DEFINE_OPFVV
#undef DEFINE_OPFWV
#undef DEFINE_OPFVF
#undef DEFINE_OPFWF
#undef DEFINE_OPFVV_FMA
#undef DEFINE_OPFVF_FMA
#undef DEFINE_OPMVV_VIE

void AssemblerRISCVV::vsetvli(Register rd, Register rs1, VSew vsew, Vlmul vlmul,
                              TailAgnosticType tail, MaskAgnosticType mask) {
  int32_t zimm = GenZimm(vsew, vlmul, tail, mask);
  Instr instr = OP_V | ((rd.code() & 0x1F) << kRvvRdShift) | (0x7 << 12) |
                ((rs1.code() & 0x1F) << kRvvRs1Shift) |
                (((uint32_t)zimm << kRvvZimmShift) & kRvvZimmMask) | 0x0 << 31;
  emit(instr);
}

void AssemblerRISCVV::vsetivli(Register rd, uint8_t uimm, VSew vsew,
                               Vlmul vlmul, TailAgnosticType tail,
                               MaskAgnosticType mask) {
  DCHECK(is_uint5(uimm));
  int32_t zimm = GenZimm(vsew, vlmul, tail, mask) & 0x3FF;
  Instr instr = OP_V | ((rd.code() & 0x1F) << kRvvRdShift) | (0x7 << 12) |
                ((uimm & 0x1F) << kRvvUimmShift) |
                (((uint32_t)zimm << kRvvZimmShift) & kRvvZimmMask) | 0x3 << 30;
  emit(instr);
}

void AssemblerRISCVV::vsetvl(Register rd, Register rs1, Register rs2) {
  Instr instr = OP_V | ((rd.code() & 0x1F) << kRvvRdShift) | (0x7 << 12) |
                ((rs1.code() & 0x1F) << kRvvRs1Shift) |
                ((rs2.code() & 0x1F) << kRvvRs2Shift) | 0x40 << 25;
  emit(instr);
}

uint8_t vsew_switch(VSew vsew) {
  uint8_t width;
  switch (vsew) {
    case E8:
      width = 0b000;
      break;
    case E16:
      width = 0b101;
      break;
    case E32:
      width = 0b110;
      break;
    default:
      width = 0b111;
      break;
  }
  return width;
}

// OPIVV OPFVV OPMVV
void AssemblerRISCVV::GenInstrV(uint8_t funct6, Opcode opcode, VRegister vd,
                                VRegister vs1, VRegister vs2, MaskType mask) {
  DCHECK(opcode == OP_MVV || opcode == OP_FVV || opcode == OP_IVV);
  Instr instr = (funct6 << kRvvFunct6Shift) | opcode | (mask << kRvvVmShift) |
                ((vd.code() & 0x1F) << kRvvVdShift) |
                ((vs1.code() & 0x1F) << kRvvVs1Shift) |
                ((vs2.code() & 0x1F) << kRvvVs2Shift);
  emit(instr);
}

void AssemblerRISCVV::GenInstrV(uint8_t funct6, Opcode opcode, VRegister vd,
                                int8_t vs1, VRegister vs2, MaskType mask) {
  DCHECK(opcode == OP_MVV || opcode == OP_FVV || opcode == OP_IVV);
  Instr instr = (funct6 << kRvvFunct6Shift) | opcode | (mask << kRvvVmShift) |
                ((vd.code() & 0x1F) << kRvvVdShift) |
                ((vs1 & 0x1F) << kRvvVs1Shift) |
                ((vs2.code() & 0x1F) << kRvvVs2Shift);
  emit(instr);
}
// OPMVV OPFVV
void AssemblerRISCVV::GenInstrV(uint8_t funct6, Opcode opcode, Register rd,
                                VRegister vs1, VRegister vs2, MaskType mask) {
  DCHECK(opcode == OP_MVV || opcode == OP_FVV);
  Instr instr = (funct6 << kRvvFunct6Shift) | opcode | (mask << kRvvVmShift) |
                ((rd.code() & 0x1F) << kRvvVdShift) |
                ((vs1.code() & 0x1F) << kRvvVs1Shift) |
                ((vs2.code() & 0x1F) << kRvvVs2Shift);
  emit(instr);
}

// OPFVV
void AssemblerRISCVV::GenInstrV(uint8_t funct6, Opcode opcode, FPURegister fd,
                                VRegister vs1, VRegister vs2, MaskType mask) {
  DCHECK(opcode == OP_FVV);
  Instr instr = (funct6 << kRvvFunct6Shift) | opcode | (mask << kRvvVmShift) |
                ((fd.code() & 0x1F) << kRvvVdShift) |
                ((vs1.code() & 0x1F) << kRvvVs1Shift) |
                ((vs2.code() & 0x1F) << kRvvVs2Shift);
  emit(instr);
}

// OPIVX OPMVX
void AssemblerRISCVV::GenInstrV(uint8_t funct6, Opcode opcode, VRegister vd,
                                Register rs1, VRegister vs2, MaskType mask) {
  DCHECK(opcode == OP_IVX || opcode == OP_MVX);
  Instr instr = (funct6 << kRvvFunct6Shift) | opcode | (mask << kRvvVmShift) |
                ((vd.code() & 0x1F) << kRvvVdShift) |
                ((rs1.code() & 0x1F) << kRvvRs1Shift) |
                ((vs2.code() & 0x1F) << kRvvVs2Shift);
  emit(instr);
}

// OPFVF
void AssemblerRISCVV::GenInstrV(uint8_t funct6, Opcode opcode, VRegister vd,
                                FPURegister fs1, VRegister vs2, MaskType mask) {
  DCHECK(opcode == OP_FVF);
  Instr instr = (funct6 << kRvvFunct6Shift) | opcode | (mask << kRvvVmShift) |
                ((vd.code() & 0x1F) << kRvvVdShift) |
                ((fs1.code() & 0x1F) << kRvvRs1Shift) |
                ((vs2.code() & 0x1F) << kRvvVs2Shift);
  emit(instr);
}

// OPMVX
void AssemblerRISCVV::GenInstrV(uint8_t funct6, Register rd, Register rs1,
                                VRegister vs2, MaskType mask) {
  Instr instr = (funct6 << kRvvFunct6Shift) | OP_MVX | (mask << kRvvVmShift) |
                ((rd.code() & 0x1F) << kRvvVdShift) |
                ((rs1.code() & 0x1F) << kRvvRs1Shift) |
                ((vs2.code() & 0x1F) << kRvvVs2Shift);
  emit(instr);
}
// OPIVI
void AssemblerRISCVV::GenInstrV(uint8_t funct6, VRegister vd, int8_t imm5,
                                VRegister vs2, MaskType mask) {
  DCHECK(is_uint5(imm5) || is_int5(imm5));
  Instr instr = (funct6 << kRvvFunct6Shift) | OP_IVI | (mask << kRvvVmShift) |
                ((vd.code() & 0x1F) << kRvvVdShift) |
                (((uint32_t)imm5 << kRvvImm5Shift) & kRvvImm5Mask) |
                ((vs2.code() & 0x1F) << kRvvVs2Shift);
  emit(instr);
}

// VL VS
void AssemblerRISCVV::GenInstrV(BaseOpcode opcode, uint8_t width, VRegister vd,
                                Register rs1, uint8_t umop, MaskType mask,
                                uint8_t IsMop, bool IsMew, uint8_t Nf) {
  DCHECK(opcode == LOAD_FP || opcode == STORE_FP);
  Instr instr = opcode | ((vd.code() << kRvvVdShift) & kRvvVdMask) |
                ((width << kRvvWidthShift) & kRvvWidthMask) |
                ((rs1.code() << kRvvRs1Shift) & kRvvRs1Mask) |
                ((umop << kRvvRs2Shift) & kRvvRs2Mask) |
                ((mask << kRvvVmShift) & kRvvVmMask) |
                ((IsMop << kRvvMopShift) & kRvvMopMask) |
                ((IsMew << kRvvMewShift) & kRvvMewMask) |
                ((Nf << kRvvNfShift) & kRvvNfMask);
  emit(instr);
}
void AssemblerRISCVV::GenInstrV(BaseOpcode opcode, uint8_t width, VRegister vd,
                                Register rs1, Register rs2, MaskType mask,
                                uint8_t IsMop, bool IsMew, uint8_t Nf) {
  DCHECK(opcode == LOAD_FP || opcode == STORE_FP);
  Instr instr = opcode | ((vd.code() << kRvvVdShift) & kRvvVdMask) |
                ((width << kRvvWidthShift) & kRvvWidthMask) |
                ((rs1.code() << kRvvRs1Shift) & kRvvRs1Mask) |
                ((rs2.code() << kRvvRs2Shift) & kRvvRs2Mask) |
                ((mask << kRvvVmShift) & kRvvVmMask) |
                ((IsMop << kRvvMopShift) & kRvvMopMask) |
                ((IsMew << kRvvMewShift) & kRvvMewMask) |
                ((Nf << kRvvNfShift) & kRvvNfMask);
  emit(instr);
}
// VL VS AMO
void AssemblerRISCVV::GenInstrV(BaseOpcode opcode, uint8_t width, VRegister vd,
                                Register rs1, VRegister vs2, MaskType mask,
                                uint8_t IsMop, bool IsMew, uint8_t Nf) {
  DCHECK(opcode == LOAD_FP || opcode == STORE_FP || opcode == AMO);
  Instr instr = opcode | ((vd.code() << kRvvVdShift) & kRvvVdMask) |
                ((width << kRvvWidthShift) & kRvvWidthMask) |
                ((rs1.code() << kRvvRs1Shift) & kRvvRs1Mask) |
                ((vs2.code() << kRvvRs2Shift) & kRvvRs2Mask) |
                ((mask << kRvvVmShift) & kRvvVmMask) |
                ((IsMop << kRvvMopShift) & kRvvMopMask) |
                ((IsMew << kRvvMewShift) & kRvvMewMask) |
                ((Nf << kRvvNfShift) & kRvvNfMask);
  emit(instr);
}
// vmv_xs vcpop_m vfirst_m
void AssemblerRISCVV::GenInstrV(uint8_t funct6, Opcode opcode, Register rd,
                                uint8_t vs1, VRegister vs2, MaskType mask) {
  DCHECK(opcode == OP_MVV);
  Instr instr = (funct6 << kRvvFunct6Shift) | opcode | (mask << kRvvVmShift) |
                ((rd.code() & 0x1F) << kRvvVdShift) |
                ((vs1 & 0x1F) << kRvvVs1Shift) |
                ((vs2.code() & 0x1F) << kRvvVs2Shift);
  emit(instr);
}

void AssemblerRISCVV::vl(VRegister vd, Register rs1, uint8_t lumop, VSew vsew,
                         MaskType mask) {
  uint8_t width = vsew_switch(vsew);
  GenInstrV(LOAD_FP, width, vd, rs1, lumop, mask, 0b00, 0, 0b000);
}
void AssemblerRISCVV::vls(VRegister vd, Register rs1, Register rs2, VSew vsew,
                          MaskType mask) {
  uint8_t width = vsew_switch(vsew);
  GenInstrV(LOAD_FP, width, vd, rs1, rs2, mask, 0b10, 0, 0b000);
}
void AssemblerRISCVV::vlx(VRegister vd, Register rs1, VRegister vs2, VSew vsew,
                          MaskType mask) {
  uint8_t width = vsew_switch(vsew);
  GenInstrV(LOAD_FP, width, vd, rs1, vs2, mask, 0b11, 0, 0);
}

void AssemblerRISCVV::vs(VRegister vd, Register rs1, uint8_t sumop, VSew vsew,
                         MaskType mask) {
  uint8_t width = vsew_switch(vsew);
  GenInstrV(STORE_FP, width, vd, rs1, sumop, mask, 0b00, 0, 0b000);
}
void AssemblerRISCVV::vss(VRegister vs3, Register rs1, Register rs2, VSew vsew,
                          MaskType mask) {
  uint8_t width = vsew_switch(vsew);
  GenInstrV(STORE_FP, width, vs3, rs1, rs2, mask, 0b10, 0, 0b000);
}

void AssemblerRISCVV::vsx(VRegister vd, Register rs1, VRegister vs2, VSew vsew,
                          MaskType mask) {
  uint8_t width = vsew_switch(vsew);
  GenInstrV(STORE_FP, width, vd, rs1, vs2, mask, 0b11, 0, 0b000);
}
void AssemblerRISCVV::vsu(VRegister vd, Register rs1, VRegister vs2, VSew vsew,
                          MaskType mask) {
  uint8_t width = vsew_switch(vsew);
  GenInstrV(STORE_FP, width, vd, rs1, vs2, mask, 0b01, 0, 0b000);
}

void AssemblerRISCVV::vlseg2(VRegister vd, Register rs1, uint8_t lumop,
                             VSew vsew, MaskType mask) {
  uint8_t width = vsew_switch(vsew);
  GenInstrV(LOAD_FP, width, vd, rs1, lumop, mask, 0b00, 0, 0b001);
}

void AssemblerRISCVV::vlseg3(VRegister vd, Register rs1, uint8_t lumop,
                             VSew vsew, MaskType mask) {
  uint8_t width = vsew_switch(vsew);
  GenInstrV(LOAD_FP, width, vd, rs1, lumop, mask, 0b00, 0, 0b010);
}

void AssemblerRISCVV::vlseg4(VRegister vd, Register rs1, uint8_t lumop,
                             VSew vsew, MaskType mask) {
  uint8_t width = vsew_switch(vsew);
  GenInstrV(LOAD_FP, width, vd, rs1, lumop, mask, 0b00, 0, 0b011);
}

void AssemblerRISCVV::vlseg5(VRegister vd, Register rs1, uint8_t lumop,
                             VSew vsew, MaskType mask) {
  uint8_t width = vsew_switch(vsew);
  GenInstrV(LOAD_FP, width, vd, rs1, lumop, mask, 0b00, 0, 0b100);
}

void AssemblerRISCVV::vlseg6(VRegister vd, Register rs1, uint8_t lumop,
                             VSew vsew, MaskType mask) {
  uint8_t width = vsew_switch(vsew);
  GenInstrV(LOAD_FP, width, vd, rs1, lumop, mask, 0b00, 0, 0b101);
}

void AssemblerRISCVV::vlseg7(VRegister vd, Register rs1, uint8_t lumop,
                             VSew vsew, MaskType mask) {
  uint8_t width = vsew_switch(vsew);
  GenInstrV(LOAD_FP, width, vd, rs1, lumop, mask, 0b00, 0, 0b110);
}

void AssemblerRISCVV::vlseg8(VRegister vd, Register rs1, uint8_t lumop,
                             VSew vsew, MaskType mask) {
  uint8_t width = vsew_switch(vsew);
  GenInstrV(LOAD_FP, width, vd, rs1, lumop, mask, 0b00, 0, 0b111);
}
void AssemblerRISCVV::vsseg2(VRegister vd, Register rs1, uint8_t sumop,
                             VSew vsew, MaskType mask) {
  uint8_t width = vsew_switch(vsew);
  GenInstrV(STORE_FP, width, vd, rs1, sumop, mask, 0b00, 0, 0b001);
}
void AssemblerRISCVV::vsseg3(VRegister vd, Register rs1, uint8_t sumop,
                             VSew vsew, MaskType mask) {
  uint8_t width = vsew_switch(vsew);
  GenInstrV(STORE_FP, width, vd, rs1, sumop, mask, 0b00, 0, 0b010);
}
void AssemblerRISCVV::vsseg4(VRegister vd, Register rs1, uint8_t sumop,
                             VSew vsew, MaskType mask) {
  uint8_t width = vsew_switch(vsew);
  GenInstrV(STORE_FP, width, vd, rs1, sumop, mask, 0b00, 0, 0b011);
}
void AssemblerRISCVV::vsseg5(VRegister vd, Register rs1, uint8_t sumop,
                             VSew vsew, MaskType mask) {
  uint8_t width = vsew_switch(vsew);
  GenInstrV(STORE_FP, width, vd, rs1, sumop, mask, 0b00, 0, 0b100);
}
void AssemblerRISCVV::vsseg6(VRegister vd, Register rs1, uint8_t sumop,
                             VSew vsew, MaskType mask) {
  uint8_t width = vsew_switch(vsew);
  GenInstrV(STORE_FP, width, vd, rs1, sumop, mask, 0b00, 0, 0b101);
}
void AssemblerRISCVV::vsseg7(VRegister vd, Register rs1, uint8_t sumop,
                             VSew vsew, MaskType mask) {
  uint8_t width = vsew_switch(vsew);
  GenInstrV(STORE_FP, width, vd, rs1, sumop, mask, 0b00, 0, 0b110);
}
void AssemblerRISCVV::vsseg8(VRegister vd, Register rs1, uint8_t sumop,
                             VSew vsew, MaskType mask) {
  uint8_t width = vsew_switch(vsew);
  GenInstrV(STORE_FP, width, vd, rs1, sumop, mask, 0b00, 0, 0b111);
}

void AssemblerRISCVV::vlsseg2(VRegister vd, Register rs1, Register rs2,
                              VSew vsew, MaskType mask) {
  uint8_t width = vsew_switch(vsew);
  GenInstrV(LOAD_FP, width, vd, rs1, rs2, mask, 0b10, 0, 0b001);
}
void AssemblerRISCVV::vlsseg3(VRegister vd, Register rs1, Register rs2,
                              VSew vsew, MaskType mask) {
  uint8_t width = vsew_switch(vsew);
  GenInstrV(LOAD_FP, width, vd, rs1, rs2, mask, 0b10, 0, 0b010);
}
void AssemblerRISCVV::vlsseg4(VRegister vd, Register rs1, Register rs2,
                              VSew vsew, MaskType mask) {
  uint8_t width = vsew_switch(vsew);
  GenInstrV(LOAD_FP, width, vd, rs1, rs2, mask, 0b10, 0, 0b011);
}
void AssemblerRISCVV::vlsseg5(VRegister vd, Register rs1, Register rs2,
                              VSew vsew, MaskType mask) {
  uint8_t width = vsew_switch(vsew);
  GenInstrV(LOAD_FP, width, vd, rs1, rs2, mask, 0b10, 0, 0b100);
}
void AssemblerRISCVV::vlsseg6(VRegister vd, Register rs1, Register rs2,
                              VSew vsew, MaskType mask) {
  uint8_t width = vsew_switch(vsew);
  GenInstrV(LOAD_FP, width, vd, rs1, rs2, mask, 0b10, 0, 0b101);
}
void AssemblerRISCVV::vlsseg7(VRegister vd, Register rs1, Register rs2,
                              VSew vsew, MaskType mask) {
  uint8_t width = vsew_switch(vsew);
  GenInstrV(LOAD_FP, width, vd, rs1, rs2, mask, 0b10, 0, 0b110);
}
void AssemblerRISCVV::vlsseg8(VRegister vd, Register rs1, Register rs2,
                              VSew vsew, MaskType mask) {
  uint8_t width = vsew_switch(vsew);
  GenInstrV(LOAD_FP, width, vd, rs1, rs2, mask, 0b10, 0, 0b111);
}
void AssemblerRISCVV::vssseg2(VRegister vd, Register rs1, Register rs2,
                              VSew vsew, MaskType mask) {
  uint8_t width = vsew_switch(vsew);
  GenInstrV(STORE_FP, width, vd, rs1, rs2, mask, 0b10, 0, 0b001);
}
void AssemblerRISCVV::vssseg3(VRegister vd, Register rs1, Register rs2,
                              VSew vsew, MaskType mask) {
  uint8_t width = vsew_switch(vsew);
  GenInstrV(STORE_FP, width, vd, rs1, rs2, mask, 0b10, 0, 0b010);
}
void AssemblerRISCVV::vssseg4(VRegister vd, Register rs1, Register rs2,
                              VSew vsew, MaskType mask) {
  uint8_t width = vsew_switch(vsew);
  GenInstrV(STORE_FP, width, vd, rs1, rs2, mask, 0b10, 0, 0b011);
}
void AssemblerRISCVV::vssseg5(VRegister vd, Register rs1, Register rs2,
                              VSew vsew, MaskType mask) {
  uint8_t width = vsew_switch(vsew);
  GenInstrV(STORE_FP, width, vd, rs1, rs2, mask, 0b10, 0, 0b100);
}
void AssemblerRISCVV::vssseg6(VRegister vd, Register rs1, Register rs2,
                              VSew vsew, MaskType mask) {
  uint8_t width = vsew_switch(vsew);
  GenInstrV(STORE_FP, width, vd, rs1, rs2, mask, 0b10, 0, 0b101);
}
void AssemblerRISCVV::vssseg7(VRegister vd, Register rs1, Register rs2,
                              VSew vsew, MaskType mask) {
  uint8_t width = vsew_switch(vsew);
  GenInstrV(STORE_FP, width, vd, rs1, rs2, mask, 0b10, 0, 0b110);
}
void AssemblerRISCVV::vssseg8(VRegister vd, Register rs1, Register rs2,
                              VSew vsew, MaskType mask) {
  uint8_t width = vsew_switch(vsew);
  GenInstrV(STORE_FP, width, vd, rs1, rs2, mask, 0b10, 0, 0b111);
}

void AssemblerRISCVV::vlxseg2(VRegister vd, Register rs1, VRegister rs2,
                              VSew vsew, MaskType mask) {
  uint8_t width = vsew_switch(vsew);
  GenInstrV(LOAD_FP, width, vd, rs1, rs2, mask, 0b11, 0, 0b001);
}
void AssemblerRISCVV::vlxseg3(VRegister vd, Register rs1, VRegister rs2,
                              VSew vsew, MaskType mask) {
  uint8_t width = vsew_switch(vsew);
  GenInstrV(LOAD_FP, width, vd, rs1, rs2, mask, 0b11, 0, 0b010);
}
void AssemblerRISCVV::vlxseg4(VRegister vd, Register rs1, VRegister rs2,
                              VSew vsew, MaskType mask) {
  uint8_t width = vsew_switch(vsew);
  GenInstrV(LOAD_FP, width, vd, rs1, rs2, mask, 0b11, 0, 0b011);
}
void AssemblerRISCVV::vlxseg5(VRegister vd, Register rs1, VRegister rs2,
                              VSew vsew, MaskType mask) {
  uint8_t width = vsew_switch(vsew);
  GenInstrV(LOAD_FP, width, vd, rs1, rs2, mask, 0b11, 0, 0b100);
}
void AssemblerRISCVV::vlxseg6(VRegister vd, Register rs1, VRegister rs2,
                              VSew vsew, MaskType mask) {
  uint8_t width = vsew_switch(vsew);
  GenInstrV(LOAD_FP, width, vd, rs1, rs2, mask, 0b11, 0, 0b101);
}
void AssemblerRISCVV::vlxseg7(VRegister vd, Register rs1, VRegister rs2,
                              VSew vsew, MaskType mask) {
  uint8_t width = vsew_switch(vsew);
  GenInstrV(LOAD_FP, width, vd, rs1, rs2, mask, 0b11, 0, 0b110);
}
void AssemblerRISCVV::vlxseg8(VRegister vd, Register rs1, VRegister rs2,
                              VSew vsew, MaskType mask) {
  uint8_t width = vsew_switch(vsew);
  GenInstrV(LOAD_FP, width, vd, rs1, rs2, mask, 0b11, 0, 0b111);
}
void AssemblerRISCVV::vsxseg2(VRegister vd, Register rs1, VRegister rs2,
                              VSew vsew, MaskType mask) {
  uint8_t width = vsew_switch(vsew);
  GenInstrV(STORE_FP, width, vd, rs1, rs2, mask, 0b11, 0, 0b001);
}
void AssemblerRISCVV::vsxseg3(VRegister vd, Register rs1, VRegister rs2,
                              VSew vsew, MaskType mask) {
  uint8_t width = vsew_switch(vsew);
  GenInstrV(STORE_FP, width, vd, rs1, rs2, mask, 0b11, 0, 0b010);
}
void AssemblerRISCVV::vsxseg4(VRegister vd, Register rs1, VRegister rs2,
                              VSew vsew, MaskType mask) {
  uint8_t width = vsew_switch(vsew);
  GenInstrV(STORE_FP, width, vd, rs1, rs2, mask, 0b11, 0, 0b011);
}
void AssemblerRISCVV::vsxseg5(VRegister vd, Register rs1, VRegister rs2,
                              VSew vsew, MaskType mask) {
  uint8_t width = vsew_switch(vsew);
  GenInstrV(STORE_FP, width, vd, rs1, rs2, mask, 0b11, 0, 0b100);
}
void AssemblerRISCVV::vsxseg6(VRegister vd, Register rs1, VRegister rs2,
                              VSew vsew, MaskType mask) {
  uint8_t width = vsew_switch(vsew);
  GenInstrV(STORE_FP, width, vd, rs1, rs2, mask, 0b11, 0, 0b101);
}
void AssemblerRISCVV::vsxseg7(VRegister vd, Register rs1, VRegister rs2,
                              VSew vsew, MaskType mask) {
  uint8_t width = vsew_switch(vsew);
  GenInstrV(STORE_FP, width, vd, rs1, rs2, mask, 0b11, 0, 0b110);
}
void AssemblerRISCVV::vsxseg8(VRegister vd, Register rs1, VRegister rs2,
                              VSew vsew, MaskType mask) {
  uint8_t width = vsew_switch(vsew);
  GenInstrV(STORE_FP, width, vd, rs1, rs2, mask, 0b11, 0, 0b111);
}

void AssemblerRISCVV::vfirst_m(Register rd, VRegister vs2, MaskType mask) {
  GenInstrV(VWXUNARY0_FUNCT6, OP_MVV, rd, 0b10001, vs2, mask);
}

void AssemblerRISCVV::vcpop_m(Register rd, VRegister vs2, MaskType mask) {
  GenInstrV(VWXUNARY0_FUNCT6, OP_MVV, rd, 0b10000, vs2, mask);
}

LoadStoreLaneParams::LoadStoreLaneParams(MachineRepresentation rep,
                                         uint8_t laneidx) {
#ifdef CAN_USE_RVV_INSTRUCTIONS
  switch (rep) {
    case MachineRepresentation::kWord8:
      *this = LoadStoreLaneParams(laneidx, 8, kRvvVLEN / 16);
      break;
    case MachineRepresentation::kWord16:
      *this = LoadStoreLaneParams(laneidx, 16, kRvvVLEN / 8);
      break;
    case MachineRepresentation::kWord32:
      *this = LoadStoreLaneParams(laneidx, 32, kRvvVLEN / 4);
      break;
    case MachineRepresentation::kWord64:
      *this = LoadStoreLaneParams(laneidx, 64, kRvvVLEN / 2);
      break;
    default:
      UNREACHABLE();
  }
#else
  UNREACHABLE();
#endif
}

}  // namespace internal
}  // namespace v8
