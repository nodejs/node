// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan --no-always-turbofan

const gsab = new SharedArrayBuffer(4, {"maxByteLength": 8});
const ta_gsab = new Uint16Array(gsab);
function f0(ta) {
  ta[ta.length - 1] = 17;
  Object.defineProperty(ta, 'a', {
    set: function (value) {
      ta[0] = 0;
    }
  });
}
function f1() {
  const ta_normal = new Uint32Array();
  f0(ta_normal);
  f0(ta_gsab);
}
%PrepareFunctionForOptimization(f1);
%PrepareFunctionForOptimization(f0);
f1();
%OptimizeFunctionOnNextCall(f1);

try {
  f1();
} catch {}
