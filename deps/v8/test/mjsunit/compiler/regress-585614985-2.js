// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function noop() {}
%NeverOptimizeFunction(noop);

const nullProtoObj = Object.setPrototypeOf({z: 1}, null);
const setterObj = Object.defineProperty({}, 'p', {set: Boolean});
function Ctor() {}
Object.setPrototypeOf(Ctor, null);
new Ctor();

function add4(x) { return x + 4; }
function add7(x) { return x + 7; }

function pickCallee(cond, val, fn1, fn2) {
  if (cond) {
    val + 2; val + 2; val + 2; val + 2; val + 2; val + 2;
    return fn1;
  }
  val + 3; val + 3; val + 3;
  return fn2;
}

function toRestArray(...args) { return args; }

function getNullProtoObj(val) {
  val + 1; val + 1; val + 1; val + 1; val + 1; val + 1;
  return nullProtoObj;
}

function test(cond, x, param) {
  let caught;
  let l0 = 0, l1 = 1, l2 = 2, l3 = 3, l4 = 4, l5 = 5,
      l6 = 6, l7 = 7, l8 = 8, l9 = 9, l10 = 10, l11 = 11;
  let acc = 0;
  let floor = Math.floor, now = Date.now, sink = noop;
  let obj = nullProtoObj, ctor = Ctor, s = setterObj;
  let args = toRestArray(x);
  let callee, fn1 = add4, fn2 = add7;
  let guard = 7, fetched = 7;

  try {
    let r = floor(...args);
    return cond ? args.length + r + acc : 3;
  } catch {
    caught = now(...args);
  }

  acc = caught === null;

  let i = 5;
  do {
    sink(l0, l1, l2, l3, l4, l5, l6, l7, l8, l9, l10, l11);
    fetched = getNullProtoObj(acc);
    if (fetched instanceof ctor) throw args;
    callee = pickCallee(cond, acc, fn1, fn2);
    guard = cond ? fetched : obj;
    param = caught;
    s.p = acc;
    while ((param = 0, obj instanceof ctor)) {
      callee = fn2;
      noop();
    }
    acc = callee(4);
    if (guard === null) return 0;
  } while (--i > 0);
  return caught === null ? 3 : acc;
}

%PrepareFunctionForOptimization(toRestArray);
%PrepareFunctionForOptimization(getNullProtoObj);
%PrepareFunctionForOptimization(add4);
%PrepareFunctionForOptimization(add7);
%PrepareFunctionForOptimization(pickCallee);
%PrepareFunctionForOptimization(test);

const sym = Symbol('x');
assertEquals(3, test(false, 1, 99));
assertEquals(2, test(true, 1, 99));
assertEquals(11, test(false, sym, 99));
assertEquals(8, test(true, sym, 99));

%OptimizeFunctionOnNextCall(test);
assertEquals(3, test(false, 1, 99));
assertEquals(2, test(true, 1, 99));
assertEquals(11, test(false, sym, 99));
assertEquals(8, test(true, sym, 99));
