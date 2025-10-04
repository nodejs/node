// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --turbofan
// Flags: --turboshaft-verify-load-elimination

function f1(a2) {
  try { this(); } catch (e) {}
  this.x = a2;
  return this;
}

function f5() {
  const v6 = new f1();
  const v7 = new f1(-1801476928);
  for (let i9 = 0; i9 < 1; i9++) {
    v6.x += v7.x;
    try {
      const t0 = "ሴ-------";
      t0("ሴ-------", v6, "ሴ-------", i9, "ሴ-------");
    } catch (e) {}
    ArrayBuffer.d = ArrayBuffer;
    function f19() {
      return f5;
    }
  }
  return f1;
}

%PrepareFunctionForOptimization(f1);
%PrepareFunctionForOptimization(f5);
f5();

%OptimizeFunctionOnNextCall(f5);
f5();
