// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_UNITTESTS_HEAP_HEAP_UTILS_H_
#define V8_UNITTESTS_HEAP_HEAP_UTILS_H_

#include "src/base/macros.h"
#include "src/common/globals.h"
#include "test/unittests/test-utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace internal {

template <typename TMixin>
class WithHeapInternals : public TMixin {
 public:
  WithHeapInternals() = default;

  void CollectGarbage(i::AllocationSpace space) {
    heap()->CollectGarbage(space, i::GarbageCollectionReason::kTesting);
  }

  Heap* heap() const { return this->i_isolate()->heap(); }

 private:
  DISALLOW_COPY_AND_ASSIGN(WithHeapInternals);
};

using TestWithHeapInternals =       //
    WithHeapInternals<              //
        WithInternalIsolateMixin<   //
            WithIsolateScopeMixin<  //
                WithIsolateMixin<   //
                    ::testing::Test>>>>;

}  // namespace internal
}  // namespace v8

#endif  // V8_UNITTESTS_HEAP_HEAP_UTILS_H_
