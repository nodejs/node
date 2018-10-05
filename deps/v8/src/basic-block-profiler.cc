// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/basic-block-profiler.h"

#include <algorithm>
#include <numeric>
#include <sstream>

#include "src/base/lazy-instance.h"

namespace v8 {
namespace internal {

namespace {
base::LazyInstance<BasicBlockProfiler>::type kBasicBlockProfiler =
    LAZY_INSTANCE_INITIALIZER;
}

BasicBlockProfiler* BasicBlockProfiler::Get() {
  return kBasicBlockProfiler.Pointer();
}

BasicBlockProfiler::Data::Data(size_t n_blocks)
    : n_blocks_(n_blocks),
      block_rpo_numbers_(n_blocks_),
      counts_(n_blocks_, 0) {}

static void InsertIntoString(std::ostringstream* os, std::string* string) {
  string->insert(0, os->str());
}

static void InsertIntoString(const char* data, std::string* string) {
  string->insert(0, data);
}

void BasicBlockProfiler::Data::SetCode(std::ostringstream* os) {
  InsertIntoString(os, &code_);
}

void BasicBlockProfiler::Data::SetFunctionName(std::unique_ptr<char[]> name) {
  InsertIntoString(name.get(), &function_name_);
}

void BasicBlockProfiler::Data::SetSchedule(std::ostringstream* os) {
  InsertIntoString(os, &schedule_);
}

void BasicBlockProfiler::Data::SetBlockRpoNumber(size_t offset,
                                                 int32_t block_rpo) {
  DCHECK(offset < n_blocks_);
  block_rpo_numbers_[offset] = block_rpo;
}

intptr_t BasicBlockProfiler::Data::GetCounterAddress(size_t offset) {
  DCHECK(offset < n_blocks_);
  return reinterpret_cast<intptr_t>(&(counts_[offset]));
}


void BasicBlockProfiler::Data::ResetCounts() {
  for (size_t i = 0; i < n_blocks_; ++i) {
    counts_[i] = 0;
  }
}

BasicBlockProfiler::Data* BasicBlockProfiler::NewData(size_t n_blocks) {
  base::LockGuard<base::Mutex> lock(&data_list_mutex_);
  Data* data = new Data(n_blocks);
  data_list_.push_back(data);
  return data;
}


BasicBlockProfiler::~BasicBlockProfiler() {
  for (DataList::iterator i = data_list_.begin(); i != data_list_.end(); ++i) {
    delete (*i);
  }
}


void BasicBlockProfiler::ResetCounts() {
  for (DataList::iterator i = data_list_.begin(); i != data_list_.end(); ++i) {
    (*i)->ResetCounts();
  }
}


std::ostream& operator<<(std::ostream& os, const BasicBlockProfiler& p) {
  os << "---- Start Profiling Data ----" << std::endl;
  typedef BasicBlockProfiler::DataList::const_iterator iterator;
  for (iterator i = p.data_list_.begin(); i != p.data_list_.end(); ++i) {
    os << **i;
  }
  os << "---- End Profiling Data ----" << std::endl;
  return os;
}


std::ostream& operator<<(std::ostream& os, const BasicBlockProfiler::Data& d) {
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
  pairs.reserve(d.n_blocks_);
  for (size_t i = 0; i < d.n_blocks_; ++i) {
    pairs.push_back(std::make_pair(d.block_rpo_numbers_[i], d.counts_[i]));
  }
  std::sort(pairs.begin(), pairs.end(),
            [=](std::pair<int32_t, uint32_t> left,
                std::pair<int32_t, uint32_t> right) {
              if (right.second == left.second)
                return left.first < right.first;
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
