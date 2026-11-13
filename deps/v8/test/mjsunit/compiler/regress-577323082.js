// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan
// Flags: --no-optimize-on-next-call-optimizes-to-maglev --no-turbolev

"use strict";

function C() {}
function nop() {}

function alwaysThrows(x) {
  if (x) throw 1;
  throw x.a + x.b + x.c + x.d + x.e + x.f + x.g + x.h;
}

function inner(ctor, fn) {
  fn(1);
  while (ctor) {
    new ctor(...arguments);
    Reflect.construct(0, arguments);
  }
}

function outer(ctor) {
  inner(ctor, alwaysThrows);
}

%PrepareFunctionForOptimization(C);
%PrepareFunctionForOptimization(nop);
%PrepareFunctionForOptimization(alwaysThrows);
%PrepareFunctionForOptimization(inner);
%PrepareFunctionForOptimization(outer);

assertThrows(() => inner(C, nop), TypeError);
assertThrows(() => outer(C));

%OptimizeFunctionOnNextCall(outer);
assertThrows(() => outer(C));

function ext() {}
%NeverOptimizeFunction(ext);

function cand1(a, b, c, d, e, f, o) {
  if (a && b && c && d && e && f) return delete o[a];
  return delete o[b];
}

function cand2() {
  ext(); ext(); ext(); ext(); ext();
  return %IsBeingInterpreted();
}

function caller(a, b, c, d, e, f, o) {
  ext();
  const r2 = cand2();
  ext();
  const r1 = cand1(a, b, c, d, e, f, o);
  return r1 ? r2 : true;
}

const obj = {};
%PrepareFunctionForOptimization(cand1);
%PrepareFunctionForOptimization(cand2);
%PrepareFunctionForOptimization(caller);
assertTrue(caller(1, 2, true, true, true, true, obj));
assertTrue(caller(0, 2, true, true, true, true, obj));
%OptimizeFunctionOnNextCall(caller);
assertFalse(caller(1, 2, true, true, true, true, obj));
