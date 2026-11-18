// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev

// 1. Unassigned parameters in generator
function* genUnassigned(a, b, c) {
  for (let i = 0; i < 5; i++) {
    yield a + b + c + i;
  }
}

%PrepareFunctionForOptimization(genUnassigned);
{
  let g = genUnassigned(10, 20, 30);
  assertEquals({value: 60, done: false}, g.next());
  assertEquals({value: 61, done: false}, g.next());
  assertEquals({value: 62, done: false}, g.next());
  assertEquals({value: 63, done: false}, g.next());
  assertEquals({value: 64, done: false}, g.next());
  assertEquals({value: undefined, done: true}, g.next());
}

%OptimizeMaglevOnNextCall(genUnassigned);
{
  let g = genUnassigned(100, 200, 300);
  assertEquals({value: 600, done: false}, g.next());
  assertEquals({value: 601, done: false}, g.next());
  assertEquals({value: 602, done: false}, g.next());
  assertEquals({value: 603, done: false}, g.next());
  assertEquals({value: 604, done: false}, g.next());
  assertEquals({value: undefined, done: true}, g.next());
}

// 2. Assigned parameters in generator
function* genAssigned(a, b) {
  for (let i = 0; i < 3; i++) {
    a += 10;
    b += 100;
    yield a + b;
  }
}

%PrepareFunctionForOptimization(genAssigned);
{
  let g = genAssigned(1, 2);
  assertEquals({value: 113, done: false}, g.next());
  assertEquals({value: 223, done: false}, g.next());
  assertEquals({value: 333, done: false}, g.next());
  assertEquals({value: undefined, done: true}, g.next());
}

%OptimizeMaglevOnNextCall(genAssigned);
{
  let g = genAssigned(5, 5);
  assertEquals({value: 120, done: false}, g.next());
  assertEquals({value: 230, done: false}, g.next());
  assertEquals({value: 340, done: false}, g.next());
  assertEquals({value: undefined, done: true}, g.next());
}

// 3. Mixed assigned and unassigned parameters
function* genMixed(unassigned1, assigned, unassigned2) {
  for (let i = 0; i < 3; i++) {
    assigned += 1;
    yield unassigned1 + ":" + assigned + ":" + unassigned2;
  }
}

%PrepareFunctionForOptimization(genMixed);
{
  let g = genMixed("X", 0, "Y");
  assertEquals({value: "X:1:Y", done: false}, g.next());
  assertEquals({value: "X:2:Y", done: false}, g.next());
  assertEquals({value: "X:3:Y", done: false}, g.next());
  assertEquals({value: undefined, done: true}, g.next());
}

%OptimizeMaglevOnNextCall(genMixed);
{
  let g = genMixed("A", 10, "B");
  assertEquals({value: "A:11:B", done: false}, g.next());
  assertEquals({value: "A:12:B", done: false}, g.next());
  assertEquals({value: "A:13:B", done: false}, g.next());
  assertEquals({value: undefined, done: true}, g.next());
}

// 4. Maglev optimization explicitly
function* genMaglev(p1, p2) {
  for (let i = 0; i < 4; i++) {
    yield p1 * 10 + p2 + i;
  }
}

%PrepareFunctionForOptimization(genMaglev);
for (let j = 0; j < 5; ++j) {
  let g = genMaglev(2, 3);
  g.next();
  g.next();
}
%OptimizeMaglevOnNextCall(genMaglev);
{
  let g = genMaglev(5, 7);
  assertEquals({value: 57, done: false}, g.next());
  assertEquals({value: 58, done: false}, g.next());
  assertEquals({value: 59, done: false}, g.next());
  assertEquals({value: 60, done: false}, g.next());
  assertEquals({value: undefined, done: true}, g.next());
}

// 5. Deoptimization while suspended
function* genDeopt(x, y) {
  yield x + y;
  yield x * y;
  yield x - y;
}

%PrepareFunctionForOptimization(genDeopt);
{
  let g = genDeopt(7, 3);
  assertEquals({value: 10, done: false}, g.next());
  assertEquals({value: 21, done: false}, g.next());
  assertEquals({value: 4, done: false}, g.next());
  assertEquals({value: undefined, done: true}, g.next());
}

%OptimizeMaglevOnNextCall(genDeopt);
{
  let g = genDeopt(9, 4);
  assertEquals({value: 13, done: false}, g.next());
  %DeoptimizeFunction(genDeopt);
  assertEquals({value: 36, done: false}, g.next());
  assertEquals({value: 5, done: false}, g.next());
  assertEquals({value: undefined, done: true}, g.next());
}

// 6. Async function with branching awaits (first executed suspend has
// suspend_id > 0)
async function asyncBranching(flag, a, b) {
  if (flag) {
    await 1;
  } else {
    await 2;
  }
  return a + b;
}

