// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

var o1 = {};
var o2 = {};

function foo(x) {
  return x.bar;
}

Object.defineProperty(o1, 'bar', {value: 200});
foo(o1);
foo(o1);

function f(b) {
  var o = o2;
  if (b) {
    return foo(o);
  }
};
%PrepareFunctionForOptimization(f);
f(false);
%OptimizeFunctionOnNextCall(f);
assertEquals(undefined, f(false));
Object.defineProperty(o2, 'bar', {value: 100});
assertEquals(100, f(true));
