// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --maglev --allow-natives-syntax

function foo(dv) {
  try {
    return dv.byteLength;
  } catch (e) {
    return 'exception';
  }
}
%PrepareFunctionForOptimization(foo);

let ab = new ArrayBuffer(100);
let dv = new DataView(ab);

assertEquals(100, foo(dv));
assertEquals('exception', foo());

%OptimizeMaglevOnNextCall(foo);

assertEquals(100, foo(dv));
assertEquals('exception', foo());
assertTrue(isMaglevved(foo));
