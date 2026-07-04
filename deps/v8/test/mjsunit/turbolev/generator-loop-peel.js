// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --turbofan
// Flags: --turbolev-non-eager-loop-peeling --no-concurrent-osr

// Resumable generators (async functions and function*) build a generator
// resume dispatch with irreducible control flow, so the MaglevLoopPeeler used
// to bail on the whole graph. But loops only become peel candidates after the
// per-loop `!resumable()` filter in the graph builder, so a loop with no
// suspend point inside it is reducible and safe to peel even inside a resumable
// generator. As in the other peel tests, a peeled loop carries a %AssertPeeled()
// marker the peeler strips; if peeling stops firing the marker survives to the
// Turbolev backend and the compile fails. Loops that must NOT peel (a suspend
// inside the loop, or a non-innermost loop) carry the opposite %AssertNotPeeled()
// marker: it survives to the backend as a no-op when the loop is left unpeeled,
// but fails the compile if the peeler ever peels the loop.

// async function with a non-resumable inner loop: the function is a resumable
// generator but the loop has no await, so it is a peel candidate. This is the
// shape of JetStream's richards-wasm `runIteration`.
async function async_peel(n) {
  let s = 0;
  for (let i = 0; i < n; i++) {
    %AssertPeeled();
    s += i;
  }
  return s;
}
%PrepareFunctionForOptimization(async_peel);

// Same, but with an await BEFORE the loop (richards-wasm awaits module setup,
// then runs the synchronous hot loop). The await splits the function across two
// resumption segments; the loop lives entirely in the second one.
async function async_await_before_loop(n) {
  await Promise.resolve();
  let s = 0;
  for (let i = 0; i < n; i++) {
    %AssertPeeled();
    s += i;
  }
  return s;
}
%PrepareFunctionForOptimization(async_await_before_loop);

// A non-resumable inner loop nested in a RESUMABLE outer loop (await in the
// outer body). Only the innermost loop is a peel candidate; the resume edge for
// the outer await targets the block after the await, never the inner loop. The
// outer loop must not peel; the inner loop must.
async function async_resumable_outer(n, m) {
  let s = 0;
  for (let i = 0; i < n; i++) {
    %AssertNotPeeled();
    for (let j = 0; j < m; j++) {
      %AssertPeeled();
      s += j;
    }
    await Promise.resolve();
  }
  return s;
}
%PrepareFunctionForOptimization(async_resumable_outer);

// function* (sync generator) with a yield outside the peelable inner loop.
function* gen_peel(n) {
  let s = 0;
  for (let i = 0; i < n; i++) {
    %AssertPeeled();
    s += i;
  }
  yield s;
}
%PrepareFunctionForOptimization(gen_peel);

// OSR inside an async function's loop — the exact tier-up path of richards-wasm
// (the long-running loop OSRs mid-call). No marker dependency on OSR vs
// full-function: the loop must peel either way.
async function async_osr(n) {
  let s = 0;
  for (let i = 0; i < n; i++) {
    %OptimizeOsr();
    %AssertPeeled();
    s += i;
  }
  return s;
}
%PrepareFunctionForOptimization(async_osr);

// MUST NOT PEEL: await inside the loop makes the loop itself resumable, so it is
// never recorded as a peel candidate. The %AssertNotPeeled() marker fails the
// compile if it ever is. This must compile and run correctly (no whole-graph
// fallout from the peeler running on the rest of the graph).
async function async_await_in_loop(n) {
  let s = 0;
  for (let i = 0; i < n; i++) {
    %AssertNotPeeled();
    s += i;
    await Promise.resolve();
  }
  return s;
}
%PrepareFunctionForOptimization(async_await_in_loop);

assertPromiseResult((async () => {
  // async_peel
  assertEquals(45, await async_peel(10));
  assertEquals(45, await async_peel(10));
  %OptimizeFunctionOnNextCall(async_peel);
  assertEquals(0, await async_peel(0));
  assertEquals(0, await async_peel(1));
  assertEquals(45, await async_peel(10));
  assertEquals(4950, await async_peel(100));
  assertOptimized(async_peel);

  // async_await_before_loop
  assertEquals(45, await async_await_before_loop(10));
  assertEquals(45, await async_await_before_loop(10));
  %OptimizeFunctionOnNextCall(async_await_before_loop);
  assertEquals(0, await async_await_before_loop(0));
  assertEquals(45, await async_await_before_loop(10));
  assertEquals(4950, await async_await_before_loop(100));
  assertOptimized(async_await_before_loop);

  // async_resumable_outer
  assertEquals(30, await async_resumable_outer(3, 5));  // 3 * (0+1+2+3+4).
  assertEquals(30, await async_resumable_outer(3, 5));
  %OptimizeFunctionOnNextCall(async_resumable_outer);
  assertEquals(0, await async_resumable_outer(0, 5));
  assertEquals(0, await async_resumable_outer(3, 0));
  assertEquals(30, await async_resumable_outer(3, 5));
  assertOptimized(async_resumable_outer);

  // async_await_in_loop (must not peel; correctness only)
  assertEquals(45, await async_await_in_loop(10));
  assertEquals(45, await async_await_in_loop(10));
  %OptimizeFunctionOnNextCall(async_await_in_loop);
  assertEquals(0, await async_await_in_loop(0));
  assertEquals(4950, await async_await_in_loop(100));
})());

// gen_peel (synchronous generator).
assertEquals(45, gen_peel(10).next().value);
assertEquals(45, gen_peel(10).next().value);
%OptimizeFunctionOnNextCall(gen_peel);
assertEquals(0, gen_peel(0).next().value);
assertEquals(0, gen_peel(1).next().value);
assertEquals(45, gen_peel(10).next().value);
assertEquals(4950, gen_peel(100).next().value);
assertOptimized(gen_peel);

// async_osr.
assertPromiseResult((async () => {
  assertEquals(190, await async_osr(20));
})());
