// Copyright 2020 The Abseil Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <cstddef>
#include <cstdint>
#include <string>

#include "absl/strings/str_cat.h"
#include "absl/strings/substitute.h"
#include "benchmark/benchmark.h"

namespace {

void BM_Substitute(benchmark::State& state) {
  std::string s(state.range(0), 'x');
  int64_t bytes = 0;
  for (auto _ : state) {
    std::string result = absl::Substitute("$0 $1 $2 $3 $4 $5 $6 $7 $8 $9 $$", s,
                                          s, s, s, s, s, s, s, s, s);
    bytes += result.size();
  }
  state.SetBytesProcessed(bytes);
}
BENCHMARK(BM_Substitute)->Range(0, 1024);

// Like BM_Substitute, but use char* strings (which must then be copied
// to STL strings) for all parameters.  This demonstrates that it is faster
// to use absl::Substitute() even if your inputs are char* strings.
void BM_SubstituteCstr(benchmark::State& state) {
  std::string s(state.range(0), 'x');
  int64_t bytes = 0;
  for (auto _ : state) {
    std::string result =
        absl::Substitute("$0 $1 $2 $3 $4 $5 $6 $7 $8 $9 $$", s.c_str(),
                         s.c_str(), s.c_str(), s.c_str(), s.c_str(), s.c_str(),
                         s.c_str(), s.c_str(), s.c_str(), s.c_str());
    bytes += result.size();
  }
  state.SetBytesProcessed(bytes);
}
BENCHMARK(BM_SubstituteCstr)->Range(0, 1024);

// For comparison with BM_Substitute.
void BM_StringPrintf(benchmark::State& state) {
  std::string s(state.range(0), 'x');
  int64_t bytes = 0;
  for (auto _ : state) {
    std::string result = absl::StrCat(s, " ", s, " ", s, " ", s, " ", s, " ", s,
                                      " ", s, " ", s, " ", s, " ", s);
    bytes += result.size();
  }
  state.SetBytesProcessed(bytes);
}
BENCHMARK(BM_StringPrintf)->Range(0, 1024);

// Benchmark using absl::Substitute() together with SimpleItoa() to print
// numbers.  This demonstrates that absl::Substitute() is faster than
// StringPrintf() even when the inputs are numbers.
void BM_SubstituteNumber(benchmark::State& state) {
  const int n = state.range(0);
  int64_t bytes = 0;
  for (auto _ : state) {
    std::string result =
        absl::Substitute("$0 $1 $2 $3 $4 $5 $6 $7 $8 $9 $$", n, n + 1, n + 2,
                         n + 3, n + 4, n + 5, n + 6, n + 7, n + 8, n + 9);
    bytes += result.size();
  }
  state.SetBytesProcessed(bytes);
}
BENCHMARK(BM_SubstituteNumber)->Arg(0)->Arg(1 << 20);

// For comparison with BM_SubstituteNumber.
void BM_StrCatNumber(benchmark::State& state) {
  const int n = state.range(0);
  int64_t bytes = 0;
  for (auto _ : state) {
    std::string result =
        absl::StrCat(n, " ", n + 1, " ", n + 2, " ", n + 3, " ", n + 4, " ",
                     n + 5, " ", n + 6, " ", n + 7, " ", n + 8, " ", n + 9);
    bytes += result.size();
  }
  state.SetBytesProcessed(bytes);
}
BENCHMARK(BM_StrCatNumber)->Arg(0)->Arg(1 << 20);

// Benchmark using absl::Substitute() with a single substitution, to test the
// speed at which it copies simple text.  Even in this case, it's faster
// that StringPrintf().
void BM_SubstituteSimpleText(benchmark::State& state) {
  std::string s(state.range(0), 'x');
  int64_t bytes = 0;
  for (auto _ : state) {
    std::string result = absl::Substitute("$0", s.c_str());
    bytes += result.size();
  }
  state.SetBytesProcessed(bytes);
}
BENCHMARK(BM_SubstituteSimpleText)->Range(0, 1024);

// For comparison with BM_SubstituteSimpleText.
void BM_StrCatSimpleText(benchmark::State& state) {
  std::string s(state.range(0), 'x');
  int64_t bytes = 0;
  for (auto _ : state) {
    std::string result = absl::StrCat("", s);
    bytes += result.size();
  }
  state.SetBytesProcessed(bytes);
}
BENCHMARK(BM_StrCatSimpleText)->Range(0, 1024);

std::string MakeFormatByDensity(int density, bool subs_mode) {
  std::string format((2 + density) * 10, 'x');
  for (size_t i = 0; i < 10; ++i) {
    format[density * i] = subs_mode ? '$' : '%';
    format[density * i + 1] = subs_mode ? ('0' + i) : 's';
  }
  return format;
}

void BM_SubstituteDensity(benchmark::State& state) {
  const std::string s(10, 'x');
  const std::string format = MakeFormatByDensity(state.range(0), true);
  int64_t bytes = 0;
  for (auto _ : state) {
    std::string result = absl::Substitute(format, s, s, s, s, s, s, s, s, s, s);
    bytes += result.size();
  }
  state.SetBytesProcessed(bytes);
}
BENCHMARK(BM_SubstituteDensity)->Range(0, 256);

void BM_StrCatDensity(benchmark::State& state) {
  const std::string s(10, 'x');
  const std::string format(state.range(0), 'x');
  int64_t bytes = 0;
  for (auto _ : state) {
    std::string result =
        absl::StrCat(s, format, s, format, s, format, s, format, s, format, s,
                     format, s, format, s, format, s, format, s, format);
    bytes += result.size();
  }
  state.SetBytesProcessed(bytes);
}
BENCHMARK(BM_StrCatDensity)->Range(0, 256);

}  // namespace
