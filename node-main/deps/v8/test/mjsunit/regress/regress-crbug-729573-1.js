// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

(function() {
function foo() {
  var a = foo.bind(this);
  %DeoptimizeNow();
  if (!a) return a;
  return 0;
};
%PrepareFunctionForOptimization(foo);
assertEquals(0, foo());
assertEquals(0, foo());
%OptimizeFunctionOnNextCall(foo);
assertEquals(0, foo());
})();

(function() {
"use strict";

function foo() {
  var a = foo.bind(this);
  %DeoptimizeNow();
  if (!a) return a;
  return 0;
};
%PrepareFunctionForOptimization(foo);
assertEquals(0, foo());
assertEquals(0, foo());
%OptimizeFunctionOnNextCall(foo);
assertEquals(0, foo());
})();

(function() {
function foo() {
  var a = foo.bind(this);
  %DeoptimizeNow();
  if (!a) return a;
  return 0;
};
%PrepareFunctionForOptimization(foo);
foo.prototype = {
  custom: 'prototype'
};

assertEquals(0, foo());
assertEquals(0, foo());
%OptimizeFunctionOnNextCall(foo);
assertEquals(0, foo());
})();

(function() {
"use strict";

function foo() {
  var a = foo.bind(this);
  %DeoptimizeNow();
  if (!a) return a;
  return 0;
};
%PrepareFunctionForOptimization(foo);
foo.prototype = {
  custom: 'prototype'
};

assertEquals(0, foo());
assertEquals(0, foo());
%OptimizeFunctionOnNextCall(foo);
assertEquals(0, foo());
})();
