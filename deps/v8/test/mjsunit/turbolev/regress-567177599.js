// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev

var loop_value = 1.2;
var exit_value = 2.5;
loop_value = 1.4;
exit_value = 1.5;

const box = {value: 1.2};
const input = {[Symbol.toPrimitive]() { return box.value; }};

function store(n, input) {
  let value = 1.1;
  let sum = 0;
  while (n-- > 0) {
    sum += value;
    value = +input;
    loop_value = value;
  }
  exit_value = value;
  return sum;
}

%PrepareFunctionForOptimization(store);
store(3, input);

%OptimizeFunctionOnNextCall(store);
store(3, input);

box.value = 42;
store(3, input);
assertEquals(42, exit_value);
assertFalse(%IsSmi(exit_value));
