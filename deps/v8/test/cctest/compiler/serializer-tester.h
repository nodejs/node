// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CCTEST_COMPILER_SERIALIZER_TESTER_H_
#define V8_CCTEST_COMPILER_SERIALIZER_TESTER_H_

#include "src/compiler/js-heap-broker.h"
#include "test/cctest/cctest.h"

namespace v8 {
namespace internal {
namespace compiler {

class ZoneStats;

// The purpose of this class is to provide testing facility for the
// SerializerForBackgroundCompilation class. On a high-level, it executes the
// following steps:
// 1. Wraps the provided source in an IIFE
// 2. Generates bytecode for the given source
// 3. Runs the bytecode which *must* return a function
// 4. Takes the returned function and optimizes it
// 5. The optimized function is accessible through `function()`
class SerializerTester : public HandleAndZoneScope {
 public:
  explicit SerializerTester(const char* source);

  JSFunctionRef function() const { return function_.value(); }
  JSHeapBroker* broker() const { return broker_; }
  Isolate* isolate() { return main_isolate(); }

 private:
  CanonicalHandleScope canonical_;
  base::Optional<JSFunctionRef> function_;
  JSHeapBroker* broker_ = nullptr;
};
}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_CCTEST_COMPILER_SERIALIZER_TESTER_H_
