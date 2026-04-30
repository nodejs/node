// Copyright 2026 The Abseil Authors.
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
#include <deque>
#include <string>
#include <type_traits>
#include <vector>

#include "absl/base/config.h"
#include "absl/base/internal/raw_logging.h"
#include "absl/flags/flag.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/any_span.h"
#include "absl/types/span.h"
#include "benchmark/benchmark.h"

ABSL_FLAG(uint64_t, benchmark_container_size, 2000,
          "The size of the containers to use for benchmarking.");

namespace absl {
ABSL_NAMESPACE_BEGIN
namespace {

//
// Benchmarks and supporting code follows.
//

template <typename T>
T MakeElement(int) {
  ABSL_RAW_LOG(FATAL, "Not implemented.");
}

template <>
std::string MakeElement<std::string>(int i) {
  return absl::StrCat(i, i, i);
}

template <>
int MakeElement<int>(int i) {
  return i;
}

template <typename Container>
Container MakeContainer() {
  const size_t container_size = absl::GetFlag(FLAGS_benchmark_container_size);
  Container c;
  for (size_t i = 0; i < container_size; ++i) {
    c.push_back(
        MakeElement<typename Container::value_type>(static_cast<int>(i)));
  }
  return c;
}

template <typename Container, typename SourceContainer>
Container MakePointerContainer(SourceContainer* source) {
  Container c;
  for (auto& value : *source) {
    c.push_back(&value);
  }
  return c;
}

//
// Sequential Access Benchmarks
//

// Control run, iterating original container.
template <typename Container>
void BM_Sequential(benchmark::State& state) {
  auto a = MakeContainer<Container>();
  benchmark::DoNotOptimize(a);
  for (auto _ : state) {
    for (auto& v : a) {
      benchmark::DoNotOptimize(v);
    }
  }
}

// Wraps the container with a Span.
template <typename Container>
void BM_Sequential_Span(benchmark::State& state) {
  auto container = MakeContainer<Container>();
  absl::Span<typename Container::value_type> array_span(container);
  benchmark::DoNotOptimize(array_span);
  for (auto _ : state) {
    for (auto& v : array_span) {
      benchmark::DoNotOptimize(v);
    }
  }
}

// Same as BM_Sequential_Span, but uses a container of pointers and
// dereferences it.
template <typename Container>
void BM_Sequential_DerefSpan(benchmark::State& state) {
  using T = typename std::remove_pointer<typename Container::value_type>::type;
  auto values = MakeContainer<std::vector<T>>();
  const auto container = MakePointerContainer<Container>(&values);
  absl::Span<T* const> array_span(container);
  benchmark::DoNotOptimize(array_span);
  for (auto _ : state) {
    for (const auto& v : array_span) {
      benchmark::DoNotOptimize(*v);
    }
  }
}

// Wraps a AnySpan<const T> around a Container. Sequentially reads the
// entire span.
template <typename Container>
void BM_Sequential_AnySpan(benchmark::State& state) {
  using T = typename Container::value_type;
  auto container = MakeContainer<Container>();
  AnySpan<T> span(container);
  benchmark::DoNotOptimize(span);
  for (auto _ : state) {
    for (T& s : span) {
      benchmark::DoNotOptimize(s);
    }
  }
}

// Same as BM_Sequential_AnySpan, but dereferences the elements.
template <typename Container>
void BM_Sequential_AnySpanDeref(benchmark::State& state) {
  using T = typename std::remove_pointer<typename Container::value_type>::type;
  auto values = MakeContainer<std::vector<T>>();
  auto container = MakePointerContainer<Container>(&values);
  AnySpan<T> span(container, any_span_transform::Deref());
  benchmark::DoNotOptimize(span);
  for (auto _ : state) {
    for (auto& s : span) {
      benchmark::DoNotOptimize(s);
    }
  }
}

//
// Random Access Benchmarks
//

// Control run, accessing the original container.
template <typename Container>
void BM_Random(benchmark::State& state) {
  auto a = MakeContainer<Container>();
  auto* b = &a;
  benchmark::DoNotOptimize(a);
  benchmark::DoNotOptimize(b);
  for (auto _ : state) {
    uint64_t index_seed = 0;
    for (size_t j = 0; j < a.size(); ++j) {
      index_seed += 5623;
      const uint64_t index = index_seed % a.size();
      benchmark::DoNotOptimize((*b)[index]);
    }
  }
}

// Wraps a Span around a Container. Randomly reads the span.
template <typename Container>
void BM_Random_Span(benchmark::State& state) {
  using T = typename Container::value_type;
  auto v = MakeContainer<Container>();
  absl::Span<T> array_span(v);
  benchmark::DoNotOptimize(array_span);
  for (auto _ : state) {
    uint64_t index_seed = 0;
    for (size_t j = 0; j < v.size(); ++j) {
      index_seed += 5623;
      const uint64_t index = index_seed % v.size();
      benchmark::DoNotOptimize(array_span[index]);
    }
  }
}

// Wraps a AnySpan around a Container. Randomly reads the span.
template <typename Container>
void BM_Random_AnySpan(benchmark::State& state) {
  using T = typename Container::value_type;
  auto container = MakeContainer<Container>();
  AnySpan<T> span(container);
  benchmark::DoNotOptimize(span);
  for (auto _ : state) {
    uint64_t index_seed = 0;
    for (size_t j = 0; j < container.size(); ++j) {
      index_seed += 5623;
      const uint64_t index = index_seed % span.size();
      benchmark::DoNotOptimize(span[index]);
    }
  }
}

// Same as BM_Random_AnySpan, but dereferences elements.
template <typename Container>
void BM_Random_AnySpanDeref(benchmark::State& state) {
  using T = typename std::remove_pointer<typename Container::value_type>::type;
  auto values = MakeContainer<std::vector<T>>();
  auto container = MakePointerContainer<Container>(&values);
  AnySpan<T> span(container, any_span_transform::Deref());
  benchmark::DoNotOptimize(span);
  for (auto _ : state) {
    uint64_t index_seed = 0;
    for (size_t j = 0; j < container.size(); ++j) {
      index_seed += 5623;
      const uint64_t index = index_seed % span.size();
      benchmark::DoNotOptimize(span[index]);
    }
  }
}

// Sequential access int.
BENCHMARK_TEMPLATE(BM_Sequential, std::vector<int>);
BENCHMARK_TEMPLATE(BM_Sequential, std::deque<int>);
BENCHMARK_TEMPLATE(BM_Sequential_Span, std::vector<int>);
BENCHMARK_TEMPLATE(BM_Sequential_AnySpan, std::vector<int>);
BENCHMARK_TEMPLATE(BM_Sequential_AnySpan, std::deque<int>);

// Sequential access string.
BENCHMARK_TEMPLATE(BM_Sequential, std::vector<std::string>);
BENCHMARK_TEMPLATE(BM_Sequential, std::deque<std::string>);
BENCHMARK_TEMPLATE(BM_Sequential_Span, std::vector<std::string>);
BENCHMARK_TEMPLATE(BM_Sequential_AnySpan, std::vector<std::string>);
BENCHMARK_TEMPLATE(BM_Sequential_AnySpan, std::deque<std::string>);
BENCHMARK_TEMPLATE(BM_Sequential_DerefSpan, std::vector<std::string*>);
BENCHMARK_TEMPLATE(BM_Sequential_AnySpanDeref, std::vector<std::string*>);

// Random access int.
BENCHMARK_TEMPLATE(BM_Random, std::vector<int>);
BENCHMARK_TEMPLATE(BM_Random, std::deque<int>);
BENCHMARK_TEMPLATE(BM_Random_Span, std::vector<int>);
BENCHMARK_TEMPLATE(BM_Random_AnySpan, std::vector<int>);
BENCHMARK_TEMPLATE(BM_Random_AnySpan, std::deque<int>);

// Random access string.
BENCHMARK_TEMPLATE(BM_Random, std::vector<std::string>);
BENCHMARK_TEMPLATE(BM_Random, std::deque<std::string>);
BENCHMARK_TEMPLATE(BM_Random_Span, std::vector<std::string>);
BENCHMARK_TEMPLATE(BM_Random_AnySpan, std::vector<std::string>);
BENCHMARK_TEMPLATE(BM_Random_AnySpan, std::deque<std::string>);
BENCHMARK_TEMPLATE(BM_Random_AnySpanDeref, std::vector<std::string*>);

}  // namespace
ABSL_NAMESPACE_END
}  // namespace absl
