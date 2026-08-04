// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

(function ShortcutEmptyStringAddRight() {
  let ar = new Float32Array(1);
  function opt(i){
    return ar[i] + (NaN ? 0 : '');
  }
  %PrepareFunctionForOptimization(opt);
  ar[0] = 42;
  opt(1);
  %OptimizeFunctionOnNextCall(opt);
  assertEquals("42", opt(0));
})();

(function ShortcutiEmptyStringAddLeft() {
  let ar = new Float32Array(1);
  function opt(i){
    return (NaN ? 0 : '') + ar[i];
  }
  %PrepareFunctionForOptimization(opt);
  ar[0] = 42;
  opt(1);
  %OptimizeFunctionOnNextCall(opt);
  assertEquals("42", opt(0));
})();
