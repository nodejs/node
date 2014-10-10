// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/basic-block-profiler.h"

namespace v8 {
namespace internal {

BasicBlockProfiler::Data::Data(size_t n_blocks)
    : n_blocks_(n_blocks), block_ids_(n_blocks_, -1), counts_(n_blocks_, 0) {}


BasicBlockProfiler::Data::~Data() {}


static void InsertIntoString(OStringStream* os, std::string* string) {
  string->insert(string->begin(), os->c_str(), &os->c_str()[os->size()]);
}


void BasicBlockProfiler::Data::SetCode(OStringStream* os) {
  InsertIntoString(os, &code_);
}


void BasicBlockProfiler::Data::SetFunctionName(OStringStream* os) {
  InsertIntoString(os, &function_name_);
}


void BasicBlockProfiler::Data::SetSchedule(OStringStream* os) {
  InsertIntoString(os, &schedule_);
}


void BasicBlockProfiler::Data::SetBlockId(size_t offset, int block_id) {
  DCHECK(offset < n_blocks_);
  block_ids_[offset] = block_id;
}


uint32_t* BasicBlockProfiler::Data::GetCounterAddress(size_t offset) {
  DCHECK(offset < n_blocks_);
  return &counts_[offset];
}


void BasicBlockProfiler::Data::ResetCounts() {
  for (size_t i = 0; i < n_blocks_; ++i) {
    counts_[i] = 0;
  }
}


BasicBlockProfiler::BasicBlockProfiler() {}


BasicBlockProfiler::Data* BasicBlockProfiler::NewData(size_t n_blocks) {
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


OStream& operator<<(OStream& os, const BasicBlockProfiler& p) {
  os << "---- Start Profiling Data ----" << endl;
  typedef BasicBlockProfiler::DataList::const_iterator iterator;
  for (iterator i = p.data_list_.begin(); i != p.data_list_.end(); ++i) {
    os << **i;
  }
  os << "---- End Profiling Data ----" << endl;
  return os;
}


OStream& operator<<(OStream& os, const BasicBlockProfiler::Data& d) {
  const char* name = "unknown function";
  if (!d.function_name_.empty()) {
    name = d.function_name_.c_str();
  }
  if (!d.schedule_.empty()) {
    os << "schedule for " << name << endl;
    os << d.schedule_.c_str() << endl;
  }
  os << "block counts for " << name << ":" << endl;
  for (size_t i = 0; i < d.n_blocks_; ++i) {
    os << "block " << d.block_ids_[i] << " : " << d.counts_[i] << endl;
  }
  os << endl;
  if (!d.code_.empty()) {
    os << d.code_.c_str() << endl;
  }
  return os;
}

}  // namespace internal
}  // namespace v8
