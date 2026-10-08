// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --no-concurrent-osr

Uint8ClampedArray.setFromHex = {};

function foo() {
  for (let i = 0; i < 5; i++) {
    %OptimizeOsr();
    const receiver = i || Uint8ClampedArray;
    try { receiver.sumPrecise(receiver); } catch (e) {}
    try { receiver.setFromHex(); } catch (e) {}
  }
}

%PrepareFunctionForOptimization(foo);
foo();
