// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev

(function() {
var A = {};

A[Symbol.hasInstance] = function(x) {
  return 1;
};

var a = {};

function foo(o) {
  return o instanceof A;
};
%PrepareFunctionForOptimization(foo);
foo(a);
foo(a);
assertSame(foo(a), true);
%OptimizeMaglevOnNextCall(foo);
assertSame(foo(a), true);
})();

(function() {
var A = {};

A[Symbol.hasInstance] = function(x) {
  %DeoptimizeFunction(foo);
  return 1;
};

var a = {};

function foo(o) {
  return o instanceof A;
};
%PrepareFunctionForOptimization(foo);
foo(a);
foo(a);
assertSame(foo(a), true);
%OptimizeMaglevOnNextCall(foo);
assertSame(foo(a), true);
})();
