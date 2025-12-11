// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original: alloc/alloc.js
console.log(42);
try {
  console.log(42);
} catch (__v_0) {}
function __f_0() {
  return 0;
}
for (let __v_1 = 0; __v_1 < 10000; __v_1++) {
  console.log(42);
}
%PrepareFunctionForOptimization(__f_0);
__f_0();
__f_0();
%OptimizeFunctionOnNextCall(__f_0);
__f_0();
