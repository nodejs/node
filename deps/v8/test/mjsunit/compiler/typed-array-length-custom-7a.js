// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --turbofan --no-always-turbofan
// Flags: --typed-array-length-loading

let ta = new Uint32Array(10);

ta.__defineSetter__('length', function () {});

function foo() {
  return ta.length;
}
%PrepareFunctionForOptimization(foo);

assertEquals(undefined, foo());

%OptimizeFunctionOnNextCall(foo);
assertEquals(undefined, foo());
assertOptimized(foo);
