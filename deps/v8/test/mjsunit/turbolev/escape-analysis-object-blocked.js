// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --turbolev-escape-analysis
// Flags: --turbofan

// The object will escape ==> we cannot elide it.

function baz(deopt) {
  if (deopt) {
    bar.arguments[0].x = 17;
  }
}
%NeverOptimizeFunction(baz);

function bar(o, deopt) {
  o.x = 12;
  baz(deopt);
}

function foo(arg, deopt) {
  let o = { x : arg };
  bar(o, deopt);
  return o;
}

%PrepareFunctionForOptimization(bar);
%PrepareFunctionForOptimization(foo);
assertEquals({ x : 12 }, foo(42, false));
assertEquals({ x : 12 }, foo(42, false));

%OptimizeFunctionOnNextCall(foo);
assertEquals({ x : 12 }, foo(42, false));
assertOptimized(foo);

// No deopt this time!
assertEquals({ x : 17 }, foo(42, true));
assertOptimized(foo);
