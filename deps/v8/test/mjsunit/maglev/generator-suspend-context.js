// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev --expose-gc

// 1. Generator without inner context (no push context)
function* genNoInnerContext(o) {
  let a = 1;
  let b = 2;
  let c = o;
  yield a + b;
  yield c.val;
  return a + b + c.val;
}

%PrepareFunctionForOptimization(genNoInnerContext);
{
  let g = genNoInnerContext({val: 10});
  assertEquals({value: 3, done: false}, g.next());
  gc();
  assertEquals({value: 10, done: false}, g.next());
  gc();
  assertEquals({value: 13, done: true}, g.next());
}

%OptimizeMaglevOnNextCall(genNoInnerContext);
{
  let g = genNoInnerContext({val: 20});
  assertEquals({value: 3, done: false}, g.next());
  gc();
  assertEquals({value: 20, done: false}, g.next());
  gc();
  assertEquals({value: 23, done: true}, g.next());
}

// 2. Generator with inner context (push context via block/catch)
function* genWithInnerContext(o) {
  let outer_var = 100;
  try {
    let inner_captured = o.val;
    let closure = () => inner_captured;
    yield closure();
    gc();
    yield closure() + outer_var;
  } catch (e) {
    yield -1;
  }
}

%PrepareFunctionForOptimization(genWithInnerContext);
{
  let g = genWithInnerContext({val: 5});
  assertEquals({value: 5, done: false}, g.next());
  assertEquals({value: 105, done: false}, g.next());
  assertEquals({value: undefined, done: true}, g.next());
}

%OptimizeMaglevOnNextCall(genWithInnerContext);
{
  let g = genWithInnerContext({val: 7});
  assertEquals({value: 7, done: false}, g.next());
  assertEquals({value: 107, done: false}, g.next());
  assertEquals({value: undefined, done: true}, g.next());
}

// 3. Sloppy eval always forces a function context
function* genSloppyEval(a) {
  eval("var x = a");
  yield x;
  gc();
  yield x + 1;
}

function* genSloppyEvalInParams(a, b = eval("var y = a; y")) {
  yield b;
  gc();
  yield b + 1;
}

async function asyncSloppyEval(a) {
  eval("var x = a");
  await 0;
  gc();
  return x + 1;
}

for (let gen of [genSloppyEval, genSloppyEvalInParams]) {
  %PrepareFunctionForOptimization(gen);
  assertEquals([1, 2], [...gen(1)]);
  assertEquals([1, 2], [...gen(1)]);
  %OptimizeMaglevOnNextCall(gen);
  assertEquals([3, 4], [...gen(3)]);
}

%PrepareFunctionForOptimization(asyncSloppyEval);
asyncSloppyEval(1).then(v => assertEquals(2, v));
%OptimizeMaglevOnNextCall(asyncSloppyEval);
asyncSloppyEval(5).then(v => assertEquals(6, v));
