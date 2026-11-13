// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// Large frame size in the async generator forces a stack overflow inside
// ResumeGeneratorTrampoline (or the callee prologue) after input_or_debug_pos
// has been overwritten with the non-Smi resume value, but before
// SwitchOnGeneratorState transitions continuation to kGeneratorExecuting.
function makeLargeAsyncGenerator() {
  const vars = Array.from({length: 500}, (_, i) => `a${i} = ${i}`).join(', ');
  return eval(`(async function* gen(awaitPromise) {
    let ${vars};
    new Error().stack;
    const x = awaitPromise ? await awaitPromise : yield 1;
    return x + a0 + a499;
  })`);
}

function makeLargeAsyncFunction() {
  const vars = Array.from({length: 500}, (_, i) => `a${i} = ${i}`).join(', ');
  return eval(`(async function fn(awaitedPromise) {
    let ${vars};
    new Error().stack;
    const x = await awaitedPromise;
    return x + a0 + a499;
  })`);
}

const genFn = makeLargeAsyncGenerator();
const asyncFn = makeLargeAsyncFunction();

// 1. Test AsyncGeneratorResumeNext (.next() with a non-Smi value near stack
// limit): must reject the returned Promise, close the generator, drain the
// request queue, and keep generator invariants valid for %DebugPrint.
(function testAsyncGeneratorResumeNextStackOverflow() {
  const g = genFn();
  g.next();
  %PerformMicrotaskCheckpoint();

  let p;
  function recurse() {
    try {
      recurse();
    } catch (e) {
      p = g.next('non-smi');
    }
  }
  recurse();

  %DebugPrint(g);

  assertPromiseResult(p, assertUnreachable, (err) => {
    assertInstanceof(err, RangeError);
  });
  %PerformMicrotaskCheckpoint();

  let after;
  g.next('clean').then((res) => {
    after = res;
  });
  %PerformMicrotaskCheckpoint();
  assertEquals({value: undefined, done: true}, after);
})();

// 2. Test AsyncGeneratorAwaitResume (.return() where Await's constructor
// lookup throws near stack limit and resumes the generator via
// AsyncGeneratorAwaitResume).
(function testAsyncGeneratorAwaitResumeStackOverflow() {
  const g = genFn();
  g.next();
  %PerformMicrotaskCheckpoint();

  let p;
  const badPromise = Promise.resolve('non-smi');
  Object.setPrototypeOf(badPromise, {
    get constructor() {
      throw 'non-smi-error';
    }
  });

  function recurse() {
    try {
      recurse();
    } catch (e) {
      p = g.return(badPromise);
    }
  }
  recurse();

  %DebugPrint(g);

  assertPromiseResult(p, assertUnreachable, (err) => {
    assertInstanceof(err, RangeError);
  });
  %PerformMicrotaskCheckpoint();

  let after;
  g.next('clean').then((res) => {
    after = res;
  });
  %PerformMicrotaskCheckpoint();
  assertEquals({value: undefined, done: true}, after);
})();

// 3. Test promise::ResumeGenerator in promise-reaction-job.tq (resuming an
// async generator from an internal await microtask when microtasks are drained
// near stack limit).
(function testPromiseResumeGeneratorStackOverflow() {
  let resolveAwait;
  const awaitedPromise = new Promise((r) => {
    resolveAwait = r;
  });
  const g = genFn(awaitedPromise);
  const p = g.next();
  resolveAwait('non-smi');

  function recurse() {
    try {
      recurse();
    } catch (e) {
      %PerformMicrotaskCheckpoint();
    }
  }
  recurse();

  %DebugPrint(g);

  assertPromiseResult(p, assertUnreachable, (err) => {
    assertInstanceof(err, RangeError);
  });
  %PerformMicrotaskCheckpoint();

  let after;
  g.next('clean').then((res) => {
    after = res;
  });
  %PerformMicrotaskCheckpoint();
  assertEquals({value: undefined, done: true}, after);
})();

// 4. Test AsyncResumeTask::kAsyncFunctionAwait in
// builtins-microtask-queue-gen.cc (resuming an async function from an
// already-fulfilled promise fast-path microtask near stack limit).
(function testAsyncFunctionFastAwaitStackOverflow() {
  const p = asyncFn(Promise.resolve('non-smi'));
  function recurse() {
    try {
      recurse();
    } catch (e) {
      %PerformMicrotaskCheckpoint();
    }
  }
  recurse();

  assertPromiseResult(p, assertUnreachable, (err) => {
    assertInstanceof(err, RangeError);
  });
  %PerformMicrotaskCheckpoint();
})();

// 5. Test AsyncFunctionAwaitResumeClosure in builtins-async-function-gen.cc
// (resuming an async function from a reusable closure reaction near stack
// limit).
(function testAsyncFunctionSlowAwaitStackOverflow() {
  let resolveAwait;
  const slowPromise = new Promise((r) => {
    resolveAwait = r;
  });
  const p = asyncFn(slowPromise);
  resolveAwait('non-smi');

  let depth = 0;
  function recurse() {
    try {
      recurse();
    } catch (e) {
      if (++depth === 25) {
        %PerformMicrotaskCheckpoint();
      } else {
        throw e;
      }
    }
  }
  try {
    recurse();
  } catch (e) {
  }

  assertPromiseResult(p, assertUnreachable, (err) => {
    assertInstanceof(err, RangeError);
  });
  %PerformMicrotaskCheckpoint();
})();
