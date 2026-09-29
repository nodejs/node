// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// Regression test for crbug.com/502944996. After a Maglev-compiled
// Array.prototype.sort mutates array g's backing store (via the literal alias),
// a deoptimization of split() must not yield two different values for the same
// g[0] load that was eliminated by load-elimination before the deopt.

function makeArr() { return [3, 1, 2]; }
const g = makeArr();

function poly(x) { return x + 1; }

let before = 0, after = 0;
function split(obj) {
  before = g[0];
  let b = poly(obj);
  after = g[0];
}

%PrepareFunctionForOptimization(makeArr);
%PrepareFunctionForOptimization(poly);
%PrepareFunctionForOptimization(split);
for (let i = 0; i < 100000; i++) split(42);
%OptimizeFunctionOnNextCall(split);
split(42);

function trigger() {
  let a = makeArr();
  let first = true;
  a.sort(function(x, y) {
    if (first) {
      first = false;
      for (let i = 0; i < 100; i++) a.push(0);
      for (let i = 0; i < 100; i++) a.pop();
    }
    return x - y;
  });
}

%PrepareFunctionForOptimization(trigger);
for (let i = 0; i < 10; i++) trigger();
%OptimizeMaglevOnNextCall(trigger);
trigger();

// Deopt split() by passing a non-number. The two g[0] loads in split must
// agree: load elimination must not forward a pre-deopt value across the deopt.
split("x");
assertEquals(before, after);
