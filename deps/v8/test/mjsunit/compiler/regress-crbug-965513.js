// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Flags: --allow-natives-syntax --opt

%EnsureFeedbackVectorForFunction(foo);
function foo(x) {
  return x * (x == 1);
};
%PrepareFunctionForOptimization(foo);
foo(0.5);
foo(1.5);
%OptimizeFunctionOnNextCall(foo);
foo(1.5);
assertOptimized(foo);
