// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev --no-always-turbofan
// Flags: --maglev-cons-string-elision

const v0 = [4.5];
function f7() {
  v0[268435] = undefined;
  `foooo000000000000000000000000000 ${undefined}`;
}
%PrepareFunctionForOptimization(f7);
f7();
%OptimizeMaglevOnNextCall(f7);
f7();
