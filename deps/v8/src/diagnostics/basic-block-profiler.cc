// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/diagnostics/basic-block-profiler.h"

#include <algorithm>
#include <numeric>
#include <sstream>

#include "src/base/lazy-instance.h"
#include "src/builtins/profile-data-reader.h"
#include "src/heap/heap-inl.h"
#include "src/objects/shared-function-info-inl.h"

namespace v8 {
namespace internal {

DEFINE_LAZY_LEAKY_OBJECT_GETTER(BasicBlockProfiler, BasicBlockProfiler::Get)
DEFINE_LAZY_LEAKY_OBJECT_GETTER(BuiltinsCallGraph, BuiltinsCallGraph::Get)

BasicBlockProfilerData::BasicBlockProfilerData(size_t n_blocks)
    : block_ids_(n_blocks), counts_(n_blocks, 0) {}

void BasicBlockProfilerData::SetCode(const std::ostringstream& os) {
  code_ = os.str();
}

void BasicBlockProfilerData::SetFunctionName(std::unique_ptr<char[]> name) {
  function_name_ = name.get();
}

void BasicBlockProfilerData::SetSchedule(const std::ostringstream& os) {
  schedule_ = os.str();
}

void BasicBlockProfilerData::SetBlockId(size_t offset, int32_t id) {
  DCHECK(offset < n_blocks());
  block_ids_[offset] = id;
}

void BasicBlockProfilerData::SetHash(int hash) { hash_ = hash; }

void BasicBlockProfilerData::ResetCounts() {
  for (size_t i = 0; i < n_blocks(); ++i) {
    counts_[i] = 0;
  }
}

void BasicBlockProfilerData::AddBranch(int32_t true_block_id,
                                       int32_t false_block_id) {
  branches_.emplace_back(true_block_id, false_block_id);
}

BasicBlockProfilerData* BasicBlockProfiler::NewData(size_t n_blocks) {
  base::MutexGuard lock(&data_list_mutex_);
  auto data = std::make_unique<BasicBlockProfilerData>(n_blocks);
  BasicBlockProfilerData* data_ptr = data.get();
  data_list_.push_back(std::move(data));
  return data_ptr;
}

namespace {
DirectHandle<String> CopyStringToJSHeap(const std::string& source,
                                        Isolate* isolate) {
  return isolate->factory()->NewStringFromAsciiChecked(source.c_str(),
                                                       AllocationType::kOld);
}
}  // namespace

BasicBlockProfilerData::BasicBlockProfilerData(
    DirectHandle<OnHeapBasicBlockProfilerData> js_heap_data, Isolate* isolate) {
  DisallowHeapAllocation no_gc;
  CopyFromJSHeap(*js_heap_data);
}

BasicBlockProfilerData::BasicBlockProfilerData(
    Tagged<OnHeapBasicBlockProfilerData> js_heap_data) {
  CopyFromJSHeap(js_heap_data);
}

void BasicBlockProfilerData::CopyFromJSHeap(
    Tagged<OnHeapBasicBlockProfilerData> js_heap_data) {
  function_name_ = js_heap_data->name()->ToStdString();
  schedule_ = js_heap_data->schedule()->ToStdString();
  code_ = js_heap_data->code()->ToStdString();
  Tagged<FixedUInt32Array> counts =
      Cast<FixedUInt32Array>(js_heap_data->counts());
  const uint32_t counts_len = counts->length().value();
  for (uint32_t i = 0; i < counts_len; ++i) {
    counts_.push_back(counts->get(i));
  }
  Tagged<FixedInt32Array> block_ids(js_heap_data->block_ids());
  const uint32_t blocks_ids_len = block_ids->length().value();
  for (uint32_t i = 0; i < blocks_ids_len; ++i) {
    block_ids_.push_back(block_ids->get(i));
  }
  Tagged<PodArray<std::pair<int32_t, int32_t>>> branches =
      js_heap_data->branches();
  const uint32_t branches_len = branches->length().value();
  for (uint32_t i = 0; i < branches_len; ++i) {
    branches_.push_back(branches->get(i));
  }
  CHECK_EQ(block_ids_.size(), counts_.size());
  hash_ = js_heap_data->hash();
}

DirectHandle<OnHeapBasicBlockProfilerData> BasicBlockProfilerData::CopyToJSHeap(
    Isolate* isolate) {
  const uint32_t blocks_count = static_cast<uint32_t>(n_blocks());
  DirectHandle<FixedInt32Array> block_ids =
      FixedInt32Array::New(isolate, blocks_count, AllocationType::kOld);
  for (uint32_t i = 0; i < blocks_count; ++i) {
    block_ids->set(i, block_ids_[i]);
  }

  DirectHandle<FixedUInt32Array> counts =
      FixedUInt32Array::New(isolate, blocks_count, AllocationType::kOld);
  for (uint32_t i = 0; i < blocks_count; ++i) {
    counts->set(i, counts_[i]);
  }

  const uint32_t branches_size = static_cast<uint32_t>(branches_.size());
  DirectHandle<PodArray<std::pair<int32_t, int32_t>>> branches =
      PodArray<std::pair<int32_t, int32_t>>::New(isolate, branches_size,
                                                 AllocationType::kOld);
  for (uint32_t i = 0; i < branches_size; ++i) {
    branches->set(i, branches_[i]);
  }
  DirectHandle<String> name = CopyStringToJSHeap(function_name_, isolate);
  DirectHandle<String> schedule = CopyStringToJSHeap(schedule_, isolate);
  DirectHandle<String> code = CopyStringToJSHeap(code_, isolate);

  return isolate->factory()->NewOnHeapBasicBlockProfilerData(
      block_ids, counts, branches, name, schedule, code, hash_,
      AllocationType::kOld);
}

void BasicBlockProfiler::ResetCounts(Isolate* isolate) {
  for (const auto& data : data_list_) {
    data->ResetCounts();
  }
  HandleScope scope(isolate);
  DirectHandle<ArrayList> list(isolate->heap()->basic_block_profiling_data(),
                               isolate);
  const uint32_t list_length = list->ulength().value();
  for (uint32_t i = 0; i < list_length; ++i) {
    DirectHandle<FixedUInt32Array> counts(
        Cast<OnHeapBasicBlockProfilerData>(list->get(i))->counts(), isolate);
    const uint32_t counts_len = counts->length().value();
    for (uint32_t j = 0; j < counts_len; ++j) {
      counts->set(j, 0);
    }
  }
}

bool BasicBlockProfiler::HasData(Isolate* isolate) {
  return !data_list_.empty() ||
         isolate->heap()->basic_block_profiling_data()->ulength().value() > 0;
}

void BasicBlockProfiler::Print(Isolate* isolate, std::ostream& os) {
  os << "---- Start Profiling Data ----" << '\n';
  for (const auto& data : data_list_) {
    os << *data;
  }
  HandleScope scope(isolate);
  DirectHandle<ArrayList> list(isolate->heap()->basic_block_profiling_data(),
                               isolate);
  const uint32_t list_length = list->ulength().value();
  std::unordered_set<std::string> builtin_names;
  for (uint32_t i = 0; i < list_length; ++i) {
    BasicBlockProfilerData data(
        direct_handle(Cast<OnHeapBasicBlockProfilerData>(list->get(i)),
                      isolate),
        isolate);
    os << data;
    // Ensure that all builtin names are unique; otherwise profile-guided
    // optimization might get confused.
    CHECK(builtin_names.insert(data.function_name_).second);
  }
  os << "---- End Profiling Data ----" << '\n';
}

void BasicBlockProfiler::Log(Isolate* isolate, std::ostream& os) {
  HandleScope scope(isolate);
  DirectHandle<ArrayList> list(isolate->heap()->basic_block_profiling_data(),
                               isolate);
  const uint32_t list_length = list->ulength().value();
  std::unordered_set<std::string> builtin_names;
  for (uint32_t i = 0; i < list_length; ++i) {
    BasicBlockProfilerData data(
        direct_handle(Cast<OnHeapBasicBlockProfilerData>(list->get(i)),
                      isolate),
        isolate);
    data.Log(isolate, os);
    // Ensure that all builtin names are unique; otherwise profile-guided
    // optimization might get confused.
    CHECK(builtin_names.insert(data.function_name_).second);
  }
}

std::vector<bool> BasicBlockProfiler::GetCoverageBitmap(Isolate* isolate) {
  DisallowGarbageCollection no_gc;
  Tagged<ArrayList> list(isolate->heap()->basic_block_profiling_data());
  std::vector<bool> out;
  const uint32_t list_length = list->ulength().value();
  for (uint32_t i = 0; i < list_length; ++i) {
    BasicBlockProfilerData data(
        Cast<OnHeapBasicBlockProfilerData>(list->get(i)));
    for (size_t j = 0; j < data.n_blocks(); ++j) {
      out.push_back(data.counts_[j] > 0);
    }
  }
  return out;
}

void BasicBlockProfilerData::Log(Isolate* isolate, std::ostream& os) {
  bool any_nonzero_counter = false;
  constexpr char kNext[] = "\t";
  for (size_t i = 0; i < n_blocks(); ++i) {
    if (counts_[i] > 0) {
      any_nonzero_counter = true;
      os << ProfileDataFromFileConstants::kBlockCounterMarker << kNext
         << function_name_.c_str() << kNext << block_ids_[i] << kNext
         << counts_[i] << '\n';
    }
  }
  if (any_nonzero_counter) {
    for (size_t i = 0; i < branches_.size(); ++i) {
      os << ProfileDataFromFileConstants::kBlockHintMarker << kNext
         << function_name_.c_str() << kNext << branches_[i].first << kNext
         << branches_[i].second << '\n';
    }
    os << ProfileDataFromFileConstants::kBuiltinHashMarker << kNext
       << function_name_.c_str() << kNext << hash_ << '\n';
  }
}

std::ostream& operator<<(std::ostream& os, const BasicBlockProfilerData& d) {
  if (std::all_of(d.counts_.cbegin(), d.counts_.cend(),
                  [](uint32_t count) { return count == 0; })) {
    // No data was collected for this function.
    return os;
  }
  const char* name = "unknown function";
  if (!d.function_name_.empty()) {
    name = d.function_name_.c_str();
  }
  if (!d.schedule_.empty()) {
    os << "schedule for " << name << " (B0 entered " << d.counts_[0]
       << " times)" << '\n';
    os << d.schedule_.c_str() << '\n';
  }
  os << "block counts for " << name << ":" << '\n';
  std::vector<std::pair<size_t, uint32_t>> pairs;
  pairs.reserve(d.n_blocks());
  for (size_t i = 0; i < d.n_blocks(); ++i) {
    pairs.push_back(std::make_pair(i, d.counts_[i]));
  }
  std::sort(
      pairs.begin(), pairs.end(),
      [=](std::pair<size_t, uint32_t> left, std::pair<size_t, uint32_t> right) {
        if (right.second == left.second) return left.first < right.first;
        return right.second < left.second;
      });
  for (auto it : pairs) {
    if (it.second == 0) break;
    os << "block B" << it.first << " : " << it.second << '\n';
  }
  os << '\n';
  if (!d.code_.empty()) {
    os << d.code_.c_str() << '\n';
  }
  return os;
}

BuiltinsCallGraph::BuiltinsCallGraph() : all_hash_matched_(true) {}

void BuiltinsCallGraph::AddBuiltinCall(Builtin caller, Builtin callee,
                                       int32_t block_id) {
  if (builtin_call_map_.count(caller) == 0) {
    builtin_call_map_.emplace(caller, BuiltinCallees());
  }
  BuiltinCallees& callees = builtin_call_map_.at(caller);
  if (callees.count(block_id) == 0) {
    callees.emplace(block_id, BlockCallees());
  }
  BlockCallees& block_callees = callees.at(block_id);
  if (block_callees.count(callee) == 0) {
    block_callees.emplace(callee);
  }
}

const BuiltinCallees* BuiltinsCallGraph::GetBuiltinCallees(Builtin builtin) {
  if (builtin_call_map_.count(builtin) == 0) return nullptr;
  return &builtin_call_map_.at(builtin);
}

}  // namespace internal
}  // namespace v8
