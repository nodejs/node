// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function foo(a) {
  a++;
  a = Math.max(0, a);
  a++;
  return a;
};
%PrepareFunctionForOptimization(foo);
foo(0);
foo(0);
%OptimizeFunctionOnNextCall(foo);
assertEquals(2147483648, foo(2147483646));