%PrepareFunctionForOptimization(asyncBranching);
assertPromiseResult(asyncBranching(true, 10, 20), res => assertEquals(30, res));
assertPromiseResult(asyncBranching(false, 1, 2), res => assertEquals(3, res));
%PerformMicrotaskCheckpoint();
%OptimizeMaglevOnNextCall(asyncBranching);
assertPromiseResult(
    asyncBranching(false, 10, 20), res => assertEquals(30, res));
assertPromiseResult(asyncBranching(true, 3, 4), res => assertEquals(7, res));
%PerformMicrotaskCheckpoint();

// 7. Async function with switch across multiple awaits
async function asyncSwitch(mode, val) {
  switch (mode) {
    case 1:
      await Promise.resolve(1);
      break;
    case 2:
      await Promise.resolve(2);
      break;
    default:
      await Promise.resolve(3);
      break;
  }
  return val;
}

%PrepareFunctionForOptimization(asyncSwitch);
assertPromiseResult(asyncSwitch(1, "a"), res => assertEquals("a", res));
assertPromiseResult(asyncSwitch(2, "b"), res => assertEquals("b", res));
assertPromiseResult(asyncSwitch(3, "c"), res => assertEquals("c", res));
%PerformMicrotaskCheckpoint();
%OptimizeMaglevOnNextCall(asyncSwitch);
assertPromiseResult(asyncSwitch(1, "hello"), res => assertEquals("hello", res));
assertPromiseResult(asyncSwitch(2, "world"), res => assertEquals("world", res));
assertPromiseResult(asyncSwitch(3, "test"), res => assertEquals("test", res));
%PerformMicrotaskCheckpoint();

// 8. Async generator with unassigned and assigned parameters
async function* asyncGen(unassigned, assigned) {
  for (let i = 0; i < 3; i++) {
    assigned += 1;
    yield await Promise.resolve(unassigned + ":" + assigned);
  }
}

%PrepareFunctionForOptimization(asyncGen);
assertPromiseResult((async () => {
  let g1 = asyncGen("A", 0);
  assertEquals({value: "A:1", done: false}, await g1.next());
  assertEquals({value: "A:2", done: false}, await g1.next());
  assertEquals({value: "A:3", done: false}, await g1.next());
  assertEquals({value: undefined, done: true}, await g1.next());

  %OptimizeMaglevOnNextCall(asyncGen);
  let g2 = asyncGen("B", 10);
  assertEquals({value: "B:11", done: false}, await g2.next());
  assertEquals({value: "B:12", done: false}, await g2.next());
  assertEquals({value: "B:13", done: false}, await g2.next());
  assertEquals({value: undefined, done: true}, await g2.next());
})());

// 9. Async function deoptimization while suspended
async function asyncDeopt(flag, x, y) {
  if (flag) {
    await 1;
  } else {
    await 2;
  }
  return x * y;
}

%PrepareFunctionForOptimization(asyncDeopt);
asyncDeopt(true, 3, 4);
%OptimizeMaglevOnNextCall(asyncDeopt);
{
  let p = asyncDeopt(false, 5, 6);
  %DeoptimizeFunction(asyncDeopt);
  assertPromiseResult(p, res => assertEquals(30, res));
}

// 10. Only the first / only the last parameter assigned.
function* genFirstAssigned(a, b, c) {
  for (let i = 0; i < 3; i++) {
    a += 1;
    yield [a, b, c];
  }
}
function* genLastAssigned(a, b, c) {
  for (let i = 0; i < 3; i++) {
    c += 1;
    yield [a, b, c];
  }
}
for (let [fn, expected] of [
       [genFirstAssigned, [[2, 2, 3], [3, 2, 3], [4, 2, 3]]],
       [genLastAssigned, [[1, 2, 4], [1, 2, 5], [1, 2, 6]]]]) {
  %PrepareFunctionForOptimization(fn);
  assertEquals(expected, [...fn(1, 2, 3)]);
  %OptimizeMaglevOnNextCall(fn);
  assertEquals(expected, [...fn(1, 2, 3)]);
}

// 11. More than 32 parameters, with an assigned one past index 31.
function* genManyParams(
    p0, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16,
    p17, p18, p19, p20, p21, p22, p23, p24, p25, p26, p27, p28, p29, p30, p31,
    p32, p33, p34) {
  for (let i = 0; i < 3; i++) {
    p33 += 1;
    yield [p0, p31, p32, p33, p34];
  }
}
{
  const args = Array.from({length: 35}, (_, i) => i);
  const expected = [[0, 31, 32, 34, 34], [0, 31, 32, 35, 34],
                    [0, 31, 32, 36, 34]];
  %PrepareFunctionForOptimization(genManyParams);
  assertEquals(expected, [...genManyParams(...args)]);
  %OptimizeMaglevOnNextCall(genManyParams);
  assertEquals(expected, [...genManyParams(...args)]);
}
