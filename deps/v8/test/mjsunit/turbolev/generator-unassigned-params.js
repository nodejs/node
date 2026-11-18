// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --turbofan

// Parameters of generators and async functions compiled with Turbolev must
// survive every combination of interpreted and optimized suspends and resumes,
// whether or not they are assigned.

// Runs `fn(...args)` to completion twice in the interpreter and once
// optimized, checking that every run yields `expected`.
function check(fn, args, expected) {
  %PrepareFunctionForOptimization(fn);
  assertEquals(expected, [...fn(...args)]);
  assertEquals(expected, [...fn(...args)]);
  %OptimizeFunctionOnNextCall(fn);
  assertEquals(expected, [...fn(...args)]);
}

// Parameters.

function* unassigned(a, b, c) {
  for (let i = 0; i < 3; i++) yield a + b + c + i;
}
check(unassigned, [1, 2, 3], [6, 7, 8]);

function* mixed(a, b) {
  for (let i = 0; i < 3; i++) {
    b += a;
    yield [a, b];
  }
}
check(mixed, [1, 10], [[1, 11], [1, 12], [1, 13]]);

function* defaultParam(a, b = a * 2) {
  yield a;
  yield b;
  yield a + b;
}
check(defaultParam, [3], [3, 6, 9]);

function* destructured({x}, [y]) {
  yield x;
  yield y;
  yield x + y;
}
check(destructured, [{x: 1}, [2]], [1, 2, 3]);

function* rest(a, ...r) {
  yield a;
  yield r.length;
  yield a + r[0];
}
check(rest, [1, 2, 3], [1, 2, 3]);

function* sloppyArguments(a) {
  yield a;
  arguments[0] = 10;
  yield a;
  yield arguments[0];
}
check(sloppyArguments, [1], [1, 10, 10]);

function* directEval(a) {
  yield a;
  eval("a = 7");
  yield a;
}
check(directEval, [1], [1, 7]);

function* nestedEval(a) {
  yield a;
  (() => eval("a = 5"))();
  yield a;
}
check(nestedEval, [1], [1, 5]);

function* closureAssigns(a) {
  const set = (v) => { a = v; };
  yield a;
  set(3);
  yield a;
}
check(closureAssigns, [1], [1, 3]);

function* manyParams(p0, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12,
                     p13, p14, p15, p16, p17, p18, p19, p20, p21, p22, p23,
                     p24, p25, p26, p27, p28, p29, p30, p31, p32, p33) {
  yield p0 + p31;
  p32 += 1;
  yield p32 + p33;
}
check(manyParams, Array.from({length: 34}, (_, i) => i), [31, 66]);

// Tier interleaving on a single live generator.

function* interleave(a, b) {
  let i = 0;
  while (true) yield a * 100 + b * 10 + i++;
}
{
  %PrepareFunctionForOptimization(interleave);
  // Created and first suspended in the interpreter.
  let g = interleave(1, 2);
  assertEquals(120, g.next().value);
  %OptimizeFunctionOnNextCall(interleave);
  // Resumed in optimized code.
  assertEquals(121, g.next().value);
  assertEquals(122, g.next().value);
  %DeoptimizeFunction(interleave);
  // Back to the interpreter.
  assertEquals(123, g.next().value);
  %PrepareFunctionForOptimization(interleave);
  %OptimizeFunctionOnNextCall(interleave);
  assertEquals(124, g.next().value);

  // Created and first suspended in optimized code, resumed in the interpreter.
  %PrepareFunctionForOptimization(interleave);
  %OptimizeFunctionOnNextCall(interleave);
  let h = interleave(3, 4);
  assertEquals(340, h.next().value);
  %DeoptimizeFunction(interleave);
  assertEquals(341, h.next().value);
  assertEquals(342, h.next().value);

  // Two live generators of the same function, in different tiers.
  %PrepareFunctionForOptimization(interleave);
  let x = interleave(5, 6);
  assertEquals(560, x.next().value);
  %OptimizeFunctionOnNextCall(interleave);
  let y = interleave(7, 8);
  assertEquals(780, y.next().value);
  assertEquals(561, x.next().value);
  %DeoptimizeFunction(interleave);
  assertEquals(781, y.next().value);
  assertEquals(562, x.next().value);
}

// OSR inside a generator.

function* osr(a) {
  for (let i = 0; i < 20; i++) {
    if (i >= 10) %OptimizeOsr();
    if (i % 5 == 0) yield a + i;
  }
  yield a;
}
%PrepareFunctionForOptimization(osr);
assertEquals([1, 6, 11, 16, 1], [...osr(1)]);
%PrepareFunctionForOptimization(osr);
assertEquals([2, 7, 12, 17, 2], [...osr(2)]);

// Async generators.

async function collect(it) {
  const r = [];
  for await (const v of it) r.push(v);
  return r;
}

async function* asyncGen(a, b) {
  await null;
  b += 1;
  yield a + b;
  await null;
  yield a;
}
%PrepareFunctionForOptimization(asyncGen);
assertPromiseResult((async () => {
  assertEquals([4, 1], await collect(asyncGen(1, 2)));
  assertEquals([4, 1], await collect(asyncGen(1, 2)));
  %OptimizeFunctionOnNextCall(asyncGen);
  assertEquals([4, 1], await collect(asyncGen(1, 2)));
  %DeoptimizeFunction(asyncGen);
  assertEquals([4, 1], await collect(asyncGen(1, 2)));
})());

// Async functions: no initial yield, so the first suspend executed at runtime
// can be any of them.

async function asyncFn(flag, a, b) {
  if (flag) {
    await 1;
  } else {
    await 2;
  }
  await 3;
  return a + b;
}
%PrepareFunctionForOptimization(asyncFn);
assertPromiseResult((async () => {
  assertEquals(3, await asyncFn(true, 1, 2));
  %OptimizeFunctionOnNextCall(asyncFn);
  assertEquals(30, await asyncFn(false, 10, 20));
  const p = asyncFn(true, 5, 6);
  %DeoptimizeFunction(asyncFn);
  assertEquals(11, await p);
})());
