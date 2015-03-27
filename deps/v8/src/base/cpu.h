// Copyright 2006-2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This module contains the architecture-specific code. This make the rest of
// the code less dependent on differences between different processor
// architecture.
// The classes have the same definition for all architectures. The
// implementation for a particular architecture is put in cpu_<arch>.cc.
// The build system then uses the implementation for the target architecture.
//

#ifndef V8_BASE_CPU_H_
#define V8_BASE_CPU_H_

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

class CPU FINAL {
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
  static const int ARM = 0x41;
  static const int NVIDIA = 0x4e;
  static const int QUALCOMM = 0x51;
  int architecture() const { return architecture_; }
  int variant() const { return variant_; }
  static const int NVIDIA_DENVER = 0x0;
  int part() const { return part_; }

  // ARM-specific part codes
  static const int ARM_CORTEX_A5 = 0xc05;
  static const int ARM_CORTEX_A7 = 0xc07;
  static const int ARM_CORTEX_A8 = 0xc08;
  static const int ARM_CORTEX_A9 = 0xc09;
  static const int ARM_CORTEX_A12 = 0xc0c;
  static const int ARM_CORTEX_A15 = 0xc0f;

  // PPC-specific part codes
  enum {
    PPC_POWER5,
    PPC_POWER6,
    PPC_POWER7,
    PPC_POWER8,
    PPC_G4,
    PPC_G5,
    PPC_PA6T
  };

  // General features
  bool has_fpu() const { return has_fpu_; }

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
  bool has_fma3() const { return has_fma3_; }
  bool is_atom() const { return is_atom_; }

  // arm features
  bool has_idiva() const { return has_idiva_; }
  bool has_neon() const { return has_neon_; }
  bool has_thumb2() const { return has_thumb2_; }
  bool has_vfp() const { return has_vfp_; }
  bool has_vfp3() const { return has_vfp3_; }
  bool has_vfp3_d32() const { return has_vfp3_d32_; }

  // mips features
  bool is_fp64_mode() const { return is_fp64_mode_; }

 private:
  char vendor_[13];
  int stepping_;
  int model_;
  int ext_model_;
  int family_;
  int ext_family_;
  int type_;
  int implementer_;
  int architecture_;
  int variant_;
  int part_;
  bool has_fpu_;
  bool has_cmov_;
  bool has_sahf_;
  bool has_mmx_;
  bool has_sse_;
  bool has_sse2_;
  bool has_sse3_;
  bool has_ssse3_;
  bool has_sse41_;
  bool has_sse42_;
  bool is_atom_;
  bool has_osxsave_;
  bool has_avx_;
  bool has_fma3_;
  bool has_idiva_;
  bool has_neon_;
  bool has_thumb2_;
  bool has_vfp_;
  bool has_vfp3_;
  bool has_vfp3_d32_;
  bool is_fp64_mode_;
};

} }  // namespace v8::base

#endif  // V8_BASE_CPU_H_
