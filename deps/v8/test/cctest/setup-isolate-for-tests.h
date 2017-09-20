// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TEST_CCTEST_SETUP_ISOLATE_FOR_TESTS_H_
#define V8_TEST_CCTEST_SETUP_ISOLATE_FOR_TESTS_H_

#include "src/setup-isolate.h"

namespace v8 {
namespace internal {

class SetupIsolateDelegateForTests : public SetupIsolateDelegate {
 public:
  SetupIsolateDelegateForTests() : SetupIsolateDelegate() {}
  virtual ~SetupIsolateDelegateForTests() {}

  void SetupBuiltins(Isolate* isolate, bool create_heap_objects) override;

  void SetupInterpreter(interpreter::Interpreter* interpreter,
                        bool create_heap_objects) override;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_TEST_CCTEST_SETUP_ISOLATE_FOR_TESTS_H_
