// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/cctest/setup-isolate-for-tests.h"

// Almost identical to setup-isolate-full.cc. The difference is that while
// testing the embedded snapshot blob can be missing.

namespace v8 {
namespace internal {

bool SetupIsolateDelegateForTests::SetupHeap(Isolate* isolate,
                                             bool create_heap_objects) {
  if (!create_heap_objects) return true;
  return SetupHeapInternal(isolate);
}

void SetupIsolateDelegateForTests::SetupBuiltins(Isolate* isolate,
                                                 bool compile_builtins) {
  if (!compile_builtins) return;
  SetupBuiltinsInternal(isolate);
}

}  // namespace internal
}  // namespace v8
