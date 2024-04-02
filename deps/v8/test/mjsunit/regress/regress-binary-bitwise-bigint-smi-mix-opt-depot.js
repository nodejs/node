// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --opt --allow-natives-syntax --no-always-opt

function foo(n) {
  let v = 0;
  for (let i = 0n; i < n; ++i) {
    v = v | 1;
    v = i;
  }

  v = 0;
  for (let i = 0n; i < n; ++i) {
    v = v ^ 1;
    v = i;
  }

  v = 0;
  for (let i = 0n; i < n; ++i) {
    v = v & 1;
    v = i;
  }

  v = 0;
  for (let i = 0n; i < n; ++i) {
    v = v << 1;
    v = i;
  }

  v = 0;
  for (let i = 0n; i < n; ++i) {
    v = v >> 1;
    v = i;
  }

  v = 0;
  for (let i = 0n; i < n; ++i) {
    v = v >>> 1;
    v = i;
  }
}

%PrepareFunctionForOptimization(foo);
assertDoesNotThrow(() => foo(1n));
%OptimizeFunctionOnNextCall(foo);
assertDoesNotThrow(() => foo(1n));
assertOptimized(foo);
%PrepareFunctionForOptimization(foo);
assertThrows(() => foo(2n), TypeError);
%OptimizeFunctionOnNextCall(foo);
assertDoesNotThrow(() => foo(1n));
assertOptimized(foo);
assertThrows(() => foo(2n), TypeError);
assertOptimized(foo);
