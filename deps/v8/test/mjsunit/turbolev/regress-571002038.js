// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev

function makeScopeAClosure(capturedA) {
  let a0 = capturedA;
  let a1 = a0 + 1;
  return function scopeAClosure() {
    let sum = a0 + a1;
    for (let i = 0; i < 4; i++) {
      sum += i;
      sum ^= i;
      sum += a0;
      sum ^= a1;
    }
    return sum;
  };
}

const closureA1 = makeScopeAClosure(1);
const closureA2 = makeScopeAClosure(2);

function makeScopeBFactory() {
  let capturedB = 10;
  return function closureFactory(whichScopeA) {
    let sum = capturedB + 1;
    if (whichScopeA === 1) return closureA1;
    if (whichScopeA === 2) return closureA2;
    return function scopeBClosure() {
      return sum;
    };
  };
}

const closureFactory = makeScopeBFactory();

function callClosureFromFactory(whichScopeA, callIt) {
  const fn = closureFactory(whichScopeA);
  if (callIt) {
    return fn();
  }
  return 0;
}

function triggerMismatch(callIt) {
  return callClosureFromFactory(0, callIt);
}

%PrepareFunctionForOptimization(closureA1);
%PrepareFunctionForOptimization(closureFactory);
%PrepareFunctionForOptimization(callClosureFromFactory);
%PrepareFunctionForOptimization(triggerMismatch);

closureFactory(1);
closureFactory(2);
closureFactory(0);

callClosureFromFactory(1, true);
callClosureFromFactory(2, true);

triggerMismatch(false);
%OptimizeFunctionOnNextCall(triggerMismatch);
triggerMismatch(true);
