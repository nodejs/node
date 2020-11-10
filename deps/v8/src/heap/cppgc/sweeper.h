// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_CPPGC_SWEEPER_H_
#define V8_HEAP_CPPGC_SWEEPER_H_

#include <memory>

#include "src/base/macros.h"

namespace cppgc {
namespace internal {

class RawHeap;

class V8_EXPORT_PRIVATE Sweeper final {
 public:
  enum class Config { kAtomic, kIncrementalAndConcurrent };

  explicit Sweeper(RawHeap*);
  ~Sweeper();

  Sweeper(const Sweeper&) = delete;
  Sweeper& operator=(const Sweeper&) = delete;

  void Start(Config);
  void Finish();

 private:
  class SweeperImpl;
  std::unique_ptr<SweeperImpl> impl_;
};

}  // namespace internal
}  // namespace cppgc

#endif  // V8_HEAP_CPPGC_SWEEPER_H_
