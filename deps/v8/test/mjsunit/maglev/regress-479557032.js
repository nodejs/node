// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev-truncated-int32-phis
// Flags: --no-lazy-feedback-allocation

function wrap(f) {
    return f();
}
function foo( a) {
  let x = wrap(() => a ? 3.35 : 4.59);
  let y = wrap(() => x & 255);
}

%PrepareFunctionForOptimization(foo);
foo();
foo();
%OptimizeFunctionOnNextCall(foo);
foo();
