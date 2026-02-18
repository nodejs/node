// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --track-array-buffer-views --turbofan

function foo(ta) {
  return ta.length;
}
function foo2(ta) {
  return ta.length;
}
%PrepareFunctionForOptimization(foo);
%PrepareFunctionForOptimization(foo2);

// Make the TA map instable to have more reliable tests.
{
  let ab = new ArrayBuffer(8);
  let ta = new Uint32Array(ab);
  %ArrayBufferDetach(ab);
}

// Check tracking works and we don't deopt.
{
  let ab = new ArrayBuffer(8);
  let ta = new Uint32Array(ab);
  assertEquals(2, foo(ta));
  let ab2 = new ArrayBuffer(8);
  let ta2 = new Uint32Array(ab2);
  assertEquals(2, foo2(ta2));

  %OptimizeFunctionOnNextCall(foo);
  %OptimizeFunctionOnNextCall(foo2);
  assertEquals(2, foo(ta));
  assertOptimized(foo);
  assertEquals(2, foo2(ta2));
  assertOptimized(foo2);

  %ArrayBufferDetach(ab);
  // foo might or might not be optimized here, depending on the dependency we
  // installed.
  assertEquals(0, foo(ta));
  // Bailout due to map change
  assertUnoptimized(foo);

  // If ab gets detached, we don't deopt unrelated code.
  assertOptimized(foo2);
  assertEquals(2, foo2(ta2));
  assertOptimized(foo2);
}


// Create another instance.
{
  let ab = new ArrayBuffer(8);
  let ta = new Uint32Array(ab);
  assertEquals(2, foo(ta));

  %OptimizeFunctionOnNextCall(foo);
  assertEquals(2, foo(ta));
  assertOptimized(foo);
}

// Invalidate the ArrayBufferDetached protector
(function() {
  const ab = new ArrayBuffer(8);
  let ta1 = new Uint32Array(ab);
  let ta2 = new Uint32Array(ab);
  %ArrayBufferDetach(ab);
})();

// Bailout due to protector
assertUnoptimized(foo);

// Check tracking works and we don't deopt.
{
  let ab = new ArrayBuffer(8);
  let ta = new Uint32Array(ab);
  assertEquals(2, foo(ta));
  let ab2 = new ArrayBuffer(8);
  let ta2 = new Uint32Array(ab2);
  assertEquals(2, foo2(ta2));

  %OptimizeFunctionOnNextCall(foo);
  %OptimizeFunctionOnNextCall(foo2);
  assertEquals(2, foo(ta));
  assertOptimized(foo);
  assertEquals(2, foo2(ta2));
  assertOptimized(foo2);

  // If ab gets detached, we don't deopt.
  %ArrayBufferDetach(ab);
  // foo might or might not be optimized here, depending on the dependency we
  // installed.
  assertEquals(0, foo(ta));
  // Might get a bailout due to map change. But depending on the compiler opts
  // we are polymorphic here.
  // assertUnoptimized(foo);

  // Other ab are unaffected
  assertOptimized(foo2);
  assertEquals(2, foo2(ta2));
  assertOptimized(foo2);
}

// Create another instance.
{
  let ab = new ArrayBuffer(8);
  let ta = new Uint32Array(ab);
  assertEquals(2, foo(ta));

  %OptimizeFunctionOnNextCall(foo);
  assertEquals(2, foo(ta));
  assertOptimized(foo);
}

// Again detach untracked view
(function() {
  const ab = new ArrayBuffer(8);
  let ta1 = new Uint32Array(ab);
  let ta2 = new Uint32Array(ab);
  %ArrayBufferDetach(ab);
})();

// No bailout since we are already detached
assertOptimized(foo);
