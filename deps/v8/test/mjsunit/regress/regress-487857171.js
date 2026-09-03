// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --track-array-buffer-views

function test() {
  const buffer = new ArrayBuffer(1024 * 1024);
  const i32 = new Int32Array(buffer);
  i32[0] = 1;
  buffer.transfer();
  return i32[0];
}
%PrepareFunctionForOptimization(test);
test();
%OptimizeFunctionOnNextCall(test);
assertEquals(test(), undefined);
