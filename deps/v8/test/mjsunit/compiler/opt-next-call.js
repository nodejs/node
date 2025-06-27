// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan

function foo() {
  return "fooed";
}

%PrepareFunctionForOptimization(foo);
%OptimizeFunctionOnNextCall(foo);
assertEquals("fooed", foo());
assertOptimized(foo);

function bar() {
  return "bared";
}

%PrepareFunctionForOptimization(bar);
assertEquals("bared", bar());
%OptimizeFunctionOnNextCall(bar);
assertEquals("bared", bar());
assertOptimized(bar);
