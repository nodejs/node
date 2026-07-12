// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --maglev

let v0 = 1;
function f0() {
  for (let v3 = 0; v3 < 25; v3++) {
    v0 = v3;
    [].forEach(v3);
    v0 = v3; // Dead code (above will throw).
  }
}
%PrepareFunctionForOptimization(f0);
try {
  f0();
} catch (e) {}
%OptimizeMaglevOnNextCall(f0);
try {
  f0();
} catch (e) {}
