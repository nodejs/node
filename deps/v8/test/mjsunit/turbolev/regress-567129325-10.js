// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --maglev-assert-types

const ta = new Int32Array(0);
const hn = %AllocateHeapNumber();
Object.prototype.prop = 42;

function bar(o, run) {
  if (run) return o.prop;
  return 0;
}
%PrepareFunctionForOptimization(bar);
bar(hn, true);
bar({}, true);

function foo(cond, y, run) {
  let x = cond ? hn : undefined;
  let res = 0;
  if (cond) {
    res = ta[x];
    let c = (y & 1) < 2;
    let zero = c ? 0 : 1;
    let a = x ^ zero;
    let p = c ? a : "str";
    res = bar(p, run);
  }
  return res;
}

%PrepareFunctionForOptimization(foo);
foo(false, 0, false);
foo(true, 0, false);

%OptimizeFunctionOnNextCall(foo);
foo(true, 0, true);
