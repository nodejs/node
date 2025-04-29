// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --turbofan --no-always-turbofan
// Flags: --typed-array-length-loading

function foo() {
  let ta = (() => new Uint8Array(100))();
  return ta.length;
}
%PrepareFunctionForOptimization(foo);

const v1 = foo();
assertEquals(100, v1);

%OptimizeFunctionOnNextCall(foo);
const v2 = foo();
assertEquals(100, v2);
