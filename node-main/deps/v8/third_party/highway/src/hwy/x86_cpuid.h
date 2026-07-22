// Copyright 2025 Google LLC
// SPDX-License-Identifier: Apache-2.0
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef HIGHWAY_HWY_X86_CPUID_H_
#define HIGHWAY_HWY_X86_CPUID_H_

// Wrapper for x86 CPUID intrinsics. Empty on other platforms.

#include <stdint.h>

#include "hwy/base.h"

#if HWY_ARCH_X86

#if HWY_COMPILER_MSVC || HWY_COMPILER_CLANGCL
#include <intrin.h>
#else
#include <cpuid.h>
#endif

namespace hwy {
namespace x86 {

// Calls CPUID instruction with eax=level and ecx=count and returns the result
// in abcd array where abcd = {eax, ebx, ecx, edx} (hence the name abcd).
static inline void Cpuid(const uint32_t level, const uint32_t count,
                         uint32_t* HWY_RESTRICT abcd) {
#if HWY_COMPILER_MSVC || HWY_COMPILER_CLANGCL
  int regs[4];
  __cpuidex(regs, static_cast<int>(level), static_cast<int>(count));
  for (int i = 0; i < 4; ++i) {
    abcd[i] = static_cast<uint32_t>(regs[i]);
  }
#else   // HWY_COMPILER_MSVC || HWY_COMPILER_CLANGCL
  uint32_t a;
  uint32_t b;
  uint32_t c;
  uint32_t d;
  __cpuid_count(level, count, a, b, c, d);
  abcd[0] = a;
  abcd[1] = b;
  abcd[2] = c;
  abcd[3] = d;
#endif  // HWY_COMPILER_MSVC || HWY_COMPILER_CLANGCL
}

static inline bool IsBitSet(const uint32_t reg, const int index) {
  return (reg & (1U << index)) != 0;
}

static inline uint32_t MaxLevel() {
  uint32_t abcd[4];
  Cpuid(0, 0, abcd);
  return abcd[0];
}

static inline bool IsAMD() {
  uint32_t abcd[4];
  Cpuid(0, 0, abcd);
  const uint32_t max_level = abcd[0];
  return max_level >= 1 && abcd[1] == 0x68747541 && abcd[2] == 0x444d4163 &&
         abcd[3] == 0x69746e65;
}

}  // namespace x86
}  // namespace hwy

#endif  // HWY_ARCH_X86
#endif  // HIGHWAY_HWY_X86_CPUID_H_
