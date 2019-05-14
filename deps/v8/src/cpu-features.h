// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CPU_FEATURES_H_
#define V8_CPU_FEATURES_H_

#include "src/globals.h"

namespace v8 {

namespace internal {

// CPU feature flags.
enum CpuFeature {
  // x86
  SSE4_1,
  SSSE3,
  SSE3,
  SAHF,
  AVX,
  FMA3,
  BMI1,
  BMI2,
  LZCNT,
  POPCNT,
  ATOM,
  // ARM
  // - Standard configurations. The baseline is ARMv6+VFPv2.
  ARMv7,        // ARMv7-A + VFPv3-D32 + NEON
  ARMv7_SUDIV,  // ARMv7-A + VFPv4-D32 + NEON + SUDIV
  ARMv8,        // ARMv8-A (+ all of the above)
  // MIPS, MIPS64
  FPU,
  FP64FPU,
  MIPSr1,
  MIPSr2,
  MIPSr6,
  MIPS_SIMD,  // MSA instructions
  // PPC
  FPR_GPR_MOV,
  LWSYNC,
  ISELECT,
  VSX,
  MODULO,
  // S390
  DISTINCT_OPS,
  GENERAL_INSTR_EXT,
  FLOATING_POINT_EXT,
  VECTOR_FACILITY,
  MISC_INSTR_EXT2,

  NUMBER_OF_CPU_FEATURES,

  // ARM feature aliases (based on the standard configurations above).
  VFPv3 = ARMv7,
  NEON = ARMv7,
  VFP32DREGS = ARMv7,
  SUDIV = ARMv7_SUDIV
};

// CpuFeatures keeps track of which features are supported by the target CPU.
// Supported features must be enabled by a CpuFeatureScope before use.
// Example:
//   if (assembler->IsSupported(SSE3)) {
//     CpuFeatureScope fscope(assembler, SSE3);
//     // Generate code containing SSE3 instructions.
//   } else {
//     // Generate alternative code.
//   }
class CpuFeatures : public AllStatic {
 public:
  static void Probe(bool cross_compile) {
    STATIC_ASSERT(NUMBER_OF_CPU_FEATURES <= kBitsPerInt);
    if (initialized_) return;
    initialized_ = true;
    ProbeImpl(cross_compile);
  }

  static unsigned SupportedFeatures() {
    Probe(false);
    return supported_;
  }

  static bool IsSupported(CpuFeature f) {
    return (supported_ & (1u << f)) != 0;
  }

  static inline bool SupportsOptimizer();

  static inline bool SupportsWasmSimd128();

  static inline unsigned icache_line_size() {
    DCHECK_NE(icache_line_size_, 0);
    return icache_line_size_;
  }

  static inline unsigned dcache_line_size() {
    DCHECK_NE(dcache_line_size_, 0);
    return dcache_line_size_;
  }

  static void PrintTarget();
  static void PrintFeatures();

 private:
  friend void V8_EXPORT_PRIVATE FlushInstructionCache(void*, size_t);
  friend class ExternalReference;
  // Flush instruction cache.
  static void FlushICache(void* start, size_t size);

  // Platform-dependent implementation.
  static void ProbeImpl(bool cross_compile);

  static unsigned supported_;
  static unsigned icache_line_size_;
  static unsigned dcache_line_size_;
  static bool initialized_;
  DISALLOW_COPY_AND_ASSIGN(CpuFeatures);
};

}  // namespace internal
}  // namespace v8
#endif  // V8_CPU_FEATURES_H_
