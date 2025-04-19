// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan --no-always-turbofan

const a = new Uint8Array();
function foo() {
  return ArrayBuffer.isView(a);
}
assertTrue(foo());
%PrepareFunctionForOptimization(foo);
%OptimizeFunctionOnNextCall(foo);
assertTrue(foo());
