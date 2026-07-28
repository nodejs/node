// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --no-maglev
// Flags: --no-lazy-feedback-allocation --print-turbolev-inline-functions

function f7() {
  for (let v9 = 0; v9 < 2; v9++) {
    globalThis & globalThis;
  }
  return f7;
}

function outer() {
  return f7().call();
}

%PrepareFunctionForOptimization(outer);
outer();
outer();
%OptimizeFunctionOnNextCall(outer);
outer();
