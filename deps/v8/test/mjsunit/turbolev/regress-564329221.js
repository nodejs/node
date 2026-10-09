// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --turbolev --turbolev-future --allow-natives-syntax
// Flags: --no-maglev-range-analysis --no-turbolev-untagged-phis

function foo(m) {
  let it = m.entries();
  let res = it.next().value;
  return res[0];
}

let m = new Map();
m.set(1, 2);

%PrepareFunctionForOptimization(foo);
foo(m);
foo(m);

%OptimizeFunctionOnNextCall(foo);
foo(m);
