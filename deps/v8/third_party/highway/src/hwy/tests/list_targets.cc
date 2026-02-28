// Copyright 2020 Google LLC
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

// Simple tool to print the list of targets that were compiled in when building
// this tool.

#include <stdint.h>
#include <stdio.h>

#include "hwy/detect_compiler_arch.h"
#include "hwy/timer.h"  // GetCpuString

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "hwy/tests/list_targets.cc"
#include "hwy/foreach_target.h"  // IWYU pragma: keep
#include "hwy/highway.h"

HWY_BEFORE_NAMESPACE();
namespace hwy {
namespace HWY_NAMESPACE {

void PrintCompiler() {
  if (HWY_COMPILER_ICX) {
    // HWY_COMPILER_ICX needs to be checked first as ICX is clang-based and as
    // it is also possible for both HWY_COMPILER_CLANGCL and HWY_COMPILER_CLANG
    // to be nonzero when compiling with ICX on Windows.
    fprintf(stderr, "Compiler: ISX %d\n", HWY_COMPILER_ICX);
  } else if (HWY_COMPILER_CLANGCL) {
    // HWY_COMPILER_CLANGCL needs to be checked before HWY_COMPILER_CLANG as
    // HWY_COMPILER_CLANG and HWY_COMPILER_CLANGCL are usually both nonzero if
    // compiling with clang-cl.
    fprintf(stderr, "Compiler: Clang-cl %d\n", HWY_COMPILER_CLANGCL);
  } else if (HWY_COMPILER_CLANG) {
    fprintf(stderr, "Compiler: Clang %d\n", HWY_COMPILER_CLANG);
  } else if (HWY_COMPILER_GCC_ACTUAL) {
    fprintf(stderr, "Compiler: GCC %d\n", HWY_COMPILER_GCC_ACTUAL);
  } else if (HWY_COMPILER_ICC) {
    fprintf(stderr, "Compiler: ICC %d\n", HWY_COMPILER_ICC);
  } else if (HWY_COMPILER_MSVC) {
    fprintf(stderr, "Compiler: MSVC %d\n", HWY_COMPILER_MSVC);
  } else {
    fprintf(stderr, "Compiler unknown!\n");
  }
}

void PrintConfig() {
#ifdef HWY_COMPILE_ONLY_EMU128
  const int only_emu128 = 1;
#else
  const int only_emu128 = 0;
#endif
#ifdef HWY_COMPILE_ONLY_SCALAR
  const int only_scalar = 1;
#else
  const int only_scalar = 0;
#endif
#ifdef HWY_COMPILE_ONLY_STATIC
  const int only_static = 1;
#else
  const int only_static = 0;
#endif
#ifdef HWY_COMPILE_ALL_ATTAINABLE
  const int all_attain = 1;
#else
  const int all_attain = 0;
#endif
#ifdef HWY_IS_TEST
  const int is_test = 1;
#else
  const int is_test = 0;
#endif
#ifdef HWY_TARGET_STR
  const char* target_str = HWY_TARGET_STR;
#else
  const char* target_str = "";
#endif
  fprintf(stderr,
          "Target attributes: %s\n"
          "Config: emu128:%d scalar:%d static:%d all_attain:%d is_test:%d\n",
          target_str, only_emu128, only_scalar, only_static, all_attain,
          is_test);
}

void PrintHave() {
  fprintf(stderr,
          "Have: constexpr_lanes:%d runtime_dispatch:%d auxv:%d "
          "f16 type:%d/ops%d bf16 type:%d/ops%d\n",
          HWY_HAVE_CONSTEXPR_LANES, HWY_HAVE_RUNTIME_DISPATCH, HWY_HAVE_AUXV,
          HWY_HAVE_SCALAR_F16_TYPE, HWY_HAVE_SCALAR_F16_OPERATORS,
          HWY_HAVE_SCALAR_BF16_TYPE, HWY_HAVE_SCALAR_BF16_OPERATORS);
}

void PrintTargets(const char* msg, int64_t targets) {
  fprintf(stderr, "%s", msg);
  // For each bit other than the sign bit:
  for (int64_t x = targets & 0x7FFFFFFFFFFFFFFFLL; x != 0; x = x & (x - 1)) {
    // Extract value of least-significant bit.
    fprintf(stderr, " %s", hwy::TargetName(x & (~x + 1)));
  }
  fprintf(stderr, "\n");
}

void TestVisitor() {
  long long enabled = 0;  // NOLINT
#define PER_TARGET(TARGET, NAMESPACE) enabled |= TARGET;
  HWY_VISIT_TARGETS(PER_TARGET)
  if (enabled != HWY_TARGETS) {
    HWY_ABORT("Enabled %llx != HWY_TARGETS %llx\n", enabled, HWY_TARGETS);
  }
}

void PrintAll() {
  PrintCompiler();
  PrintConfig();
  PrintHave();

  PrintTargets("Compiled HWY_TARGETS:  ", HWY_TARGETS);
  PrintTargets("HWY_ATTAINABLE_TARGETS:", HWY_ATTAINABLE_TARGETS);
  PrintTargets("HWY_BASELINE_TARGETS:  ", HWY_BASELINE_TARGETS);
  PrintTargets("HWY_STATIC_TARGET:     ", HWY_STATIC_TARGET);
  PrintTargets("HWY_BROKEN_TARGETS:    ", HWY_BROKEN_TARGETS);
  PrintTargets("HWY_DISABLED_TARGETS:  ", HWY_DISABLED_TARGETS);
  PrintTargets("CPU supports:          ", hwy::SupportedTargets());

  char cpu100[100];
  (void)platform::GetCpuString(cpu100);
  fprintf(stderr, "CPU: %s\n", cpu100);

  TestVisitor();
}

// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace hwy
HWY_AFTER_NAMESPACE();

#if HWY_ONCE
namespace hwy {
namespace {
HWY_EXPORT(PrintAll);
void CallPrintAll() { HWY_DYNAMIC_DISPATCH(PrintAll)(); }
}  // namespace
}  // namespace hwy

int main() {
  hwy::CallPrintAll();
  return 0;
}
#endif  // HWY_ONCE
