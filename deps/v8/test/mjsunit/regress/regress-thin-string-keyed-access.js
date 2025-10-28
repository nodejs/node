// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function f0() {
  const v0 = () => {};
  function f1(v1) {
    return v0[v1];
  }
  f1("c" + "c");
  f1("c" + "c");
  f1();
}

%PrepareFunctionForOptimization(f0);
f0();
f0();
%OptimizeFunctionOnNextCall(f0);
f0();
