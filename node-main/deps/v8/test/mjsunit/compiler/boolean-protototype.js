// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function test1(s) {
  return s.toString;
}
%PrepareFunctionForOptimization(test1);
assertSame(test1(false), Boolean.prototype.toString);
assertSame(test1(true), Boolean.prototype.toString);
%OptimizeFunctionOnNextCall(test1);
assertSame(test1(false), Boolean.prototype.toString);
assertSame(test1(true), Boolean.prototype.toString);

function test2(s) {
  return s.valueOf;
}
%PrepareFunctionForOptimization(test2);
assertSame(test2(false), Boolean.prototype.valueOf);
assertSame(test2(true), Boolean.prototype.valueOf);
%OptimizeFunctionOnNextCall(test2);
assertSame(test2(false), Boolean.prototype.valueOf);
assertSame(test2(true), Boolean.prototype.valueOf);

Boolean.prototype.foo = 42;
function test3(s) {
  return s["foo"];
}
%PrepareFunctionForOptimization(test3);
assertEquals(test3(false), 42);
assertEquals(test3(true), 42);
%OptimizeFunctionOnNextCall(test3);
assertEquals(test3(false), 42);
assertEquals(test3(true), 42);

Boolean.prototype.bar = function bar() { "use strict"; return this; }
function test4(s) {
  return s.bar();
}
%PrepareFunctionForOptimization(test4);
assertEquals(test4(false), false);
assertEquals(test4(true), true);
%OptimizeFunctionOnNextCall(test4);
assertEquals(test4(false), false);
assertEquals(test4(true), true);
