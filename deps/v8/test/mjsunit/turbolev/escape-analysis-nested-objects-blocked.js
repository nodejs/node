// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --turbolev-escape-analysis
// Flags: --turbofan

// Escape analysis will not happen because {o2} escapes.

function baz(deopt) {
  if (deopt) {
    bar.arguments[0].y.x = 17;
  }
}
%NeverOptimizeFunction(baz);

function bar(o, deopt) {
  o.y.x = 12;
  baz(deopt);
}

function foo(arg, deopt) {
  let o1 = { x : arg };
  let o2 = { y : o1 };
  bar(o2, deopt);
  return o2;
}

%PrepareFunctionForOptimization(bar);
%PrepareFunctionForOptimization(foo);
assertEquals({y : { x : 12 } }, foo(42, false));
assertEquals({y : { x : 12 } }, foo(42, false));

%OptimizeFunctionOnNextCall(foo);
assertEquals({y : { x : 12 } }, foo(42, false));
assertOptimized(foo);

assertEquals({y : { x : 17 } }, foo(42, true));
assertOptimized(foo);
