// Copyright 2026 Google LLC
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

// clang-format off
#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "hwy/contrib/math/math_benchmark.cc"
#include "hwy/foreach_target.h"  // IWYU pragma: keep
#include "hwy/highway.h"
#include "hwy/contrib/math/math-inl.h"
#include "hwy/contrib/math/fast_math-inl.h"
#include "hwy/nanobenchmark.h"
#include "hwy/tests/test_util-inl.h"
// clang-format on

#include <stdio.h>
#include <string.h>

// ============================================================================
// You can dynamically select two highway math functions from the list to
// benchmark by passing their names via command line flags `--fxn1` and
// `--fxn2`. Defaults:
//   If no flags are passed, the benchmark automatically defaults to
//   benchmarking `FastExp` and `Exp`.
//
// Note:
//   - Either provide 2 function flags or none.
//   - Function names must be from the list of valid functions.
// ============================================================================

namespace hwy {
extern const char* g_fxn1;
extern const char* g_fxn2;
}  // namespace hwy

#define HWY_MATH_BENCHMARKS(V) \
  V(Exp)                       \
  V(FastExp)                   \
  V(FastExpNormal)             \
  V(Exp2)                      \
  V(FastExp2)                  \
  V(FastExp2Normal)            \
  V(FastExpMinusOrZero)        \
  V(Log)                       \
  V(FastLog)                   \
  V(FastLogPositiveNormal)     \
  V(Log2)                      \
  V(FastLog2)                  \
  V(FastLog2PositiveNormal)    \
  V(Log10)                     \
  V(FastLog10)                 \
  V(FastLog10PositiveNormal)   \
  V(Log1p)                     \
  V(FastLog1p)                 \
  V(FastLog1pPositiveNormal)   \
  V(Atan)                      \
  V(FastAtan)                  \
  V(FastAtanPositive)          \
  V(Tanh)                      \
  V(FastTanh)                  \
  V(Atan2)                     \
  V(FastAtan2)

HWY_BEFORE_NAMESPACE();
namespace hwy {
namespace HWY_NAMESPACE {

namespace hn = hwy::HWY_NAMESPACE;

// Helper to safely convert float/double results to benchmark output
// without triggering SIGILL on Infinity or compile errors on size mismatch.
template <class Val>
hwy::FuncOutput SafeCast(Val v) {
  using BitsT = hwy::MakeUnsigned<Val>;
  auto bits = hwy::BitCastScalar<BitsT>(v);
  return static_cast<hwy::FuncOutput>(bits);
}

// Macro to define benchmark body
#define DEFINE_MATH_BENCH(NAME, FUNC, MAP_EXPR)                             \
  template <class D>                                                        \
  void Bench##NAME(D d) {                                                   \
    using T = hn::TFromD<D>;                                                \
    printf("Benchmarking " #NAME " for %s:\n",                              \
           hwy::TypeName(T(), hn::Lanes(d)).c_str());                       \
    auto func = [d](const hwy::FuncInput in) -> hwy::FuncOutput {           \
      const double val = MAP_EXPR;                                          \
      const auto v = hn::Set(d, static_cast<T>(val));                       \
      const auto res = FUNC;                                                \
      return SafeCast(hn::GetLane(res));                                    \
    };                                                                      \
    const size_t kNumInputs = 16;                                           \
    hwy::FuncInput inputs[kNumInputs];                                      \
    for (size_t i = 0; i < kNumInputs; ++i) inputs[i] = i;                  \
    hwy::Result results[kNumInputs];                                        \
    hwy::Params params = hwy::DefaultBenchmarkParams();                     \
    const size_t num_results =                                              \
        hwy::MeasureClosure(func, inputs, kNumInputs, results, params);     \
    double sum_ticks = 0;                                                   \
    for (size_t i = 0; i < num_results; ++i) sum_ticks += results[i].ticks; \
    if (num_results > 0) {                                                  \
      printf("  Avg ticks: %f\n", sum_ticks / num_results);                 \
    }                                                                       \
  }

// Macro to define benchmark body for 2-element variants
#define DEFINE_MATH_BENCH_2ARG(NAME, FUNC, MAP_EXPR_Y, MAP_EXPR_X)          \
  template <class D>                                                        \
  void Bench##NAME(D d) {                                                   \
    using T = hn::TFromD<D>;                                                \
    printf("Benchmarking " #NAME " for %s:\n",                              \
           hwy::TypeName(T(), hn::Lanes(d)).c_str());                       \
    auto func = [d](const hwy::FuncInput in) -> hwy::FuncOutput {           \
      const double val_y = MAP_EXPR_Y;                                      \
      const double val_x = MAP_EXPR_X;                                      \
      const auto y = hn::Set(d, static_cast<T>(val_y));                     \
      const auto x = hn::Set(d, static_cast<T>(val_x));                     \
      const auto res = FUNC;                                                \
      return SafeCast(hn::GetLane(res));                                    \
    };                                                                      \
    const size_t kNumInputs = 16;                                           \
    hwy::FuncInput inputs[kNumInputs];                                      \
    for (size_t i = 0; i < kNumInputs; ++i) inputs[i] = i;                  \
    hwy::Result results[kNumInputs];                                        \
    hwy::Params params = hwy::DefaultBenchmarkParams();                     \
    const size_t num_results =                                              \
        hwy::MeasureClosure(func, inputs, kNumInputs, results, params);     \
    double sum_ticks = 0;                                                   \
    for (size_t i = 0; i < num_results; ++i) sum_ticks += results[i].ticks; \
    if (num_results > 0) {                                                  \
      printf("  Avg ticks: %f\n", sum_ticks / num_results);                 \
    }                                                                       \
  }

