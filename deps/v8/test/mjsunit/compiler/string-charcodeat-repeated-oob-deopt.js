// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan

// An out-of-bounds charCodeAt deopt only disables bounds check speculation for
// the call site. A second such deopt from stale code that inlined the same call
// site must not disable speculation (and thus inlining) altogether.

function charCodeAt(s, i) {
  return s.charCodeAt(i);
}
function a(s, i) {
  return charCodeAt(s, i);
}
function b(s, i) {
  return charCodeAt(s, i);
}

%PrepareFunctionForOptimization(charCodeAt);
%PrepareFunctionForOptimization(a);
%PrepareFunctionForOptimization(b);
assertEquals(97, a('abc', 0));
assertEquals(98, b('abc', 1));
%OptimizeFunctionOnNextCall(a);
assertEquals(97, a('abc', 0));
%OptimizeFunctionOnNextCall(b);
assertEquals(98, b('abc', 1));
assertOptimized(a);
assertOptimized(b);

// Both a and b deopt on the same call site.
assertEquals(NaN, a('abc', 3));
assertUnoptimized(a);
assertEquals(NaN, b('abc', 3));
assertUnoptimized(b);

// The call site is still inlined, with an in-bounds check that does not
// deopt; a non-Smi index is a deopt that a non-inlined call would not have.
%PrepareFunctionForOptimization(a);
%OptimizeFunctionOnNextCall(a);
assertEquals(NaN, a('abc', 3));
assertOptimized(a);
assertEquals(98, a('abc', 1.5));
assertUnoptimized(a);
