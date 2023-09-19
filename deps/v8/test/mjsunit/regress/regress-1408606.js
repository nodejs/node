// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

var v0 = new Uint8ClampedArray();
function f0(cnt) {
  Object.defineProperty(v0, cnt == 0 ? "subarray" : "property", {
  })
  v0[-7] = -1.5;
  v0[-7];
}
%PrepareFunctionForOptimization(f0);
f0(0);
f0();
%OptimizeFunctionOnNextCall(f0);
f0();
