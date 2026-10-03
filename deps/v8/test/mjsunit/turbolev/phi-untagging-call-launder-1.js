// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan --maglev --turbolev

function bar(c) {
  c+c+c+c+c+c+c+c+c+c+c+c; // Prevent eager-inlining.
  return c ? 0x7ffffff1 : 42;
}

function foo(c) {
  let phi = bar(c);
  phi | 0;
  // The concatenation below will see that its input is a Smi (thanks to the
  // `|0` above that inserted a CheckedSmiUntag), but won't see that this input
  // is a Phi because `bar` isn't eagerly inlined. In the old phi untagging
  // system, this means that a UseRequiresSmi call would have been missing,
  // which would have lead to wrongly untagging {phi} to a non-smi Int32. Now,
  // it should deopt.
  return "p" + phi;
}

%PrepareFunctionForOptimization(bar);
%PrepareFunctionForOptimization(foo);
assertEquals("p42", foo(false));

%OptimizeFunctionOnNextCall(foo);
assertEquals("p42", foo(false));
assertOptimized(foo);

assertEquals("p2147483633", foo(true));
assertUnoptimized(foo);
