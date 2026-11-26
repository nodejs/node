// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

'use strict';

function throwError() {
  throw 1;
}

// Bytecode size > 30 so it is deferred to JSInliningHeuristic::Finalize()
// instead of being inlined eagerly as a small function.
function noThrow() {
  let a = 1, b = 2, c = 3, d = 4, e = 5, f = 6, g = 7, h = 8, i = 9, j = 10,
      k = 11, l = 12;
  return a;
}

function nop() {}
let obj = {x: 1};

function inner(p, s, fn) {
  try {
    fn();
  } catch (e) {
    // s.startsWith(...arguments) is put on JSCallReducer's waitlist while
    // arguments is still used by Reflect.apply. When reduced during Finalize(),
    // it builds a loop (connecting a Terminate node to graph()->end()).
    s.startsWith(...arguments);
    Reflect.apply(nop, null, arguments);
  }
  // Keep Return from being directly connected to the try/catch Merge, so
  // collapsing the Merge does not revisit Return and End.
  obj.x = 2;
  return 1;
}

function outer(p, s) {
  return inner(p, s, noThrow);
}

%PrepareFunctionForOptimization(throwError);
%PrepareFunctionForOptimization(noThrow);
%PrepareFunctionForOptimization(nop);
%PrepareFunctionForOptimization(inner);
%PrepareFunctionForOptimization(outer);

inner('hello', 'hello world', throwError);
inner('hello', 'hello world', throwError);
assertEquals(1, outer('hello', 'hello world'));
assertEquals(1, outer('hello', 'hello world'));

%OptimizeFunctionOnNextCall(outer);
assertEquals(1, outer('hello', 'hello world'));
