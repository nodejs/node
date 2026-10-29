// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax
// Flags: --maglev-poly-calls

function f0(v7) {
  var v8 = v7();
  if (v8.next) {
    v8.next();
  }
}
%PrepareFunctionForOptimization(f0);

f0(function () { return {} });
f0(function *() {});
%OptimizeMaglevOnNextCall(f0);
f0(function () { return {} });
