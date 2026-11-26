// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// Exercises receiver conversion in the CallFunction builtins for all
// ConvertReceiverModes, combined with argument adaptation.

const global = this;

function sloppy0() { return [this]; }
function sloppy1(a) { return [this, a]; }
function sloppy3(a, b, c) { return [this, a, b, c]; }
function strict0() { 'use strict'; return [this]; }
function strict1(a) { 'use strict'; return [this, a]; }
function strict3(a, b, c) { 'use strict'; return [this, a, b, c]; }
class C { m(a, b) { return [this, a, b]; } }
const classMethod = C.prototype.m;

const obj = {};
const proxy = new Proxy({}, {});
const sym = Symbol('s');
const receivers = [undefined, null, 1, -0, 1.5, 'str', true, sym, 10n, obj,
                   proxy, sloppy0, [], new Number(3)];

function isPrimitive(r) {
  return r === null || (typeof r !== 'object' && typeof r !== 'function');
}

function checkThis(actual, isStrict, r) {
  if (isStrict || !isPrimitive(r)) {
    assertSame(r, actual);
  } else if (r === undefined || r === null) {
    assertSame(global, actual);
  } else {
    assertEquals('object', typeof actual);
    assertSame(Object.getPrototypeOf(Object(r)), Object.getPrototypeOf(actual));
    assertTrue(Object.is(r, actual.valueOf()));
  }
}

// ConvertReceiverMode::kAny (Function.prototype.call / Reflect.apply).
function callAny(f, r, ...args) { return f.call(r, ...args); }
function applyAny(f, r, args) { return Reflect.apply(f, r, args); }

// ConvertReceiverMode::kNullOrUndefined (plain calls).
function callUndefined(f, a, b) { return f(a, b); }

// ConvertReceiverMode::kNotNullOrUndefined (property calls).
function callProperty(r, a, b) { return r.f(a, b); }

const functions = [
  [sloppy0, false], [sloppy1, false], [sloppy3, false],
  [strict0, true], [strict1, true], [strict3, true], [classMethod, true],
];

function test() {
  for (const [f, isStrict] of functions) {
    for (const r of receivers) {
      for (let argc = 0; argc <= 4; argc++) {
        const args = [];
        for (let i = 0; i < argc; i++) args.push(i + 100);
        for (const result of [callAny(f, r, ...args), applyAny(f, r, args)]) {
          const [actual, ...rest] = result;
          checkThis(actual, isStrict, r);
          for (let i = 0; i < rest.length; i++) {
            assertSame(i < argc ? i + 100 : undefined, rest[i]);
          }
        }
      }
    }

    const [actual, a, b] = callUndefined(f, 1, 2);
    assertSame(isStrict ? undefined : global, actual);
    if (f.length > 0) assertSame(1, a);
    if (f.length > 1) assertSame(2, b);

    for (const r of [1, 'str', true, sym, 10n]) {
      const proto = Object.getPrototypeOf(Object(r));
      proto.f = f;
      try {
        const [actual] = callProperty(r, 1, 2);
        checkThis(actual, isStrict, r);
      } finally {
        delete proto.f;
      }
    }
    for (const r of [obj, proxy, []]) {
      const o = Object.create(r);
      o.f = f;
      assertSame(o, callProperty(o, 1, 2)[0]);
    }
  }
}

%PrepareFunctionForOptimization(callAny);
%PrepareFunctionForOptimization(applyAny);
%PrepareFunctionForOptimization(callUndefined);
%PrepareFunctionForOptimization(callProperty);
test();
test();
%OptimizeFunctionOnNextCall(callAny);
%OptimizeFunctionOnNextCall(applyAny);
%OptimizeFunctionOnNextCall(callUndefined);
%OptimizeFunctionOnNextCall(callProperty);
test();
