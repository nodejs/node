// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --maglev
// Flags: --max-turbolev-eager-inlined-bytecode-size=0

// `inner` is large enough that the inliner picks it over the values callsite,
// and with eager inlining disabled it is inlined lazily by the optimizer, so
// the Array.prototype.values call is reduced post-inline by the reducer (inline
// allocation of the JSArrayIterator).
function inner(arr) {
  for (let i = 0; i < 5; i++);
  return arr.values();
}

function foo(arr) {
  return inner(arr);
}

const arr = [11, 22, 33];

function check(it) {
  let e = it.next();
  assertFalse(e.done);
  assertEquals(11, e.value);
  e = it.next();
  assertFalse(e.done);
  assertEquals(22, e.value);
  e = it.next();
  assertFalse(e.done);
  assertEquals(33, e.value);
  e = it.next();
  assertTrue(e.done);
  assertEquals(undefined, e.value);
}

%PrepareFunctionForOptimization(inner);
%PrepareFunctionForOptimization(foo);
check(foo(arr));
check(foo(arr));

%OptimizeFunctionOnNextCall(foo);
check(foo(arr));
assertOptimized(foo);
