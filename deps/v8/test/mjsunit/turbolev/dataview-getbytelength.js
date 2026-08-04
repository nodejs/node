// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --turbolev --turbofan --allow-natives-syntax

function foo(dv) {
  return dv.byteLength;
}
%PrepareFunctionForOptimization(foo);

let ab = new ArrayBuffer(100);
let dv = new DataView(ab);

assertEquals(100, foo(dv));

%OptimizeFunctionOnNextCall(foo);

assertEquals(100, foo(dv));
assertOptimized(foo);
