// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax

function foo(i) {
  let x;

  let a = [1.1, 1.1, 1.1];
  a[0] = a[i];
  x = a[0];

  a = [1.1, 1.1, 1.1];
  a[0] = a[i];
  x = a[0];

  return x;
}

%PrepareFunctionForOptimization(foo);
assertEquals(1.1, foo('0'));
assertEquals(1.1, foo('0'));
%OptimizeFunctionOnNextCall(foo);
assertEquals(1.1, foo('0'));
