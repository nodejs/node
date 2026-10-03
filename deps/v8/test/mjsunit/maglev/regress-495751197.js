// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

let do_transition = false;
let marker = {x: 1.337};

Object.defineProperty(Object.prototype, '_leak', {
  get() {
    if (do_transition) {
      this[1][0] = marker;
    }
    return this;
  },
  configurable: true
});

function target(iter) {
  return iter.next().value;
}

function inner() {
  var leaked;
  // Forces WithContext creation. _leak triggers the runtime call.
  with (arguments) { leaked = _leak; }
  return target.apply(null, arguments);
}

function outer() {
  let a = [1.1, 2.2, 3.3];
  let iter = a.values();
  return inner(iter, a);
}

%PrepareFunctionForOptimization(target);
%PrepareFunctionForOptimization(inner);
%PrepareFunctionForOptimization(outer);

outer(); outer(); outer();

%OptimizeMaglevOnNextCall(outer);

do_transition = true;
let r = outer();
assertFalse(typeof r === "number");
