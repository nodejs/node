// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

var a = [];

function foo() {
  return a[Symbol.iterator]().next();
};
%PrepareFunctionForOptimization(foo);
a.__proto__.push(5);
a.bla = {};

foo();
%OptimizeFunctionOnNextCall(foo);
foo();
