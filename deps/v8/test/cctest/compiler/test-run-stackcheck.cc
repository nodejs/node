// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#include "test/cctest/compiler/function-tester.h"

using namespace v8::internal;
using namespace v8::internal::compiler;

TEST(TerminateAtMethodEntry) {
  FunctionTester T("(function(a,b) { return 23; })");

  T.CheckCall(T.Val(23));
  T.isolate->stack_guard()->RequestTerminateExecution();
  T.CheckThrows(T.undefined(), T.undefined());
}
