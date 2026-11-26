// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev --expose-gc

// 1. Generator without inner context (context load elided on resume)
function* genNoInnerContext(o, x) {
  let a = 1;
  let b = 2;
  yield a + b + x;
  gc();
  yield o.val + x;
  gc();
  return a + b + o.val + x;
}

%PrepareFunctionForOptimization(genNoInnerContext);
{
  let g = genNoInnerContext({val: 10}, 5);
  assertEquals({value: 8, done: false}, g.next());
  assertEquals({value: 15, done: false}, g.next());
  assertEquals({value: 18, done: true}, g.next());
}

%OptimizeMaglevOnNextCall(genNoInnerContext);
{
  let g = genNoInnerContext({val: 20}, 7);
  assertEquals({value: 10, done: false}, g.next());
  assertEquals({value: 27, done: false}, g.next());
  assertEquals({value: 30, done: true}, g.next());
}

// 2. Generator with inner context (context load preserved on resume)
function* genWithInnerContext(o) {
  let outer = 42;
  {
    let inner = o.val;
    let get_inner = () => inner + outer;
    yield get_inner();
    gc();
    yield get_inner() * 2;
  }
  return outer;
}

%PrepareFunctionForOptimization(genWithInnerContext);
{
  let g = genWithInnerContext({val: 3});
  assertEquals({value: 45, done: false}, g.next());
  assertEquals({value: 90, done: false}, g.next());
  assertEquals({value: 42, done: true}, g.next());
}

%OptimizeMaglevOnNextCall(genWithInnerContext);
{
  let g = genWithInnerContext({val: 5});
  assertEquals({value: 47, done: false}, g.next());
  assertEquals({value: 94, done: false}, g.next());
  assertEquals({value: 42, done: true}, g.next());
}
