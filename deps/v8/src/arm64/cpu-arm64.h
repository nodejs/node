// Copyright 2013 the V8 project authors. All rights reserved.
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

#ifndef V8_ARM64_CPU_ARM64_H_
#define V8_ARM64_CPU_ARM64_H_

#include <stdio.h>
#include "serialize.h"
#include "cpu.h"

namespace v8 {
namespace internal {


// CpuFeatures keeps track of which features are supported by the target CPU.
// Supported features must be enabled by a CpuFeatureScope before use.
class CpuFeatures : public AllStatic {
 public:
  // Detect features of the target CPU. Set safe defaults if the serializer
  // is enabled (snapshots must be portable).
  static void Probe();

  // Check whether a feature is supported by the target CPU.
  static bool IsSupported(CpuFeature f) {
    ASSERT(initialized_);
    // There are no optional features for ARM64.
    return false;
  };

  static bool IsFoundByRuntimeProbingOnly(CpuFeature f) {
    ASSERT(initialized_);
    // There are no optional features for ARM64.
    return false;
  }

  static bool IsSafeForSnapshot(CpuFeature f) {
    return (IsSupported(f) &&
            (!Serializer::enabled() || !IsFoundByRuntimeProbingOnly(f)));
  }

  // I and D cache line size in bytes.
  static unsigned dcache_line_size();
  static unsigned icache_line_size();

  static unsigned supported_;

  static bool VerifyCrossCompiling() {
    // There are no optional features for ARM64.
    ASSERT(cross_compile_ == 0);
    return true;
  }

  static bool VerifyCrossCompiling(CpuFeature f) {
    // There are no optional features for ARM64.
    USE(f);
    ASSERT(cross_compile_ == 0);
    return true;
  }

 private:
  // Return the content of the cache type register.
  static uint32_t GetCacheType();

  // I and D cache line size in bytes.
  static unsigned icache_line_size_;
  static unsigned dcache_line_size_;

#ifdef DEBUG
  static bool initialized_;
#endif

  // This isn't used (and is always 0), but it is required by V8.
  static unsigned found_by_runtime_probing_only_;

  static unsigned cross_compile_;

  friend class PlatformFeatureScope;
  DISALLOW_COPY_AND_ASSIGN(CpuFeatures);
};

} }  // namespace v8::internal

#endif  // V8_ARM64_CPU_ARM64_H_
