// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turboshaft

function makeDictionaryObject() {
  let obj = {};
  for (let i = 0; i < 1000; i++) {
    obj["p" + i] = i;
  }
  return obj;
}

let obj = makeDictionaryObject();
Object.defineProperty(obj, "throw_prop", {
  get: function () {
    throw new Error("expected failure");
  },
  configurable: true,
});

// Case 1: Surrounding try-catch inside the optimized function.
function foo(o) {
  try {
    return o.throw_prop;
  } catch (e) {
    return e.message;
  }
}

%PrepareFunctionForOptimization(foo);
assertEquals("expected failure", foo(obj));
assertEquals("expected failure", foo(obj));

%OptimizeFunctionOnNextCall(foo);
assertEquals("expected failure", foo(obj));

// Case 2a: Catching the exception at the top level.
function bar(o) {
  return o.throw_prop;
}

%PrepareFunctionForOptimization(bar);
assertThrows(() => bar(obj), Error, "expected failure");
assertThrows(() => bar(obj), Error, "expected failure");

%OptimizeFunctionOnNextCall(bar);
assertThrows(() => bar(obj), Error, "expected failure");

// Case 2b: Catching the exception in the caller.
function baz(o) {
  return o.throw_prop;
}

function bazCaller(o) {
  try {
    return baz(o);
  } catch (e) {
    return e.message;
  }
}

%PrepareFunctionForOptimization(baz);
assertEquals("expected failure", bazCaller(obj));
assertEquals("expected failure", bazCaller(obj));

%OptimizeFunctionOnNextCall(baz);
assertEquals("expected failure", bazCaller(obj));

// Case 3: Fallback path triggers lazy deoptimization of the callee.
function deoptCallee(o) {
  return o.deopt_prop;
}

Object.defineProperty(obj, "deopt_prop", {
  get: function () {
    %DeoptimizeFunction(deoptCallee);
    return 42;
  },
  configurable: true,
});

%PrepareFunctionForOptimization(deoptCallee);
assertEquals(42, deoptCallee(obj));
assertEquals(42, deoptCallee(obj));

%OptimizeFunctionOnNextCall(deoptCallee);
assertEquals(42, deoptCallee(obj));
assertUnoptimized(deoptCallee);
