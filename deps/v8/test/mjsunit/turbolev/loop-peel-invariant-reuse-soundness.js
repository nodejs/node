// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --turbofan
// Flags: --turbolev-non-eager-loop-peeling

// Soundness guard for reusing loop-invariant clones across peeled loop headers.
// The peeled iteration and the in-loop body hold structurally-identical loads;
// the optimizer may only reuse the dominating peeled load for a location the
// loop does NOT write. These loops write the loaded location, so the peeled
// value must not be reused for the loop-entry read.

// `o.x` is written every iteration; the aliasing store `other.x = i` (same map)
// clears the cached `o.x` so the second read is a fresh, structurally-identical
// reload -- but `o.x` is not loop invariant.
function aliasing_store(o, other, n) {
  let s = 0;
  for (let i = 0; i < n; i++) {
    s += o.x;
    o.x = i;
    other.x = i;
    s += o.x;
  }
  return s;
}
function run_aliasing(n) {
  return aliasing_store({x: 100}, {x: 0}, n);
}
%PrepareFunctionForOptimization(aliasing_store);
%PrepareFunctionForOptimization(run_aliasing);
const expected_aliasing = run_aliasing(20);
run_aliasing(20);
%OptimizeFunctionOnNextCall(aliasing_store);
%OptimizeFunctionOnNextCall(run_aliasing);
assertEquals(expected_aliasing, run_aliasing(20));
assertEquals(expected_aliasing, run_aliasing(20));

// Same idea, but the cache is cleared by an opaque call between the write and
// the reload.
let sink = 0;
function clobber() { sink++; }
%NeverOptimizeFunction(clobber);
function call_clobber(o, n) {
  let s = 0;
  for (let i = 0; i < n; i++) {
    s += o.x;
    o.x = i;
    clobber();
    s += o.x;
  }
  return s;
}
function run_call(n) {
  return call_clobber({x: 100}, n);
}
%PrepareFunctionForOptimization(call_clobber);
%PrepareFunctionForOptimization(run_call);
const expected_call = run_call(20);
run_call(20);
%OptimizeFunctionOnNextCall(call_clobber);
%OptimizeFunctionOnNextCall(run_call);
assertEquals(expected_call, run_call(20));
assertEquals(expected_call, run_call(20));
