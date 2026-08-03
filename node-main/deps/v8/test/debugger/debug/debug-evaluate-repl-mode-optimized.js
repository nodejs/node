// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --turbofan

Debug = debug.Debug

const evaluate = Debug.evaluateGlobalREPL;
const evaluateNonREPL = (source) => Debug.evaluateGlobal(source, false).value();

// Test that a let declared variable 'x' bound by an optimized function is
// updated properly.
evaluate('let x = 42;');

evaluate('function foo() { return x; }');

%PrepareFunctionForOptimization(foo);
foo();
foo();
%OptimizeFunctionOnNextCall(foo);
assertEquals(42, foo());
assertOptimized(foo);

evaluate('let x = 21');
assertEquals(21, foo());

// Test that we do not throw a 'use before declaration error' and
// that the optimized code does not load the hole from the wrong
// script context.
evaluate('x; let x;');
assertEquals(undefined, foo());

// Test that a const declared variable 'y' bound by an optimized function is
// updated properly.
evaluate('const y = 42;');

evaluate('function foo1() { return y; }');

%PrepareFunctionForOptimization(foo1);
foo1();
foo1();
%OptimizeFunctionOnNextCall(foo1);
assertEquals(42, foo1());
assertOptimized(foo1);

evaluate('const y = 21');
assertEquals(21, foo1());

// Test that a const declared variable 'z' bound by an optimized function is
// updated properly.
evaluate('const z = {a:0};');

evaluate('function foo2() { z.a = 42; }');

%PrepareFunctionForOptimization(foo2);
foo2();
foo2();
%OptimizeFunctionOnNextCall(foo2);
foo2();
assertOptimized(foo2);

evaluate('const z = {a:21}');
foo2();
assertEquals(42, z.a);
