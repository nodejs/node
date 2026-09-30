// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --maglev-disable-builtin-reducers

let dummy;

function callWithDefault(f = true) {
  return f();
}

function foo(x) {
  let a = callWithDefault(() => Math.floor(x));
  return a + Math.floor(x);
}

%PrepareFunctionForOptimization(foo);
assertTrue(Number.isNaN(foo()));
assertTrue(Number.isNaN(foo()));
%OptimizeFunctionOnNextCall(foo);
assertTrue(Number.isNaN(foo()));
