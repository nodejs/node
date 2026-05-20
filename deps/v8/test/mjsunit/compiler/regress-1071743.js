// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax


function foo(v) {
  let x = Math.floor(v);
  Number.prototype[v] = 42;
  return x + Math.floor(v);
}

%PrepareFunctionForOptimization(foo);
assertSame(foo(-0), -0);
assertSame(foo(-0), -0);
%OptimizeFunctionOnNextCall(foo);
assertSame(foo(-0), -0);


function bar(v) {
  v = v ? (v|0) : -0;  // v has now type Integral32OrMinusZero.
  let x = Math.floor(v);
  Number.prototype[v] = 42;
  return x + Math.floor(v);
}

%PrepareFunctionForOptimization(bar);
assertSame(2, bar(1));
assertSame(2, bar(1));
%OptimizeFunctionOnNextCall(bar);
assertSame(-0, bar(-0));
