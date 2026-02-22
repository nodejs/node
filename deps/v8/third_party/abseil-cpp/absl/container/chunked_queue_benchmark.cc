// Copyright 2025 The Abseil Authors.
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
#include <forward_list>
#include <list>
#include <random>

#include "absl/container/chunked_queue.h"
#include "absl/random/random.h"
#include "absl/status/status.h"
#include "absl/strings/cord.h"
#include "benchmark/benchmark.h"

namespace {

// Queue implementation using std::forward_list. Used to benchmark
// absl::chunked_queue against another plausable implementation.
template <typename T>
class forward_list_queue {
 public:
  using iterator = typename std::forward_list<T>::iterator;

  forward_list_queue() = default;
  ~forward_list_queue() = default;

  template <typename... Args>
  void emplace_back(Args&&... args) {
    if (list_.empty()) {
      list_.emplace_front(std::forward<Args>(args)...);
      tail_ = list_.begin();
    } else {
      list_.emplace_after(tail_, std::forward<Args>(args)...);
      ++tail_;
    }
  }

  void push_back(const T& value) { emplace_back(value); }
  iterator begin() { return list_.begin(); }
  iterator end() { return list_.end(); }
  T& front() { return list_.front(); }
  const T& front() const { return list_.front(); }
  void pop_front() { list_.pop_front(); }
  bool empty() const { return list_.empty(); }
  void clear() { list_.clear(); }

 private:
  std::forward_list<T> list_;
  typename std::forward_list<T>::iterator tail_;
};

template <class T>
using Deque = std::deque<T>;
template <class T>
using List = std::list<T>;
template <class T>
using FwdList = forward_list_queue<T>;
template <class T>
using Chunked = absl::chunked_queue<T>;
template <class T>
using ExpChunked = absl::chunked_queue<T, 2, 64>;

class Element {
 public:
  Element() : Element(-1) {}
  Element(int type) : type_(type) {}      // NOLINT
  operator int() const { return type_; }  // NOLINT