// Exp / FastExp
DEFINE_MATH_BENCH(CallExp, hn::CallExp(d, v),
                  -10.0 + static_cast<double>(in) * (20.0 / 15.0))
DEFINE_MATH_BENCH(CallFastExp, hn::CallFastExp(d, v),
                  -10.0 + static_cast<double>(in) * (20.0 / 15.0))
DEFINE_MATH_BENCH(CallFastExpNormal, hn::CallFastExpNormal(d, v),
                  -10.0 + static_cast<double>(in) * (20.0 / 15.0))

// Exp2 / FastExp2
DEFINE_MATH_BENCH(CallExp2, hn::CallExp2(d, v),
                  -10.0 + static_cast<double>(in) * (20.0 / 15.0))
DEFINE_MATH_BENCH(CallFastExp2, hn::CallFastExp2(d, v),
                  -10.0 + static_cast<double>(in) * (20.0 / 15.0))
DEFINE_MATH_BENCH(CallFastExp2Normal, hn::CallFastExp2Normal(d, v),
                  -10.0 + static_cast<double>(in) * (20.0 / 15.0))

// FastExpMinusOrZero
DEFINE_MATH_BENCH(CallFastExpMinusOrZero, hn::CallFastExpMinusOrZero(d, v),
                  -10.0 + static_cast<double>(in) * (10.0 / 15.0))

// Log / FastLog
DEFINE_MATH_BENCH(CallLog, hn::CallLog(d, v),
                  0.1 + static_cast<double>(in) * 1.0)
DEFINE_MATH_BENCH(CallFastLog, hn::CallFastLog(d, v),
                  0.1 + static_cast<double>(in) * 1.0)
DEFINE_MATH_BENCH(CallFastLogPositiveNormal,
                  hn::CallFastLogPositiveNormal(d, v),
                  0.1 + static_cast<double>(in) * 1.0)

// Log2 / FastLog2
DEFINE_MATH_BENCH(CallLog2, hn::CallLog2(d, v),
                  0.1 + static_cast<double>(in) * 1.0)
DEFINE_MATH_BENCH(CallFastLog2, hn::CallFastLog2(d, v),
                  0.1 + static_cast<double>(in) * 1.0)
DEFINE_MATH_BENCH(CallFastLog2PositiveNormal,
                  hn::CallFastLog2PositiveNormal(d, v),
                  0.1 + static_cast<double>(in) * 1.0)

// Log10 / FastLog10
DEFINE_MATH_BENCH(CallLog10, hn::CallLog10(d, v),
                  0.1 + static_cast<double>(in) * 1.0)
DEFINE_MATH_BENCH(CallFastLog10, hn::CallFastLog10(d, v),
                  0.1 + static_cast<double>(in) * 1.0)
DEFINE_MATH_BENCH(CallFastLog10PositiveNormal,
                  hn::CallFastLog10PositiveNormal(d, v),
                  0.1 + static_cast<double>(in) * 1.0)

// Log1p / FastLog1p
DEFINE_MATH_BENCH(CallLog1p, hn::CallLog1p(d, v),
                  0.1 + static_cast<double>(in) * 1.0)
DEFINE_MATH_BENCH(CallFastLog1p, hn::CallFastLog1p(d, v),
                  0.1 + static_cast<double>(in) * 1.0)
DEFINE_MATH_BENCH(CallFastLog1pPositiveNormal,
                  hn::CallFastLog1pPositiveNormal(d, v),
                  0.1 + static_cast<double>(in) * 1.0)

// Atan / FastAtan
DEFINE_MATH_BENCH(CallAtan, hn::CallAtan(d, v),
                  -10.0 + static_cast<double>(in) * (20.0 / 15.0))
