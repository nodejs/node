// Copyright 2006-2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BASE_CPU_CPU_H_
#define V8_BASE_CPU_CPU_H_

// This module contains the architecture-specific code. This make the rest of
// the code less dependent on differences between different processor
// architecture.
// The classes have the same definition for all architectures. The
// implementation for a particular architecture is put in cpu_<arch>.cc.
// The build system then uses the implementation for the target architecture.
//

#include "src/base/base-export.h"
#include "src/base/macros.h"

namespace v8 {
namespace base {

// ----------------------------------------------------------------------------
// CPU
//
// Query information about the processor.
//
// This class also has static methods for the architecture specific functions.
// Add methods here to cope with differences between the supported
// architectures. For each architecture the file cpu_<arch>.cc contains the
// implementation of these static functions.

class V8_BASE_EXPORT CPU final {
 public:
  CPU();

  // x86 CPUID information
  const char* vendor() const { return vendor_; }
  int stepping() const { return stepping_; }
  int model() const { return model_; }
  int ext_model() const { return ext_model_; }
  int family() const { return family_; }
  int ext_family() const { return ext_family_; }
  int type() const { return type_; }

  // arm implementer/part information
  int implementer() const { return implementer_; }
  static constexpr int kArm = 0x41;
  static constexpr int kNvidia = 0x4e;
  static constexpr int kQualcomm = 0x51;
  int architecture() const { return architecture_; }
  int variant() const { return variant_; }
  static constexpr int kNvidiaDenver = 0x0;
  int part() const { return part_; }

  // ARM-specific part codes
  static constexpr int kArmCortexA5 = 0xc05;
  static constexpr int kArmCortexA7 = 0xc07;
  static constexpr int kArmCortexA8 = 0xc08;
  static constexpr int kArmCortexA9 = 0xc09;
  static constexpr int kArmCortexA12 = 0xc0c;
  static constexpr int kArmCortexA15 = 0xc0f;

  // Denver-specific part code
  static constexpr int kNvidiaDenverV10 = 0x002;

  // PPC-specific part codes
  enum { kPPCPower9, kPPCPower10, kPPCPower11 };

  // General features
  bool has_fpu() const { return has_fpu_; }
  int icache_line_size() const { return icache_line_size_; }
  int dcache_line_size() const { return dcache_line_size_; }
  static constexpr int kUnknownCacheLineSize = 0;

  // x86 features
  bool has_cmov() const { return has_cmov_; }
  bool has_sahf() const { return has_sahf_; }
  bool has_mmx() const { return has_mmx_; }
  bool has_sse() const { return has_sse_; }
  bool has_sse2() const { return has_sse2_; }
  bool has_sse3() const { return has_sse3_; }
  bool has_ssse3() const { return has_ssse3_; }
  bool has_sse41() const { return has_sse41_; }
  bool has_sse42() const { return has_sse42_; }
  bool has_osxsave() const { return has_osxsave_; }
  bool has_avx() const { return has_avx_; }
  bool has_avx2() const { return has_avx2_; }
  bool has_avx_vnni() const { return has_avx_vnni_; }
  bool has_avx_vnni_int8() const { return has_avx_vnni_int8_; }
  bool has_fma3() const { return has_fma3_; }
  bool has_f16c() const { return has_f16c_; }
  bool has_bmi1() const { return has_bmi1_; }
  bool has_bmi2() const { return has_bmi2_; }
  bool has_lzcnt() const { return has_lzcnt_; }
  bool has_popcnt() const { return has_popcnt_; }
  bool has_apx_f() const { return has_apx_f_; }
  bool is_atom() const { return is_atom_; }
  bool has_intel_jcc_erratum() const { return has_intel_jcc_erratum_; }
  bool has_cetss() const { return has_cetss_; }
  bool has_non_stop_time_stamp_counter() const {
    return has_non_stop_time_stamp_counter_;
  }
  bool is_running_in_vm() const { return is_running_in_vm_; }
  bool exposes_num_virtual_address_bits() const {
    return num_virtual_address_bits_ != kUnknownNumVirtualAddressBits;
  }
  int num_virtual_address_bits() const {
    DCHECK(exposes_num_virtual_address_bits());
    return num_virtual_address_bits_;
  }
  static constexpr int kUnknownNumVirtualAddressBits = 0;