 private:
  int type_;
  absl::Cord item_;
  absl::Status status_;
};

template <class Q>
Q MakeQueue(int64_t num_elements) {
  Q q;
  for (int64_t i = 0; i < num_elements; i++) {
    q.push_back(static_cast<int>(i));
  }
  return q;
}

void CustomArgs(benchmark::internal::Benchmark* b) {
  b->Arg(1 << 4);
  b->Arg(1 << 10);
  b->Arg(1 << 17);
}

template <class Q>
void BM_construct(benchmark::State& state) {
  for (auto s : state) {
    Q q;
    benchmark::DoNotOptimize(q);
  }
}

BENCHMARK_TEMPLATE(BM_construct, Deque<int64_t>);
BENCHMARK_TEMPLATE(BM_construct, List<int64_t>);
BENCHMARK_TEMPLATE(BM_construct, FwdList<int64_t>);
BENCHMARK_TEMPLATE(BM_construct, Chunked<int64_t>);
BENCHMARK_TEMPLATE(BM_construct, ExpChunked<int64_t>);
BENCHMARK_TEMPLATE(BM_construct, Deque<Element>);
BENCHMARK_TEMPLATE(BM_construct, List<Element>);
BENCHMARK_TEMPLATE(BM_construct, FwdList<Element>);
BENCHMARK_TEMPLATE(BM_construct, Chunked<Element>);
BENCHMARK_TEMPLATE(BM_construct, ExpChunked<Element>);

template <class Q>
void BM_destroy(benchmark::State& state) {
  const int64_t num_elements = state.range(0);

  for (auto s : state) {
    state.PauseTiming();
    {
      Q q = MakeQueue<Q>(num_elements);
      benchmark::DoNotOptimize(q);
      state.ResumeTiming();
    }
  }
  state.SetItemsProcessed(state.iterations() * num_elements);
}

BENCHMARK_TEMPLATE(BM_destroy, Deque<int64_t>)->Apply(CustomArgs);
BENCHMARK_TEMPLATE(BM_destroy, List<int64_t>)->Apply(CustomArgs);
BENCHMARK_TEMPLATE(BM_destroy, FwdList<int64_t>)->Apply(CustomArgs);
BENCHMARK_TEMPLATE(BM_destroy, Chunked<int64_t>)->Apply(CustomArgs);
BENCHMARK_TEMPLATE(BM_destroy, ExpChunked<int64_t>)->Apply(CustomArgs);
BENCHMARK_TEMPLATE(BM_destroy, Deque<Element>)->Apply(CustomArgs);
BENCHMARK_TEMPLATE(BM_destroy, List<Element>)->Apply(CustomArgs);
BENCHMARK_TEMPLATE(BM_destroy, FwdList<Element>)->Apply(CustomArgs);
BENCHMARK_TEMPLATE(BM_destroy, Chunked<Element>)->Apply(CustomArgs);
BENCHMARK_TEMPLATE(BM_destroy, ExpChunked<Element>)->Apply(CustomArgs);

template <class Q>
void BM_push_back(benchmark::State& state) {
  const int64_t num_elements = state.range(0);

  state.SetItemsProcessed(state.max_iterations * num_elements);
  for (auto s : state) {
    state.PauseTiming();
    Q q;
    state.ResumeTiming();
    for (int j = 0; j < num_elements; j++) q.push_back(j);
    benchmark::DoNotOptimize(q);
  }
}

BENCHMARK_TEMPLATE(BM_push_back, Deque<int64_t>)->Apply(CustomArgs);
BENCHMARK_TEMPLATE(BM_push_back, List<int64_t>)->Apply(CustomArgs);
BENCHMARK_TEMPLATE(BM_push_back, FwdList<int64_t>)->Apply(CustomArgs);
BENCHMARK_TEMPLATE(BM_push_back, Chunked<int64_t>)->Apply(CustomArgs);
BENCHMARK_TEMPLATE(BM_push_back, ExpChunked<int64_t>)->Apply(CustomArgs);
BENCHMARK_TEMPLATE(BM_push_back, Deque<Element>)->Apply(CustomArgs);
BENCHMARK_TEMPLATE(BM_push_back, List<Element>)->Apply(CustomArgs);
BENCHMARK_TEMPLATE(BM_push_back, FwdList<Element>)->Apply(CustomArgs);
BENCHMARK_TEMPLATE(BM_push_back, Chunked<Element>)->Apply(CustomArgs);
BENCHMARK_TEMPLATE(BM_push_back, ExpChunked<Element>)->Apply(CustomArgs);

template <class Q>
void BM_pop_front(benchmark::State& state) {
  const int64_t num_elements = state.range(0);

  state.SetItemsProcessed(state.max_iterations * num_elements);
  for (auto s : state) {
    state.PauseTiming();
    Q q = MakeQueue<Q>(num_elements);
    state.ResumeTiming();
    for (int j = 0; j < num_elements; j++) q.pop_front();
    benchmark::DoNotOptimize(q);
  }
}

BENCHMARK_TEMPLATE(BM_pop_front, Deque<int64_t>)->Apply(CustomArgs);
BENCHMARK_TEMPLATE(BM_pop_front, List<int64_t>)->Apply(CustomArgs);
BENCHMARK_TEMPLATE(BM_pop_front, FwdList<int64_t>)->Apply(CustomArgs);
BENCHMARK_TEMPLATE(BM_pop_front, Chunked<int64_t>)->Apply(CustomArgs);
BENCHMARK_TEMPLATE(BM_pop_front, ExpChunked<int64_t>)->Apply(CustomArgs);
BENCHMARK_TEMPLATE(BM_pop_front, Deque<Element>)->Apply(CustomArgs);
BENCHMARK_TEMPLATE(BM_pop_front, List<Element>)->Apply(CustomArgs);
BENCHMARK_TEMPLATE(BM_pop_front, FwdList<Element>)->Apply(CustomArgs);
BENCHMARK_TEMPLATE(BM_pop_front, Chunked<Element>)->Apply(CustomArgs);
BENCHMARK_TEMPLATE(BM_pop_front, ExpChunked<Element>)->Apply(CustomArgs);

template <class Q>
void BM_clear(benchmark::State& state) {
  const int64_t num_elements = state.range(0);

  state.SetItemsProcessed(state.max_iterations * num_elements);
  for (auto s : state) {
    state.PauseTiming();
    Q q = MakeQueue<Q>(num_elements);
    state.ResumeTiming();
    q.clear();
    benchmark::DoNotOptimize(q);
  }
}

BENCHMARK_TEMPLATE(BM_clear, Deque<int64_t>)->Apply(CustomArgs);
BENCHMARK_TEMPLATE(BM_clear, List<int64_t>)->Apply(CustomArgs);
BENCHMARK_TEMPLATE(BM_clear, FwdList<int64_t>)->Apply(CustomArgs);
BENCHMARK_TEMPLATE(BM_clear, Chunked<int64_t>)->Apply(CustomArgs);
BENCHMARK_TEMPLATE(BM_clear, ExpChunked<int64_t>)->Apply(CustomArgs);
BENCHMARK_TEMPLATE(BM_clear, Deque<Element>)->Apply(CustomArgs);
BENCHMARK_TEMPLATE(BM_clear, List<Element>)->Apply(CustomArgs);
BENCHMARK_TEMPLATE(BM_clear, FwdList<Element>)->Apply(CustomArgs);
BENCHMARK_TEMPLATE(BM_clear, Chunked<Element>)->Apply(CustomArgs);
BENCHMARK_TEMPLATE(BM_clear, ExpChunked<Element>)->Apply(CustomArgs);

template <class Q>
void BM_iter(benchmark::State& state) {
  const int64_t num_elements = state.range(0);

  state.SetItemsProcessed(state.max_iterations * num_elements);
  for (auto s : state) {
    state.PauseTiming();
    Q q = MakeQueue<Q>(state.max_iterations);
    int sum = 0;
    state.ResumeTiming();
    for (const auto& v : q) sum += v;
    benchmark::DoNotOptimize(sum);
  }
}

BENCHMARK_TEMPLATE(BM_iter, Deque<int64_t>)->Apply(CustomArgs);
BENCHMARK_TEMPLATE(BM_iter, List<int64_t>)->Apply(CustomArgs);
BENCHMARK_TEMPLATE(BM_iter, FwdList<int64_t>)->Apply(CustomArgs);
BENCHMARK_TEMPLATE(BM_iter, Chunked<int64_t>)->Apply(CustomArgs);
BENCHMARK_TEMPLATE(BM_iter, ExpChunked<int64_t>)->Apply(CustomArgs);
BENCHMARK_TEMPLATE(BM_iter, Deque<Element>)->Apply(CustomArgs);
BENCHMARK_TEMPLATE(BM_iter, List<Element>)->Apply(CustomArgs);
BENCHMARK_TEMPLATE(BM_iter, FwdList<Element>)->Apply(CustomArgs);
BENCHMARK_TEMPLATE(BM_iter, Chunked<Element>)->Apply(CustomArgs);
BENCHMARK_TEMPLATE(BM_iter, ExpChunked<Element>)->Apply(CustomArgs);

template <class Q>
void BM_resize_shrink(benchmark::State& state) {
  const int64_t num_elements = state.range(0);

  state.SetItemsProcessed(state.max_iterations * num_elements);
  for (auto s : state) {
    state.PauseTiming();
    Q q = MakeQueue<Q>(num_elements * 2);
    state.ResumeTiming();
    q.resize(num_elements);
    benchmark::DoNotOptimize(q);
  }
}

// FwdList does not support resize.
BENCHMARK_TEMPLATE(BM_resize_shrink, Deque<int64_t>)->Apply(CustomArgs);
BENCHMARK_TEMPLATE(BM_resize_shrink, List<int64_t>)->Apply(CustomArgs);
// BENCHMARK_TEMPLATE(BM_resize_shrink, FwdList<int64>)->Apply(CustomArgs);
BENCHMARK_TEMPLATE(BM_resize_shrink, Chunked<int64_t>)->Apply(CustomArgs);
BENCHMARK_TEMPLATE(BM_resize_shrink, ExpChunked<int64_t>)->Apply(CustomArgs);
BENCHMARK_TEMPLATE(BM_resize_shrink, Deque<Element>)->Apply(CustomArgs);
BENCHMARK_TEMPLATE(BM_resize_shrink, List<Element>)->Apply(CustomArgs);
// BENCHMARK_TEMPLATE(BM_resize_shrink, FwdList<Element>)->Apply(CustomArgs);
BENCHMARK_TEMPLATE(BM_resize_shrink, Chunked<Element>)->Apply(CustomArgs);
BENCHMARK_TEMPLATE(BM_resize_shrink, ExpChunked<Element>)->Apply(CustomArgs);

template <class Q>
void BM_resize_grow(benchmark::State& state) {
  const int64_t num_elements = state.range(0);

  state.SetItemsProcessed(state.max_iterations * num_elements);
  for (auto s : state) {
    state.PauseTiming();
    Q q = MakeQueue<Q>(num_elements);
    state.ResumeTiming();
    q.resize(static_cast<size_t>(num_elements) * 2);
    benchmark::DoNotOptimize(q);
  }
}

// FwdList does not support resize.
BENCHMARK_TEMPLATE(BM_resize_grow, Deque<int64_t>)->Apply(CustomArgs);
BENCHMARK_TEMPLATE(BM_resize_grow, List<int64_t>)->Apply(CustomArgs);
// BENCHMARK_TEMPLATE(BM_resize_grow, FwdList<int64>)->Apply(CustomArgs);
BENCHMARK_TEMPLATE(BM_resize_grow, Chunked<int64_t>)->Apply(CustomArgs);
BENCHMARK_TEMPLATE(BM_resize_grow, ExpChunked<int64_t>)->Apply(CustomArgs);
BENCHMARK_TEMPLATE(BM_resize_grow, Deque<Element>)->Apply(CustomArgs);
BENCHMARK_TEMPLATE(BM_resize_grow, List<Element>)->Apply(CustomArgs);
// BENCHMARK_TEMPLATE(BM_resize_grow, FwdList<Element>)->Apply(CustomArgs);
BENCHMARK_TEMPLATE(BM_resize_grow, Chunked<Element>)->Apply(CustomArgs);
BENCHMARK_TEMPLATE(BM_resize_grow, ExpChunked<Element>)->Apply(CustomArgs);

template <class Q>
void BM_assign_shrink(benchmark::State& state) {
  const int64_t num_elements = state.range(0);

  state.SetItemsProcessed(state.max_iterations * num_elements);
  for (auto s : state) {
    state.PauseTiming();
    const Q src = MakeQueue<Q>(num_elements);
    Q dst = MakeQueue<Q>(num_elements * 2);
    state.ResumeTiming();
    dst = src;
    benchmark::DoNotOptimize(dst);
  }
}

BENCHMARK_TEMPLATE(BM_assign_shrink, Deque<int64_t>)->Apply(CustomArgs);
BENCHMARK_TEMPLATE(BM_assign_shrink, List<int64_t>)->Apply(CustomArgs);
BENCHMARK_TEMPLATE(BM_assign_shrink, FwdList<int64_t>)->Apply(CustomArgs);
BENCHMARK_TEMPLATE(BM_assign_shrink, Chunked<int64_t>)->Apply(CustomArgs);
BENCHMARK_TEMPLATE(BM_assign_shrink, ExpChunked<int64_t>)->Apply(CustomArgs);
BENCHMARK_TEMPLATE(BM_assign_shrink, Deque<Element>)->Apply(CustomArgs);
BENCHMARK_TEMPLATE(BM_assign_shrink, List<Element>)->Apply(CustomArgs);
BENCHMARK_TEMPLATE(BM_assign_shrink, FwdList<Element>)->Apply(CustomArgs);
BENCHMARK_TEMPLATE(BM_assign_shrink, Chunked<Element>)->Apply(CustomArgs);
BENCHMARK_TEMPLATE(BM_assign_shrink, ExpChunked<Element>)->Apply(CustomArgs);

template <class Q>
void BM_assign_grow(benchmark::State& state) {
  const int64_t num_elements = state.range(0);

  state.SetItemsProcessed(state.max_iterations * num_elements);
  for (auto s : state) {
    state.PauseTiming();
    const Q src = MakeQueue<Q>(num_elements * 2);
    Q dst = MakeQueue<Q>(num_elements);
    state.ResumeTiming();
    dst = src;
    benchmark::DoNotOptimize(dst);
  }
}

BENCHMARK_TEMPLATE(BM_assign_grow, Deque<int64_t>)->Apply(CustomArgs);
BENCHMARK_TEMPLATE(BM_assign_grow, List<int64_t>)->Apply(CustomArgs);
BENCHMARK_TEMPLATE(BM_assign_grow, FwdList<int64_t>)->Apply(CustomArgs);
BENCHMARK_TEMPLATE(BM_assign_grow, Chunked<int64_t>)->Apply(CustomArgs);
BENCHMARK_TEMPLATE(BM_assign_grow, ExpChunked<int64_t>)->Apply(CustomArgs);
BENCHMARK_TEMPLATE(BM_assign_grow, Deque<Element>)->Apply(CustomArgs);
BENCHMARK_TEMPLATE(BM_assign_grow, List<Element>)->Apply(CustomArgs);
BENCHMARK_TEMPLATE(BM_assign_grow, FwdList<Element>)->Apply(CustomArgs);
BENCHMARK_TEMPLATE(BM_assign_grow, Chunked<Element>)->Apply(CustomArgs);
BENCHMARK_TEMPLATE(BM_assign_grow, ExpChunked<Element>)->Apply(CustomArgs);

template <class Q>
void BM_push_pop(benchmark::State& state) {
  const int64_t num_elements = state.range(0);

  state.SetItemsProcessed(state.max_iterations * num_elements);

  std::mt19937 rnd;
  for (auto s : state) {
    state.PauseTiming();
    Q q;
    state.ResumeTiming();
    for (int j = 0; j < num_elements; j++) {
      if (q.empty() || absl::Bernoulli(rnd, 0.5)) {
        q.push_back(state.iterations());
      } else {
        q.pop_front();
      }
    }
    benchmark::DoNotOptimize(q);
  }
}

BENCHMARK_TEMPLATE(BM_push_pop, Deque<int64_t>)->Apply(CustomArgs);
BENCHMARK_TEMPLATE(BM_push_pop, List<int64_t>)->Apply(CustomArgs);
BENCHMARK_TEMPLATE(BM_push_pop, FwdList<int64_t>)->Apply(CustomArgs);
BENCHMARK_TEMPLATE(BM_push_pop, Chunked<int64_t>)->Apply(CustomArgs);
BENCHMARK_TEMPLATE(BM_push_pop, ExpChunked<int64_t>)->Apply(CustomArgs);
BENCHMARK_TEMPLATE(BM_push_pop, Deque<Element>)->Apply(CustomArgs);
BENCHMARK_TEMPLATE(BM_push_pop, List<Element>)->Apply(CustomArgs);
BENCHMARK_TEMPLATE(BM_push_pop, FwdList<Element>)->Apply(CustomArgs);
BENCHMARK_TEMPLATE(BM_push_pop, Chunked<Element>)->Apply(CustomArgs);
BENCHMARK_TEMPLATE(BM_push_pop, ExpChunked<Element>)->Apply(CustomArgs);

}  // namespace
