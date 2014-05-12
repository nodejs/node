// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
  static void Probe(bool serializer_enabled);

  // Check whether a feature is supported by the target CPU.
  static bool IsSupported(CpuFeature f) {
    ASSERT(initialized_);
    // There are no optional features for ARM64.
    return false;
  };

  // There are no optional features for ARM64.
  static bool IsSafeForSnapshot(Isolate* isolate, CpuFeature f) {
    return IsSupported(f);
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

  static bool SupportsCrankshaft() { return true; }

 private:
#ifdef DEBUG
  static bool initialized_;
#endif

  // This isn't used (and is always 0), but it is required by V8.
  static unsigned cross_compile_;

  friend class PlatformFeatureScope;
  DISALLOW_COPY_AND_ASSIGN(CpuFeatures);
};

} }  // namespace v8::internal

#endif  // V8_ARM64_CPU_ARM64_H_