  // arm features
  bool has_idiva() const { return has_idiva_; }
  bool has_neon() const { return has_neon_; }
  bool has_thumb2() const { return has_thumb2_; }
  bool has_vfp() const { return has_vfp_; }
  bool has_vfp3() const { return has_vfp3_; }
  bool has_vfp3_d32() const { return has_vfp3_d32_; }
  bool has_jscvt() const { return has_jscvt_; }
  bool has_dot_prod() const { return has_dot_prod_; }
  bool has_lse() const { return has_lse_; }
  bool has_mte() const { return has_mte_; }
  bool has_sha3() const { return has_sha3_; }
  bool has_pmull1q() const { return has_pmull1q_; }
  bool has_fp16() const { return has_fp16_; }
  bool has_hbc() const { return has_hbc_; }
  bool has_cssc() const { return has_cssc_; }
  bool has_mops() const { return has_mops_; }
  bool has_svebitperm() const { return has_svebitperm_; }

  // mips features
  bool is_fp64_mode() const { return is_fp64_mode_; }
  bool has_msa() const { return has_msa_; }

  // riscv-specific part codes
  unsigned vlen() const { return vlen_; }
  bool has_rvv() const { return has_rvv_; }
  bool has_zba() const { return has_zba_; }
  bool has_zbb() const { return has_zbb_; }
  bool has_zbs() const { return has_zbs_; }
  enum class RV_MMU_MODE {
    kRiscvSV39,
    kRiscvSV48,
    kRiscvSV57,
  };
  RV_MMU_MODE riscv_mmu() const { return riscv_mmu_; }
  static constexpr unsigned kUnknownVlen = 0;

  // LoongArch features
  bool has_lsx() const { return has_lsx_; }
  bool has_lasx() const { return has_lasx_; }

 private:
#if defined(V8_OS_STARBOARD)
  bool StarboardDetectCPU();
#endif
  void DetectFeatures();

  char vendor_[13] = {};
  int stepping_ = 0;
  int model_ = 0;
  int ext_model_ = 0;
  int family_ = 0;
  int ext_family_ = 0;
  int type_ = 0;
  int implementer_ = 0;
  int architecture_ = 0;
  int variant_ = -1;
  int part_ = 0;
  int icache_line_size_ = kUnknownCacheLineSize;
  int dcache_line_size_ = kUnknownCacheLineSize;
  int num_virtual_address_bits_ = kUnknownNumVirtualAddressBits;
  bool has_fpu_ = false;
  bool has_cmov_ = false;
  bool has_sahf_ = false;
  bool has_mmx_ = false;
  bool has_sse_ = false;
  bool has_sse2_ = false;
  bool has_sse3_ = false;
  bool has_ssse3_ = false;
  bool has_sse41_ = false;
  bool has_sse42_ = false;
  bool is_atom_ = false;
  bool has_intel_jcc_erratum_ = false;
  bool has_cetss_ = false;
  bool has_osxsave_ = false;
  bool has_avx_ = false;
  bool has_avx2_ = false;
  bool has_avx_vnni_ = false;
  bool has_avx_vnni_int8_ = false;
  bool has_fma3_ = false;
  bool has_f16c_ = false;
  bool has_bmi1_ = false;
  bool has_bmi2_ = false;
  bool has_lzcnt_ = false;
  bool has_popcnt_ = false;
  bool has_apx_f_ = false;
  bool has_idiva_ = false;
  bool has_neon_ = false;
  bool has_thumb2_ = false;
  bool has_vfp_ = false;
  bool has_vfp3_ = false;
  bool has_vfp3_d32_ = false;
  bool has_jscvt_ = false;
  bool has_dot_prod_ = false;
  bool has_lse_ = false;
  bool has_mte_ = false;
  bool has_sha3_ = false;
  bool has_pmull1q_ = false;
  bool has_fp16_ = false;
  bool has_hbc_ = false;
  bool has_cssc_ = false;
  bool has_mops_ = false;
  bool has_svebitperm_ = false;
  bool is_fp64_mode_ = false;
  bool has_non_stop_time_stamp_counter_ = false;
  bool is_running_in_vm_ = false;
  bool has_msa_ = false;
  RV_MMU_MODE riscv_mmu_ = RV_MMU_MODE::kRiscvSV48;
  unsigned vlen_ = kUnknownVlen;
  bool has_rvv_ = false;
  bool has_zba_ = false;
  bool has_zbb_ = false;
  bool has_zbs_ = false;
  bool has_lsx_ = false;
  bool has_lasx_ = false;
};

}  // namespace base
}  // namespace v8

#endif  // V8_BASE_CPU_CPU_H_
