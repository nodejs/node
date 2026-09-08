// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --turbofan --no-maglev
// Flags: --no-stress-opt --no-stress-incremental-marking

// JSNativeContextSpecialization::ReduceJSResolvePromise strength-reduces
// JSResolvePromise to a direct FulfillPromise when the resolution provably has
// no "then" property. That skips the SameValue(resolution, promise)
// self-resolution cycle check, which is identity-based and precedes the "then"
// lookup in the spec. A JSPromise whose prototype "then" was deleted also lacks
// the property, so the reduction refuses promise maps to keep an async function
// that resolves with its own promise rejecting with TypeError.

// "then" is not an internal slot; drop it so JSPromise instances lack it.
const origThen = Promise.prototype.then;
delete Promise.prototype.then;

// Own property gives the resolution a stable leaf map that map inference can
// rely on (the bare promise map is not stable enough for the reduction).
function tag(p) { p.tag = 1; return p; }

// Observe a promise's settled state without its (deleted) "then".
function settled(p) {
  return new Promise((resolve) => {
    origThen.call(
        p, (v) => resolve({status: "fulfilled", selfEq: v === p}),
        (e) => resolve({status: "rejected", error: e}));
  });
}

(async () => {
  const box = {r: null};
  const dummy = tag(Promise.resolve(1));
  async function foo() {
    await 0;
    const r = box.r;
    r.tag;  // Establish fresh map knowledge for the resolution.
    return r;
  }

  // Warm up and optimize with a non-self resolution (a plain fulfillment).
  %PrepareFunctionForOptimization(foo);
  box.r = dummy;
  for (let i = 0; i < 10; i++) await foo();
  %OptimizeFunctionOnNextCall(foo);
  await foo();
  assertOptimized(foo);

  // Self-resolution: foo resolves its own promise (same tagged map) with
  // itself. The cycle check must reject rather than fulfill it with itself.
  const p = foo();
  tag(p);
  box.r = p;
  const r = await settled(p);
  assertEquals("rejected", r.status);
  assertInstanceof(r.error, TypeError);
  assertOptimized(foo);
})();
