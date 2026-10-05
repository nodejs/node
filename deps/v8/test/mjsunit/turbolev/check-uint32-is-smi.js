// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --turbofan

function test(str, x) {
  let start = x >>> 0;
  return str.startsWith("a", start);
}

%PrepareFunctionForOptimization(test);
test("abc", 0);
test("abc", 1);
%OptimizeFunctionOnNextCall(test);
test("abc", 0);
assertOptimized(test);

const kSmiMaxValue = 1073741823;
test("abc", kSmiMaxValue - 1);
assertOptimized(test);

// Smi::kMaxValue is a valid Smi and must not deopt.
test("abc", kSmiMaxValue);
assertOptimized(test);
