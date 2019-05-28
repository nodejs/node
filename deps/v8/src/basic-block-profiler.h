// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BASIC_BLOCK_PROFILER_H_
#define V8_BASIC_BLOCK_PROFILER_H_

#include <iosfwd>
#include <list>
#include <string>
#include <vector>

#include "src/base/macros.h"
#include "src/base/platform/mutex.h"
#include "src/globals.h"

namespace v8 {
namespace internal {

class BasicBlockProfiler {
 public:
  class Data {
   public:
    size_t n_blocks() const { return n_blocks_; }
    const uint32_t* counts() const { return &counts_[0]; }

    void SetCode(std::ostringstream* os);
    void SetFunctionName(std::unique_ptr<char[]> name);
    void SetSchedule(std::ostringstream* os);
    void SetBlockRpoNumber(size_t offset, int32_t block_rpo);
    intptr_t GetCounterAddress(size_t offset);

   private:
    friend class BasicBlockProfiler;
    friend std::ostream& operator<<(std::ostream& os,
                                    const BasicBlockProfiler::Data& s);

    explicit Data(size_t n_blocks);
    ~Data() = default;

    V8_EXPORT_PRIVATE void ResetCounts();

    const size_t n_blocks_;
    std::vector<int32_t> block_rpo_numbers_;
    std::vector<uint32_t> counts_;
    std::string function_name_;
    std::string schedule_;
    std::string code_;
    DISALLOW_COPY_AND_ASSIGN(Data);
  };

  typedef std::list<Data*> DataList;

  BasicBlockProfiler() = default;
  ~BasicBlockProfiler();

  V8_EXPORT_PRIVATE static BasicBlockProfiler* Get();
  Data* NewData(size_t n_blocks);
  V8_EXPORT_PRIVATE void ResetCounts();

  const DataList* data_list() { return &data_list_; }

 private:
  friend V8_EXPORT_PRIVATE std::ostream& operator<<(
      std::ostream& os, const BasicBlockProfiler& s);

  DataList data_list_;
  base::Mutex data_list_mutex_;

  DISALLOW_COPY_AND_ASSIGN(BasicBlockProfiler);
};

V8_EXPORT_PRIVATE std::ostream& operator<<(std::ostream& os,
                                           const BasicBlockProfiler& s);
std::ostream& operator<<(std::ostream& os, const BasicBlockProfiler::Data& s);

}  // namespace internal
}  // namespace v8

#endif  // V8_BASIC_BLOCK_PROFILER_H_
