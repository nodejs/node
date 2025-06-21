// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function foo(x) {
  var k = "value";
  return x[k] = 1;
};
%PrepareFunctionForOptimization(foo);
var obj = {};
Object.defineProperty(obj, 'value', {
  set: function(x) {
    throw 'nope';
  }
});
try {
  foo(obj);
} catch (e) {
}
try {
  foo(obj);
} catch (e) {
}
%OptimizeFunctionOnNextCall(foo);
try {
  foo(obj);
} catch (e) {
}

function bar(x) {
  var k = "value";
  return (x[k] = 1) ? "ok" : "nope";
};
%PrepareFunctionForOptimization(bar);
var obj2 = {};
Object.defineProperty(obj2, 'value', {
  set: function(x) {
    throw 'nope';
    return true;
  }
});

try {
  bar(obj2);
} catch (e) {
}
try {
  bar(obj2);
} catch (e) {
}
%OptimizeFunctionOnNextCall(bar);
try {
  bar(obj2);
} catch (e) {
}
