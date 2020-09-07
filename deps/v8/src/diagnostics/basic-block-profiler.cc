// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/diagnostics/basic-block-profiler.h"

#include <algorithm>
#include <numeric>
#include <sstream>

#include "src/base/lazy-instance.h"
#include "src/heap/heap-inl.h"
#include "torque-generated/exported-class-definitions-tq-inl.h"

namespace v8 {
namespace internal {

DEFINE_LAZY_LEAKY_OBJECT_GETTER(BasicBlockProfiler, BasicBlockProfiler::Get)

BasicBlockProfilerData::BasicBlockProfilerData(size_t n_blocks)
    : block_rpo_numbers_(n_blocks), counts_(n_blocks, 0) {}

void BasicBlockProfilerData::SetCode(const std::ostringstream& os) {
  code_ = os.str();
}

void BasicBlockProfilerData::SetFunctionName(std::unique_ptr<char[]> name) {
  function_name_ = name.get();
}

void BasicBlockProfilerData::SetSchedule(const std::ostringstream& os) {
  schedule_ = os.str();
}

void BasicBlockProfilerData::SetBlockRpoNumber(size_t offset,
                                               int32_t block_rpo) {
  DCHECK(offset < n_blocks());
  block_rpo_numbers_[offset] = block_rpo;
}

void BasicBlockProfilerData::ResetCounts() {
  for (size_t i = 0; i < n_blocks(); ++i) {
    counts_[i] = 0;
  }
}

BasicBlockProfilerData* BasicBlockProfiler::NewData(size_t n_blocks) {
  base::MutexGuard lock(&data_list_mutex_);
  auto data = std::make_unique<BasicBlockProfilerData>(n_blocks);
  BasicBlockProfilerData* data_ptr = data.get();
  data_list_.push_back(std::move(data));
  return data_ptr;
}

namespace {
Handle<String> CopyStringToJSHeap(const std::string& source, Isolate* isolate) {
  return isolate->factory()->NewStringFromAsciiChecked(source.c_str(),
                                                       AllocationType::kOld);
}

// Size of entries in both block_rpo_numbers and counts.
constexpr int kBasicBlockSlotSize = kInt32Size;
}  // namespace

BasicBlockProfilerData::BasicBlockProfilerData(
    Handle<OnHeapBasicBlockProfilerData> js_heap_data, Isolate* isolate) {
  function_name_ = js_heap_data->name().ToCString().get();
  schedule_ = js_heap_data->schedule().ToCString().get();
  code_ = js_heap_data->code().ToCString().get();
  Handle<ByteArray> counts(js_heap_data->counts(), isolate);
  for (int i = 0; i < counts->length() / kBasicBlockSlotSize; ++i) {
    counts_.push_back(counts->get_uint32(i));
  }
  Handle<ByteArray> rpo_numbers(js_heap_data->block_rpo_numbers(), isolate);
  for (int i = 0; i < rpo_numbers->length() / kBasicBlockSlotSize; ++i) {
    block_rpo_numbers_.push_back(rpo_numbers->get_int(i));
  }
  CHECK_EQ(block_rpo_numbers_.size(), counts_.size());
}

Handle<OnHeapBasicBlockProfilerData> BasicBlockProfilerData::CopyToJSHeap(
    Isolate* isolate) {
  int array_size_in_bytes = static_cast<int>(n_blocks() * kBasicBlockSlotSize);
  CHECK(array_size_in_bytes >= 0 &&
        static_cast<size_t>(array_size_in_bytes) / kBasicBlockSlotSize ==
            n_blocks());  // Overflow
  Handle<ByteArray> block_rpo_numbers = isolate->factory()->NewByteArray(
      array_size_in_bytes, AllocationType::kOld);
  for (int i = 0; i < static_cast<int>(n_blocks()); ++i) {
    block_rpo_numbers->set_int(i, block_rpo_numbers_[i]);
  }
  Handle<ByteArray> counts = isolate->factory()->NewByteArray(
      array_size_in_bytes, AllocationType::kOld);
  for (int i = 0; i < static_cast<int>(n_blocks()); ++i) {
    counts->set_uint32(i, counts_[i]);
  }
  Handle<String> name = CopyStringToJSHeap(function_name_, isolate);
  Handle<String> schedule = CopyStringToJSHeap(schedule_, isolate);
  Handle<String> code = CopyStringToJSHeap(code_, isolate);

  return isolate->factory()->NewOnHeapBasicBlockProfilerData(
      block_rpo_numbers, counts, name, schedule, code, AllocationType::kOld);
}

void BasicBlockProfiler::ResetCounts(Isolate* isolate) {
  for (const auto& data : data_list_) {
    data->ResetCounts();
  }
  HandleScope scope(isolate);
  Handle<ArrayList> list(isolate->heap()->basic_block_profiling_data(),
                         isolate);
  for (int i = 0; i < list->Length(); ++i) {
    Handle<ByteArray> counts(
        OnHeapBasicBlockProfilerData::cast(list->Get(i)).counts(), isolate);
    for (int j = 0; j < counts->length() / kBasicBlockSlotSize; ++j) {
      counts->set_uint32(j, 0);
    }
  }
}

bool BasicBlockProfiler::HasData(Isolate* isolate) {
  return data_list_.size() > 0 ||
         isolate->heap()->basic_block_profiling_data().Length() > 0;
}

void BasicBlockProfiler::Print(std::ostream& os, Isolate* isolate) {
  os << "---- Start Profiling Data ----" << std::endl;
  for (const auto& data : data_list_) {
    os << *data;
  }
  HandleScope scope(isolate);
  Handle<ArrayList> list(isolate->heap()->basic_block_profiling_data(),
                         isolate);
  for (int i = 0; i < list->Length(); ++i) {
    BasicBlockProfilerData data(
        handle(OnHeapBasicBlockProfilerData::cast(list->Get(i)), isolate),
        isolate);
    os << data;
  }
  os << "---- End Profiling Data ----" << std::endl;
}

std::ostream& operator<<(std::ostream& os, const BasicBlockProfilerData& d) {
  int block_count_sum = std::accumulate(d.counts_.begin(), d.counts_.end(), 0);
  if (block_count_sum == 0) return os;
  const char* name = "unknown function";
  if (!d.function_name_.empty()) {
    name = d.function_name_.c_str();
  }
  if (!d.schedule_.empty()) {
    os << "schedule for " << name << " (B0 entered " << d.counts_[0]
       << " times)" << std::endl;
    os << d.schedule_.c_str() << std::endl;
  }
  os << "block counts for " << name << ":" << std::endl;
  std::vector<std::pair<int32_t, uint32_t>> pairs;
  pairs.reserve(d.n_blocks());
  for (size_t i = 0; i < d.n_blocks(); ++i) {
    pairs.push_back(std::make_pair(d.block_rpo_numbers_[i], d.counts_[i]));
  }
  std::sort(pairs.begin(), pairs.end(),
            [=](std::pair<int32_t, uint32_t> left,
                std::pair<int32_t, uint32_t> right) {
              if (right.second == left.second) return left.first < right.first;
              return right.second < left.second;
            });
  for (auto it : pairs) {
    if (it.second == 0) break;
    os << "block B" << it.first << " : " << it.second << std::endl;
  }
  os << std::endl;
  if (!d.code_.empty()) {
    os << d.code_.c_str() << std::endl;
  }
  return os;
}

}  // namespace internal
}  // namespace v8
