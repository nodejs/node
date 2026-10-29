// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --maglev --allow-natives-syntax

function wrap(f) { return f(); }

function make_f() {
  let x = 0;
  let setter = wrap(() => function (v) { x = v; });
  %PrepareFunctionForOptimization(setter);
  function f() {
    Array().join(), x, new Boolean();
    x = 0;
    setter(1);
    // Since x is a context slot and setter might alias it, Maglev should
    // invalidate its cache for x and reload it for the comparison.
    if (x !== 1) return 1;
    return 0;
  }
  %PrepareFunctionForOptimization(f);
  return f;
}

let f1 = make_f();
f1();
f1();
%OptimizeMaglevOnNextCall(f1);
if (f1() !== 0) throw new Error("Failed at final call");
