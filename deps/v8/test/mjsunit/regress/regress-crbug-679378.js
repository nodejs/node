// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

var x = {};
x.__defineGetter__('0', () => 0);
x.a = {
  v: 1.51
};

var y = {};
y.a = {
  u: 'OK'
};

function foo(o) {
  return o.a.u;
};
%PrepareFunctionForOptimization(foo);
foo(y);
foo(y);
foo(x);
%OptimizeFunctionOnNextCall(foo);
%DebugPrint(foo(x));
