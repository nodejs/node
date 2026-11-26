// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev

let i32 = new Int32Array([0x40000000]);
globalThis.g_hn = 1.5;
globalThis.g_hn = 2.5;

function store_ho(x, flag) {
  let r = 0;
  if (flag === 101) r = 1;
  if (flag === 102) r = 2;
  globalThis.g_hn = x;
  return r;
}

function foo(arr) {
  let u = arr[0];
  store_ho(u, 0);
  "abc".startsWith("a", u);
  return u + 1.5;
}

%PrepareFunctionForOptimization(store_ho);
%PrepareFunctionForOptimization(foo);
foo(i32);

%OptimizeFunctionOnNextCall(foo);
foo(i32);
