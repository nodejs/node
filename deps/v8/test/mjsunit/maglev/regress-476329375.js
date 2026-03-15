// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function __wrapTC(f, permissive = true) {
  try {
    return f();
  } catch (e) {
  }
}

function foo() {
  for (let [__v_55, __v_56] = (() => {
    let __v_57 = 10;
    return [0, __v_57];
  })(); __v_55 < __v_56; __v_55++) {
    const __v_58 = __wrapTC(() => __v_56 & __v_56);
    let __v_59 = new Array(__v_58);
    const __v_60 = __wrapTC();
    try {
      __v_61.bind();
      __v_59;
    } catch (e) {}
  }
}

%PrepareFunctionForOptimization(__wrapTC);
%PrepareFunctionForOptimization(foo);
foo();

%OptimizeMaglevOnNextCall(foo);
foo();
