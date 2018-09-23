// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_PREPROCESS_LIVE_RANGES_H_
#define V8_PREPROCESS_LIVE_RANGES_H_

#include "src/compiler/register-allocator.h"

namespace v8 {
namespace internal {
namespace compiler {


class PreprocessLiveRanges final {
 public:
  PreprocessLiveRanges(RegisterAllocationData* data, Zone* zone)
      : data_(data), zone_(zone) {}
  void PreprocessRanges();

 private:
  void SplitRangesAroundDeferredBlocks();

  RegisterAllocationData* data() { return data_; }
  Zone* zone() { return zone_; }

  RegisterAllocationData* data_;
  Zone* zone_;
};


}  // namespace compiler
}  // namespace internal
}  // namespace v8
#endif  // V8_PREPROCESS_LIVE_RANGES_H_
