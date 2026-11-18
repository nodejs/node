// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev-non-eager-inlining
// Flags: --max-maglev-eager-inlined-bytecode-size=0

function isZero(a) {
  return a === 0;
}
function callee1(v) {
  if (v) return 1;
  return 0;
}
function callee2(v) {
  let r = 0;
  if (!v) r = 1;
  return r;
}
function sloppyCallee() {
  return typeof this;
}
function strictThis() {
  "use strict";
  return this;
}

%PrepareFunctionForOptimization(isZero);
%PrepareFunctionForOptimization(callee1);
%PrepareFunctionForOptimization(callee2);
%PrepareFunctionForOptimization(sloppyCallee);
%PrepareFunctionForOptimization(strictThis);

isZero(0);
isZero(1);
callee1(1);
callee1(0);
callee2(1);
callee2(0);
sloppyCallee();
strictThis();

// 1. BranchIfToBooleanTrue with hole constant input.
function innerBranch(n, init) {
  switch (init) {
    case 1:
      let x = 1;
    case 0:
      while (x && n-- > 0) {
        do {} while (init > 1);
        if (isZero(init)) throw 1;
        x = 1;
      }
      if (x) return 1;
      return 0;
  }
}
function testBranch(n) {
  return innerBranch(n, 0);
}
%PrepareFunctionForOptimization(innerBranch);
%PrepareFunctionForOptimization(testBranch);
assertEquals(1, innerBranch(2, 1));
assertThrows(() => testBranch(2), ReferenceError);
%OptimizeFunctionOnNextCall(testBranch);
assertThrows(() => testBranch(2), ReferenceError);

// 2. ToBoolean with hole constant input.
function innerToBoolean(n, init) {
  switch (init) {
    case 1:
      let x = 1;
    case 0:
      while (x && n-- > 0) {
        do {} while (init > 1);
        if (isZero(init)) throw 1;
        x = 1;
      }
      return !!x;
  }
}
function testToBoolean(n) {
  return innerToBoolean(n, 0);
}
%PrepareFunctionForOptimization(innerToBoolean);
%PrepareFunctionForOptimization(testToBoolean);
assertEquals(true, innerToBoolean(2, 1));
assertThrows(() => testToBoolean(2), ReferenceError);
%OptimizeFunctionOnNextCall(testToBoolean);
assertThrows(() => testToBoolean(2), ReferenceError);

// 3. ToBooleanLogicalNot with hole constant input.
function innerToBooleanNot(n, init) {
  switch (init) {
    case 1:
      let x = 1;
    case 0:
      while (x && n-- > 0) {
        do {} while (init > 1);
        if (isZero(init)) throw 1;
        x = 1;
      }
      return !x;
  }
}
function testToBooleanNot(n) {
  return innerToBooleanNot(n, 0);
}
%PrepareFunctionForOptimization(innerToBooleanNot);
%PrepareFunctionForOptimization(testToBooleanNot);
assertEquals(false, innerToBooleanNot(2, 1));
assertThrows(() => testToBooleanNot(2), ReferenceError);
%OptimizeFunctionOnNextCall(testToBooleanNot);
assertThrows(() => testToBooleanNot(2), ReferenceError);

// 4. LogicalNot with hole constant input.
function innerLogicalNot(n, init) {
  switch (init) {
    case 1:
      let x = true;
    case 0:
      while ((x, x.valueOf(), n-- > 0)) {
        do {} while (init > 1);
        if (isZero(init)) throw 1;
        x = true;
      }
      return !x;
  }
}
function testLogicalNot(n) {
  return innerLogicalNot(n, 0);
}
%PrepareFunctionForOptimization(innerLogicalNot);
%PrepareFunctionForOptimization(testLogicalNot);
assertEquals(false, innerLogicalNot(2, 1));
assertThrows(() => testLogicalNot(2), ReferenceError);
%OptimizeFunctionOnNextCall(testLogicalNot);
assertThrows(() => testLogicalNot(2), ReferenceError);

// 5. Call inlining JumpIfToBooleanFalse / JumpIfToBooleanTrue with hole arg.
function innerCallArg1(n, init, f) {
  switch (init) {
    case 1:
      let x = 1;
    case 0:
      while (x && n-- > 0) {
        do {} while (init > 1);
        if (isZero(init)) throw 1;
        x = 1;
      }
      return f(x);
  }
}
function testCallArg1(n) {
  return innerCallArg1(n, 0, callee1);
}
%PrepareFunctionForOptimization(innerCallArg1);
%PrepareFunctionForOptimization(testCallArg1);
assertEquals(1, innerCallArg1(2, 1, callee1));
assertEquals(0, innerCallArg1(2, 1, callee2));
assertThrows(() => testCallArg1(2), ReferenceError);
%OptimizeFunctionOnNextCall(testCallArg1);
assertThrows(() => testCallArg1(2), ReferenceError);

function innerCallArg2(n, init, f) {
  switch (init) {
    case 1:
      let x = 1;
    case 0:
      while (x && n-- > 0) {
        do {} while (init > 1);
        if (isZero(init)) throw 1;
        x = 1;
      }
      return f(x);
  }
}
function testCallArg2(n) {
  return innerCallArg2(n, 0, callee2);
}
%PrepareFunctionForOptimization(innerCallArg2);
%PrepareFunctionForOptimization(testCallArg2);
assertEquals(1, innerCallArg2(2, 1, callee1));
assertEquals(0, innerCallArg2(2, 1, callee2));
assertThrows(() => testCallArg2(2), ReferenceError);
%OptimizeFunctionOnNextCall(testCallArg2);
assertThrows(() => testCallArg2(2), ReferenceError);

// 6. Call with CheckedInternalizedString(tdz_hole) receiver.
function innerCallCheckedStringReceiver(n, init, f) {
  switch (init) {
    case 1:
      let x = "a";
    case 0:
      while ((x === "a", n-- > 0)) {
        do {} while (init > 1);
        if (isZero(init)) throw 1;
        x = "b";
      }
      return f.call(x);
  }
}
function testCallCheckedStringReceiver(n, f) {
  return innerCallCheckedStringReceiver(n, 0, f);
}
%PrepareFunctionForOptimization(innerCallCheckedStringReceiver);
%PrepareFunctionForOptimization(testCallCheckedStringReceiver);
assertEquals("object", innerCallCheckedStringReceiver(2, 1, sloppyCallee));
assertEquals("b", innerCallCheckedStringReceiver(2, 1, strictThis));
assertThrows(
    () => testCallCheckedStringReceiver(2, sloppyCallee), ReferenceError);
assertThrows(
    () => testCallCheckedStringReceiver(2, strictThis), ReferenceError);
%OptimizeFunctionOnNextCall(testCallCheckedStringReceiver);
assertThrows(
    () => testCallCheckedStringReceiver(2, sloppyCallee), ReferenceError);
