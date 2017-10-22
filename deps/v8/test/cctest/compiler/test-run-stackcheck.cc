// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/isolate.h"
#include "test/cctest/compiler/function-tester.h"

namespace v8 {
namespace internal {
namespace compiler {

TEST(TerminateAtMethodEntry) {
  FunctionTester T("(function(a,b) { return 23; })");

  T.CheckCall(T.Val(23));
  T.isolate->stack_guard()->RequestTerminateExecution();
  T.CheckThrows(T.undefined(), T.undefined());
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
