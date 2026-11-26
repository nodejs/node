// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev --maglev-non-eager-inlining
// Flags: --min-maglev-inlining-frequency=0

const arr = [1, 2, 3];
arr.x = 1;

// Elided ConsString allocation: an InlinedAllocation without a static map.
function getCons(s) {
  return "bbbbbbbbbbbbbbbb" + (s + "aaaaaaaaaaaaaaaa");
}
%PrepareFunctionForOptimization(getCons);
%DisableEagerInlining(getCons);

function getArr() {
  return arr;
}
%PrepareFunctionForOptimization(getArr);

function callNext(it) {
  // Make the bytecode larger than getCons, so that getCons is inlined first.
  let a = 1, b = 2, c = 3, d = 4;
  if (a + b + c + d === 0) return 0;
  return it.next();
}
%PrepareFunctionForOptimization(callNext);
%DisableEagerInlining(callNext);

function foo(f, s, do_work) {
  let v = f(s);
  if (!do_work) return v;
  // The map check based on the getArr feedback makes v known to be a
  // JSReceiver, so the Array.prototype.values call below is reduced to an
  // InlinedAllocation JSArrayIterator. Inlining getCons later replaces v with
  // the ConsString allocation, which is then seen in ArrayIterator.p.next.
  v.x;
  let it = Array.prototype.values.call(v);
  return callNext(it);
}
%PrepareFunctionForOptimization(foo);

function bar(s, do_work) {
  return foo(getCons, s, do_work);
}
%PrepareFunctionForOptimization(bar);

assertEquals({value: 1, done: false}, foo(getArr, "x", true));
const expected = "bbbbbbbbbbbbbbbbxaaaaaaaaaaaaaaaa";
assertEquals(expected, bar("x", false));

%OptimizeMaglevOnNextCall(bar);
assertEquals(expected, bar("x", false));
assertMaglevved(bar);
