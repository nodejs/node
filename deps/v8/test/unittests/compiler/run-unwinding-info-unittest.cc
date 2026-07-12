// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/v8config.h"
#include "src/objects/foreign.h"

// Test enabled only on supported architectures.
#if V8_OS_LINUX &&                                                 \
    (defined(V8_TARGET_ARCH_X64) || defined(V8_TARGET_ARCH_ARM) || \
     defined(V8_TARGET_ARCH_ARM64))

#include "src/flags/flags.h"
#include "src/objects/objects-inl.h"
#include "src/objects/objects.h"
#include "test/unittests/compiler/function-tester.h"
#include "test/unittests/test-utils.h"

namespace v8::internal::compiler {

using RunUnwindingInfoTest = TestWithContext;

TEST_F(RunUnwindingInfoTest, RunUnwindingInfo) {
  v8_flags.perf_prof_unwinding_info = true;
  v8_flags.jit_fuzzing = true;

  FunctionTester tester(i_isolate(),
                        "(function (x) {\n"
                        "  function f(x) { return x*x; }\n"
                        "  return x > 0 ? x+1 : f(x);\n"
                        "})");

  for (int i = 0; i < 500; ++i) {
    tester.Call(tester.NewNumber(-1));
  }

  EXPECT_TRUE(tester.function->code(i_isolate())->has_unwinding_info());
}

// TODO(ssanfilippo) Build low-level graph and check that state is correctly
// restored in the following situation:
//
//                         +-----------------+
//                         |     no frame    |---+
//  check that a           +-----------------+   |
//  a noframe state        | construct frame |<--+
//  is restored here  -->  +-----------------+   |
//                         | construct frame |<--+
//                         +-----------------+
//
// Same for <construct>/<destruct>/<destruct> (a <construct> status is restored)

// TODO(ssanfilippo) Intentionally reach a BB with different initial states
// and check that the UnwindingInforWriter fails in debug mode:
//
//      +----------------+
//  +---|     State A    |
//  |   +----------------+
//  |   |  State B != A  |---+
//  |   +----------------+   |
//  +-->|  Failure here  |<--+
//      +----------------+

}  // namespace v8::internal::compiler

#endif
