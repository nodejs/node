// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --verify-heap


function __wrapTC(f, permissive = true) {
  try {
    return f();
  } catch (e) {
  }
}

function foo(x) {
  let v_nan = __wrapTC(() => Math.atanh(x));
  let v_undefined = __wrapTC();
  return [v_nan, v_undefined];
}

%PrepareFunctionForOptimization(foo);
foo();
%OptimizeFunctionOnNextCall(foo);
let r = foo(2);
assertEquals(NaN, r[0]);
assertEquals(undefined, r[1]);
