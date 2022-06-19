// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CCTEST_COMPILER_SERIALIZER_TESTER_H_
#define V8_CCTEST_COMPILER_SERIALIZER_TESTER_H_

#include <memory>

#include "src/compiler/js-heap-broker.h"
#include "test/cctest/cctest.h"

namespace v8 {
namespace internal {
namespace compiler {

class ZoneStats;

// The purpose of this class is to provide testing facility for the
// SerializerForBackgroundCompilation class. On a high-level, it executes the
// code given by {global_source} at global scope and then performs the following
// steps for {local_source}:
// 1. Wraps it in an IIFE.
// 2. Generates bytecode and runs it, which *must* return a function.
// 3. Takes the returned function and optimizes it.
// 4. The optimized function is accessible through `function()`.
class SerializerTester : public HandleAndZoneScope {
 public:
  explicit SerializerTester(const char* global_source,
                            const char* local_source);

  JSFunctionRef function() const { return function_.value(); }
  JSHeapBroker* broker() const { return broker_.get(); }
  Isolate* isolate() { return main_isolate(); }

 private:
  CanonicalHandleScope canonical_;
  base::Optional<JSFunctionRef> function_;
  std::unique_ptr<JSHeapBroker> broker_;
};
}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_CCTEST_COMPILER_SERIALIZER_TESTER_H_
