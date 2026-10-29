// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function f1(a2) {
    const v6 = ("string").codePointAt(1196);
  (a2 || v6)?.c;
  return a2;
}
%PrepareFunctionForOptimization(f1);
assertEquals(0, f1(0));
%OptimizeFunctionOnNextCall(f1);
assertEquals(0, f1(0));
