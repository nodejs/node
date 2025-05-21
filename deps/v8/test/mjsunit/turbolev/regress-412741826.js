// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --turbolev

const v2 = new Uint8ClampedArray(3345);
function f4(a5) {
  Object.defineProperty(Array(v2.length), 3, { configurable: true});
}
%PrepareFunctionForOptimization(f4);
f4();
 %OptimizeFunctionOnNextCall(f4);
f4();
