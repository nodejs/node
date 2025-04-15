// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --turbofan --no-always-turbofan

load('test/mjsunit/elements-kinds-helpers.js');

function plusOne(x) {
  return x + 1;
}
%PrepareFunctionForOptimization(plusOne);

function foo(a) {
  return a.map(plusOne);
}
%PrepareFunctionForOptimization(foo);

// First optimize the function normally.
const array = [1, 2, 3];
foo(array);
%OptimizeFunctionOnNextCall(foo);
foo(array);

assertOptimized(foo);

const array2 = [1, 2, 3];
MakeArrayDictionaryMode(array2, () => { return 0; });

// Having Array.prototype.map create a dictionary mode array will invalidate
// the array constructor inlining protector. However, foo will not be
// deoptimized, since it doesn't depend on the protector.
array2.map(plusOne);
assertOptimized(foo);

// But if a dictionary mode array is passed to foo, it will deopt.
foo(array2);
assertUnoptimized(foo);

// The next time foo is optimized, the feedback contains a dictionary mode
// array, so Array.prototype.map will not be lowered.
%OptimizeFunctionOnNextCall(foo);
foo(array);
assertOptimized(foo);
foo(array2);
assertOptimized(foo);
