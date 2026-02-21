// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax


function test() {
  const a = new DataView(new ArrayBuffer(32));
  const b = new DataView(new ArrayBuffer(32));
  a.setFloat64(0);
  b.setFloat64(0, undefined);

  for(let i = 0; i < 8; ++i) {
    assertEquals(a.getUint8(i), b.getUint8(i));
  }
}

%PrepareFunctionForOptimization(test);
test();
test();
%OptimizeFunctionOnNextCall(test);
test();
