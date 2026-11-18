// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --expose-gc

// Context of generators compiled with Turbolev must survive every combination
// of interpreted and optimized suspends and resumes, whether or not the
// function pushes an inner context.

// Runs `fn(...args)` to completion twice in the interpreter and once
// optimized, checking that every run yields `expected`.
function check(fn, args, expected) {
  %PrepareFunctionForOptimization(fn);
  assertEquals(expected, [...fn(...args)]);
  assertEquals(expected, [...fn(...args)]);
  %OptimizeFunctionOnNextCall(fn);
  assertEquals(expected, [...fn(...args)]);
}

function* noContext(o) {
  yield o.v;
  gc();
  yield o.v + 1;
}
check(noContext, [{v: 1}], [1, 2]);

function* blockContext(o) {
  {
    let x = o.v;
    const f = () => x;
    yield f();
    gc();
    yield f() + 1;
  }
  yield 0;
}
check(blockContext, [{v: 1}], [1, 2, 0]);

function* catchContext(o) {
  try {
    throw o;
  } catch (e) {
    const f = () => e.v;
    yield f();
    gc();
    yield f() + 1;
  }
}
check(catchContext, [{v: 1}], [1, 2]);

function* withContext(o) {
  with (o) {
    yield v;
    yield v + 1;
  }
}
check(withContext, [{v: 1}], [1, 2]);

function* functionContext(a) {
  const f = () => a;
  yield f();
  gc();
  yield f() + 1;
}
check(functionContext, [1], [1, 2]);

// Sloppy eval always forces a function context, so these push a context too.
function* sloppyEval(a) {
  eval("var x = a");
  yield x;
  gc();
  yield x + 1;
}
check(sloppyEval, [1], [1, 2]);

function* sloppyEvalInParams(a, b = eval("var y = a; y")) {
  yield b;
  gc();
  yield b + 1;
}
check(sloppyEvalInParams, [1], [1, 2]);
