// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

class DoubleHolder extends Array {}

const box = { f2: false };
function double_getter() {
  let f2 = box.f2;
  let p0 = this[0];
  let p1 = f2 ? 1.5 : p0;
  if (p1 === 0.0) return 1;
  return p1;
}
function array_getter() {
  return this[0];
}
Object.defineProperty(DoubleHolder.prototype, "trigger", { get: double_getter });
Object.defineProperty(Array.prototype, "trigger", { get: array_getter });

function access(o) {
  return o.trigger;
}

function victim(...args) {
  return access(args);
}

const double_arr = new DoubleHolder();
double_arr[0] = 1.1;

%PrepareFunctionForOptimization(double_getter);
%PrepareFunctionForOptimization(array_getter);
%PrepareFunctionForOptimization(access);
%PrepareFunctionForOptimization(victim);

for (let i = 0; i < 10; i++) {
  box.f2 = (i % 2 === 0);
  access(double_arr);
  access([1, 2]);
}

assertEquals(1, victim(1, 2));
%OptimizeFunctionOnNextCall(victim);
assertEquals(1, victim(1, 2));
