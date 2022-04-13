// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --trace-turbo-inlining
// Flags: --max-inlined-bytecode-size-small=0

function f() {}
function g() {}
function h() {}

function test(n) {
  h;
  (n == 0 ? f : (n > 0 ? g : h))();
}

%EnsureFeedbackVectorForFunction(f);
%EnsureFeedbackVectorForFunction(g);

%PrepareFunctionForOptimization(test);
test(0);
test(1);
%OptimizeFunctionOnNextCall(test);
test(0);
