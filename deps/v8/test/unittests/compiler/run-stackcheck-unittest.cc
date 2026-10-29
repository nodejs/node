// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/execution/isolate.h"
#include "test/unittests/compiler/function-tester.h"
#include "test/unittests/test-utils.h"

namespace v8::internal::compiler {

using RunStackCheckTest = TestWithContext;

TEST_F(RunStackCheckTest, TerminateAtMethodEntry) {
  FunctionTester T(i_isolate(), "(function(a,b) { return 23; })");

  T.CheckCall(T.NewNumber(23));
  T.isolate->stack_guard()->RequestTerminateExecution();
  T.CheckThrows(T.undefined(), T.undefined());
}

}  // namespace v8::internal::compiler
