// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_UNITTESTS_SETUP_ISOLATE_FOR_TESTS_H_
#define V8_UNITTESTS_SETUP_ISOLATE_FOR_TESTS_H_

#include "src/init/setup-isolate.h"

namespace v8 {
namespace internal {

// Almost identical to setup-isolate-full.cc. The difference is that while
// testing the embedded snapshot blob can be missing.
class SetupIsolateDelegateForTests : public SetupIsolateDelegate {
 public:
  SetupIsolateDelegateForTests() = default;

  bool SetupHeap(Isolate* isolate, bool create_heap_objects) override {
    if (!create_heap_objects) return true;
    return SetupHeapInternal(isolate);
  }

  void SetupBuiltins(Isolate* isolate, bool compile_builtins) override {
    if (!compile_builtins) return;
    SetupBuiltinsInternal(isolate, /* compute_builtins_effects */ false);
  }
};

}  // namespace internal
}  // namespace v8

#endif  // V8_UNITTESTS_SETUP_ISOLATE_FOR_TESTS_H_
