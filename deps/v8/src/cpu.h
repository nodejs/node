// Copyright 2006-2013 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// This module contains the architecture-specific code. This make the rest of
// the code less dependent on differences between different processor
// architecture.
// The classes have the same definition for all architectures. The
// implementation for a particular architecture is put in cpu_<arch>.cc.
// The build system then uses the implementation for the target architecture.
//

#ifndef V8_CPU_H_
#define V8_CPU_H_

#include "allocation.h"

namespace v8 {
namespace internal {

// ----------------------------------------------------------------------------
// CPU
//
// Query information about the processor.
//
// This class also has static methods for the architecture specific functions.
// Add methods here to cope with differences between the supported
// architectures. For each architecture the file cpu_<arch>.cc contains the
// implementation of these static functions.

class CPU V8_FINAL BASE_EMBEDDED {
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
  static const int QUALCOMM = 0x51;
  int architecture() const { return architecture_; }
  int part() const { return part_; }
  static const int ARM_CORTEX_A5 = 0xc05;
  static const int ARM_CORTEX_A7 = 0xc07;
  static const int ARM_CORTEX_A8 = 0xc08;
  static const int ARM_CORTEX_A9 = 0xc09;
  static const int ARM_CORTEX_A12 = 0xc0c;
  static const int ARM_CORTEX_A15 = 0xc0f;

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

  // arm features
  bool has_idiva() const { return has_idiva_; }
  bool has_neon() const { return has_neon_; }
  bool has_thumbee() const { return has_thumbee_; }
  bool has_vfp() const { return has_vfp_; }
  bool has_vfp3() const { return has_vfp3_; }
  bool has_vfp3_d32() const { return has_vfp3_d32_; }

  // Returns the number of processors online.
  static int NumberOfProcessorsOnline();

  // Initializes the cpu architecture support. Called once at VM startup.
  static void SetUp();

  static bool SupportsCrankshaft();

  // Flush instruction cache.
  static void FlushICache(void* start, size_t size);

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
  bool has_idiva_;
  bool has_neon_;
  bool has_thumbee_;
  bool has_vfp_;
  bool has_vfp3_;
  bool has_vfp3_d32_;
};

} }  // namespace v8::internal

#endif  // V8_CPU_H_
