// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev

function bit_or(v) {
  return v | 0;
}

function foo(cond, exit_early) {
  let x = cond ? 1 : 'a';
  let res = 0;
  while (true) {
    for (let j = 0; j < 1; j++) {}
    if (exit_early) {
      res = bit_or(x);
      break;
    }
    x = 2;
    exit_early = true;
  }
  return Object.is(x, 1) ? res : -1;
}

%PrepareFunctionForOptimization(bit_or);
bit_or(1);
bit_or(undefined);

%PrepareFunctionForOptimization(foo);
assertEquals(-1, foo(false, false));
assertEquals(1, foo(true, true));

%OptimizeMaglevOnNextCall(foo);
assertEquals(1, foo(true, true));
assertEquals(-1, foo(false, false));
assertEquals(-1, foo(false, true));

function index_of(s) {
  return '12'.indexOf(s);
}

function bar(exit_early) {
  let x = 1;
  let res = 0;
  while (true) {
    for (let j = 0; j < 1; j++) {}
    if (exit_early) {
      res = index_of(x);
      break;
    }
    x = 2;
    exit_early = true;
  }
  return (x | 0) + res;
}

%PrepareFunctionForOptimization(index_of);
index_of('1');

%PrepareFunctionForOptimization(bar);
assertEquals(3, bar(false));

%OptimizeMaglevOnNextCall(bar);
assertEquals(3, bar(false));
assertEquals(1, bar(true));
