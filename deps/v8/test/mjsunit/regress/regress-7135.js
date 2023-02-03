// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan

function foo() { return -"0" }
%PrepareFunctionForOptimization(foo);
foo();
%OptimizeFunctionOnNextCall(foo);
foo();
assertOptimized(foo);

function bar() { return -"1" }
%PrepareFunctionForOptimization(bar);
bar();
%OptimizeFunctionOnNextCall(bar);
bar();
assertOptimized(bar);
