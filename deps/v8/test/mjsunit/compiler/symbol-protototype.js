// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function test1(s) {
  return s.toString;
}
%PrepareFunctionForOptimization(test1);
assertSame(test1(Symbol()), Symbol.prototype.toString);
assertSame(test1(Symbol()), Symbol.prototype.toString);
%OptimizeFunctionOnNextCall(test1);
assertSame(test1(Symbol()), Symbol.prototype.toString);

function test2(s) {
  return s.valueOf;
}
%PrepareFunctionForOptimization(test2);
assertSame(test2(Symbol()), Symbol.prototype.valueOf);
assertSame(test2(Symbol()), Symbol.prototype.valueOf);
%OptimizeFunctionOnNextCall(test2);
assertSame(test2(Symbol()), Symbol.prototype.valueOf);

Symbol.prototype.foo = 1;
function test3(s) {
  return s["foo"];
}
%PrepareFunctionForOptimization(test3);
assertEquals(test3(Symbol()), 1);
assertEquals(test3(Symbol()), 1);
%OptimizeFunctionOnNextCall(test3);
assertEquals(test3(Symbol()), 1);

Symbol.prototype.bar = function() { "use strict"; return this; }
function test4(s) {
  return s.bar();
}
%PrepareFunctionForOptimization(test4);
var s = Symbol("foo");
assertEquals(test4(s), s);
assertEquals(test4(s), s);
%OptimizeFunctionOnNextCall(test4);
assertEquals(test4(s), s);
