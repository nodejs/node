// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev

class Base {}
function target1() { return 1; }
function target2() { return 2; }

class DerivedCall extends Base {
  constructor(flag, maxIter, doSuper, setFn, f) {
    let fn = f;
    let r = null;
    for (let i = 0; ; i++) {
      if (i >= 1 && flag) {
        r = this;
        break;
      }
      if (i >= maxIter) {
        throw 1;
      }
      if (doSuper && i === 0) {
        super();
      }
      if (setFn) {
        fn = target1;
      }
    }
    fn.call(r);
    return {};
  }
}

function triggerCall(flag) {
  return new DerivedCall(flag, 1, false, true, target1);
}

%PrepareFunctionForOptimization(DerivedCall);
%PrepareFunctionForOptimization(triggerCall);
for (let k = 0; k < 20; k++) {
  new DerivedCall(true, 2, true, false, target1);
  new DerivedCall(true, 2, true, false, target2);
  new DerivedCall(true, 2, true, true, target1);
  assertThrowsEquals(() => new DerivedCall(false, 1, false, true, target1), 1);
  assertThrows(() => triggerCall(true), ReferenceError);
  assertThrowsEquals(() => triggerCall(false), 1);
}
%OptimizeFunctionOnNextCall(triggerCall);
assertThrows(() => triggerCall(true), ReferenceError);
assertThrowsEquals(() => triggerCall(false), 1);

class DerivedToBool extends Base {
  constructor(flag, maxIter, doSuper) {
    let r = null;
    for (let i = 0; ; i++) {
      if (i >= 1 && flag) {
        r = this;
        break;
      }
      if (i >= maxIter) {
        throw 1;
      }
      if (doSuper && i === 0) {
        super();
      }
    }
    if (r || !r) {
      return {};
    }
    return {};
  }
}

function triggerToBool(flag) {
  return new DerivedToBool(flag, 1, false);
}

%PrepareFunctionForOptimization(DerivedToBool);
%PrepareFunctionForOptimization(triggerToBool);
for (let k = 0; k < 20; k++) {
  new DerivedToBool(true, 2, true);
  assertThrowsEquals(() => new DerivedToBool(false, 1, false), 1);
  assertThrows(() => triggerToBool(true), ReferenceError);
  assertThrowsEquals(() => triggerToBool(false), 1);
}
%OptimizeFunctionOnNextCall(triggerToBool);
assertThrows(() => triggerToBool(true), ReferenceError);
assertThrowsEquals(() => triggerToBool(false), 1);

class DerivedPostLoopCheck extends Base {
  constructor(maxIter, doSuper) {
    for (let i = 0; ; i++) {
      if (i >= 1) {
        break;
      }
      if (i >= maxIter) {
        throw 1;
      }
      if (doSuper && i === 0) {
        super();
      }
    }
    return this;
  }
}

function triggerPostLoopCheck() {
  return new DerivedPostLoopCheck(2, false);
}

%PrepareFunctionForOptimization(DerivedPostLoopCheck);
%PrepareFunctionForOptimization(triggerPostLoopCheck);
for (let k = 0; k < 20; k++) {
  new DerivedPostLoopCheck(2, true);
  assertThrows(() => triggerPostLoopCheck(), ReferenceError);
}
%OptimizeFunctionOnNextCall(triggerPostLoopCheck);
assertThrows(() => triggerPostLoopCheck(), ReferenceError);
