// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// Awaiting a promise whose `.constructor` getter throws must reject the
// async function's promise, not throw synchronously out of the async
// function call.
function makeBrokenPromise() {
  const p = Promise.resolve();
  Object.defineProperty(p, "constructor", {
    get() { throw new Error("broken"); }
  });
  return p;
}

async function f() {
  await makeBrokenPromise();
}

%PrepareFunctionForOptimization(f);
f().catch(() => {});
f().catch(() => {});
%OptimizeFunctionOnNextCall(f);

// Must not throw synchronously; must return a rejected promise.
let threw = false;
let result;
try {
  result = f();
} catch (e) {
  threw = true;
}
assertFalse(threw);
assertInstanceof(result, Promise);

let rejection;
result.then(
    () => assertUnreachable(),
    e => { rejection = e; });
%PerformMicrotaskCheckpoint();
assertInstanceof(rejection, Error);
assertEquals("broken", rejection.message);
