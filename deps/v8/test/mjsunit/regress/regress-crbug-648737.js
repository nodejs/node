// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbo-escape

function f(str) {
  var s = "We turn {" + str + "} into a ConsString now";
  return s.length;
};
%PrepareFunctionForOptimization(f);
assertEquals(33, f('a'));
assertEquals(33, f("b"));
%OptimizeFunctionOnNextCall(f);
assertEquals(33, f("c"));

function g(str) {
  var s = "We also try to materalize {" + str + "} when deopting";
  %DeoptimizeNow();
  return s.length;
};
%PrepareFunctionForOptimization(g);
assertEquals(43, g('a'));
assertEquals(43, g("b"));
%OptimizeFunctionOnNextCall(g);
assertEquals(43, g("c"));
