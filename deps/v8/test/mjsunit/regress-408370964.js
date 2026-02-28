// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function f0(a1) {
  let v10 = Math.max((a1 | 0) * (2 ** 52), 4294967297);
  v10--;
  return v10 + v10;
}

%PrepareFunctionForOptimization(f0);
f0(0); // Addition will have AdditiveSafeInt feedback.
%OptimizeFunctionOnNextCall(f0);
f0(0); // Compile with have AdditiveSafeInt feedback.

function f1() {
  f0(1);
  return 1;
}
%PrepareFunctionForOptimization(f1);
f1(); // Since f0 is optimized, feedback is not updated.
%OptimizeFunctionOnNextCall(f1);
f1(); // Addition has the AdditiveSafeInt feedback, but the
      // static inputs suggest otherwise.
