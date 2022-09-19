// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax

Array.prototype[0] = 'a';
delete Array.prototype[0];

function foo(a, i) {
  return a[i];
};
%PrepareFunctionForOptimization(foo);
var a = new Array(100000);
a[3] = 'x';

foo(a, 3);
foo(a, 3);
foo(a, 3);
%OptimizeFunctionOnNextCall(foo);
foo(a, 3);
Array.prototype[0] = 'a';
var z = foo(a, 0);
assertEquals('a', z);
