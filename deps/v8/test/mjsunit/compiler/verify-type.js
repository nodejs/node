// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --no-always-turbofan --no-concurrent-recompilation

function foo(x) {
  return %VerifyType(x * x);
}

%PrepareFunctionForOptimization(foo);
foo(42);
%OptimizeFunctionOnNextCall(foo);
foo(42);
