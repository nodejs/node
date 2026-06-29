// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev

class C0 {
}
function f1(a2) {
  try { v3 = a2.bar(); } catch (e) {}
}
%PrepareFunctionForOptimization(f1);
f1(Object);
const v6 = {};
f1(Object.defineProperty(v6, "bar", { get: C0 }));
const v11 = %OptimizeMaglevOnNextCall(f1);
f1();