DEFINE_MATH_BENCH(CallFastAtan, hn::CallFastAtan(d, v),
                  -10.0 + static_cast<double>(in) * (20.0 / 15.0))
DEFINE_MATH_BENCH(CallFastAtanPositive, hn::CallFastAtanPositive(d, v),
                  0.1 + static_cast<double>(in) * (20.0 / 15.0))

// Tanh / FastTanh
DEFINE_MATH_BENCH(CallTanh, hn::CallTanh(d, v),
                  -10.0 + static_cast<double>(in) * (20.0 / 15.0))
DEFINE_MATH_BENCH(CallFastTanh, hn::CallFastTanh(d, v),
                  -10.0 + static_cast<double>(in) * (20.0 / 15.0))

// Atan2 / FastAtan2
DEFINE_MATH_BENCH_2ARG(CallAtan2, hn::CallAtan2(d, y, x),
                       -10.0 + static_cast<double>(in / 4) * (20.0 / 3.0),
                       -10.0 + static_cast<double>(in % 4) * (20.0 / 3.0))
DEFINE_MATH_BENCH_2ARG(CallFastAtan2, hn::CallFastAtan2(d, y, x),
                       -10.0 + static_cast<double>(in / 4) * (20.0 / 3.0),
                       -10.0 + static_cast<double>(in % 4) * (20.0 / 3.0))

struct RunBenchmarks {
  template <class T, class D>
  HWY_NOINLINE void operator()(T, D d) {
    auto run_fxn = [&](const char* fxn) {
      struct BenchmarkTable {
        const char* name;
        void (*func)(D);
      };

      static const BenchmarkTable table[] = {
#define V(NAME) {#NAME, &BenchCall##NAME<D>},
          HWY_MATH_BENCHMARKS(V)
#undef V
      };

      for (const auto& entry : table) {
        if (strcmp(fxn, entry.name) == 0) {
          entry.func(d);
          return;
        }
      }
    };

    if (hwy::g_fxn1) run_fxn(hwy::g_fxn1);
    if (hwy::g_fxn2) run_fxn(hwy::g_fxn2);
  }
};

HWY_NOINLINE void RunAllBenchmarks() {
  hn::ForFloat3264Types(hn::ForPartialVectors<RunBenchmarks>());
}

}  // namespace HWY_NAMESPACE
}  // namespace hwy
HWY_AFTER_NAMESPACE();

#if HWY_ONCE
namespace hwy {
const char* g_fxn1 = nullptr;
const char* g_fxn2 = nullptr;

namespace {
HWY_EXPORT(RunAllBenchmarks);
}  // namespace
}  // namespace hwy

int main(int argc, char** argv) {
  const char* fxn1 = nullptr;
  const char* fxn2 = nullptr;

  for (int i = 1; i < argc; ++i) {
    if (strncmp(argv[i], "--fxn1=", 7) == 0) {
      fxn1 = argv[i] + 7;
    } else if (strncmp(argv[i], "--fxn2=", 7) == 0) {
      fxn2 = argv[i] + 7;
    }
  }

  if ((fxn1 == nullptr) != (fxn2 == nullptr)) {
    fprintf(stderr,
            "Error: Either pass both --fxn1 and --fxn2, or pass neither to use "
            "defaults.\n");
    return 1;
  }

  if (fxn1 == nullptr && fxn2 == nullptr) {
    fxn1 = "FastExp";
    fxn2 = "Exp";
  }

  const char* valid_fxns[] = {
#define V(NAME) #NAME,
      HWY_MATH_BENCHMARKS(V)
#undef V
  };

  bool valid1 = false;
  bool valid2 = false;
  for (const char* v : valid_fxns) {
    if (strcmp(fxn1, v) == 0) valid1 = true;
    if (strcmp(fxn2, v) == 0) valid2 = true;
  }

  if (!valid1 || !valid2) {
    fprintf(stderr,
            "Error: One or both function names do not match any defined "
            "benchmark functions.\n");
    fprintf(stderr, "Valid functions are: ");
    const size_t num_valid = sizeof(valid_fxns) / sizeof(valid_fxns[0]);
    for (size_t i = 0; i < num_valid; ++i) {
      fprintf(stderr, "%s%s", valid_fxns[i],
              (i == num_valid - 1) ? ".\n" : ", ");
    }
    return 1;
  }

  hwy::g_fxn1 = fxn1;
  hwy::g_fxn2 = fxn2;

  HWY_DYNAMIC_DISPATCH(hwy::RunAllBenchmarks)();
  return 0;
}
#endif  // HWY_ONCE
