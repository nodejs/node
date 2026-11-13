// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --stack-size=250

(function testTryCatchRegExpLiteralStackOverflow() {
  function createRegExpInTry(compile_re) {
    try {
      if (compile_re) {
        return /[[[[[[[[[[[[[[[[[[[[[[[[[[[[[a]]]]]]]]]]]]]]]]]]]]]]]]]]]]]/v;
      }
    } catch (e) {
      return e;
    }
    return null;
  }

  %PrepareFunctionForOptimization(createRegExpInTry);
  assertNull(createRegExpInTry(false));
  assertNull(createRegExpInTry(false));
  %OptimizeFunctionOnNextCall(createRegExpInTry);
  assertNull(createRegExpInTry(false));

  let caught_error = null;
  function runNearStackLimit() {
    try {
      JSON.parse('1', runNearStackLimit);
    } catch (e) {
      if (e instanceof SyntaxError) throw e;
    }
    if (caught_error === null) {
      try {
        const res = createRegExpInTry(true);
        if (res instanceof SyntaxError) {
          caught_error = res;
        }
      } catch (e) {
        if (e instanceof SyntaxError) throw e;
      }
    }
  }

  runNearStackLimit();
  assertInstanceof(caught_error, SyntaxError);
})();

(function testAsyncGeneratorRegExpLiteralStackOverflow() {
  async function* gen(compile_re) {
    if (compile_re) {
      /[[[[[[[[[[[[[[[[[[[[[[[[[[[[[a]]]]]]]]]]]]]]]]]]]]]]]]]]]]]/v;
    }
  }

  const warmup = [];
  %PrepareFunctionForOptimization(gen);
  warmup.push(gen(false).next());
  warmup.push(gen(false).next());
  %OptimizeFunctionOnNextCall(gen);
  warmup.push(gen(false).next());

  const promises = [];
  let attempts = 0;
  function runNearStackLimit() {
    try {
      JSON.parse('1', runNearStackLimit);
    } catch (e) {
    }
    if (attempts < 32) {
      attempts++;
      try {
        const p = gen(true).next();
        promises[promises.length] = p;
      } catch (e) {
      }
    }
  }

  runNearStackLimit();

  let rejected_syntax_error = null;
  for (const p of warmup) {
    p.then(() => {}, () => {});
  }
  for (const p of promises) {
    p.then(
        () => {},
        (e) => {
          if (e instanceof SyntaxError) rejected_syntax_error = e;
        });
  }
  %PerformMicrotaskCheckpoint();
  assertInstanceof(rejected_syntax_error, SyntaxError);
})();
