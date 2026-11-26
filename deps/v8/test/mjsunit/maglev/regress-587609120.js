// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev

const tmp_it = [1][Symbol.iterator]();
tmp_it.extra = 1;

function B1(v) {
  this.a = 1;
  this.b = 1;
  this.c = v;
}
const b1a = new B1("hello");
const b1b = new B1({});
const b1_unstable = new B1("x");
b1_unstable.extra = 1;

function B2(v) {
  this.a = 1;
  this.b = 1;
  this.d = v;
}
const b2a = new B2(1);
const b2b = new B2(2);

const sideEffects = [() => 1, () => 2, () => 3, () => 4, () => 5];

function both(o1, o2, do_load, se) {
  if (do_load) {
    o1.c;
    se();
    const v = o2.d;
    return false == v;
  }
}

function foo(arr, do_load, se) {
  const it = arr[Symbol.iterator]();
  se();
  return both(it, it, do_load, se);
}

const arr = [1, 2, 3];

%PrepareFunctionForOptimization(both);
%PrepareFunctionForOptimization(foo);
for (const se of sideEffects) {
  both(b1a, b2a, true, se);
  both(b1b, b2b, true, se);
  foo(arr, false, se);
}
%OptimizeMaglevOnNextCall(foo);
assertEquals(undefined, foo(arr, false, sideEffects[0]));
assertMaglevved(foo);
assertEquals(false, foo(arr, true, sideEffects[0]));
