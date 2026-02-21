// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function addBenchmark(name, test) {
  new BenchmarkSuite(name, [1000],
      [
        new Benchmark(name, false, false, 0, test)
      ]);
}

addBenchmark('Smi-Or', orSmi);
addBenchmark('Number-Or', orNumber);
addBenchmark('Smi-Xor', xorSmi);
addBenchmark('Number-Xor', xorNumber);
addBenchmark('Smi-And', andSmi);
addBenchmark('Number-And', andNumber);
addBenchmark('Smi-Constant-Or', orSmiConstant);
addBenchmark('Smi-Constant-Xor', xorSmiConstant);
addBenchmark('Smi-Constant-And', andSmiConstant);
addBenchmark('Smi-ShiftLeft', shiftLeftSmi);
addBenchmark('Number-ShiftLeft', shiftLeftNumber);
addBenchmark('Smi-ShiftRight', shiftRightSmi);
addBenchmark('Number-ShiftRight', shiftRightNumber);
addBenchmark('Smi-ShiftRightLogical', shiftRightLogicalSmi);
addBenchmark('Number-ShiftRightLogical', shiftRightLogicalNumber);
addBenchmark('Smi-Constant-ShiftLeft', shiftLeftSmiConstant);
addBenchmark('Smi-Constant-ShiftRight', shiftRightSmiConstant);
addBenchmark('Smi-Constant-ShiftRightLogical', shiftRightLogicalSmiConstant);


function bitwiseOr(a, b) {
  for (var i = 0; i < 1000; ++i) {
    a | b; a | b; a | b; a | b; a | b; a | b; a | b; a | b; a | b; a | b;
    a | b; a | b; a | b; a | b; a | b; a | b; a | b; a | b; a | b; a | b;
    a | b; a | b; a | b; a | b; a | b; a | b; a | b; a | b; a | b; a | b;
    a | b; a | b; a | b; a | b; a | b; a | b; a | b; a | b; a | b; a | b;
    a | b; a | b; a | b; a | b; a | b; a | b; a | b; a | b; a | b; a | b;
    a | b; a | b; a | b; a | b; a | b; a | b; a | b; a | b; a | b; a | b;
    a | b; a | b; a | b; a | b; a | b; a | b; a | b; a | b; a | b; a | b;
    a | b; a | b; a | b; a | b; a | b; a | b; a | b; a | b; a | b; a | b;
    a | b; a | b; a | b; a | b; a | b; a | b; a | b; a | b; a | b; a | b;
    a | b; a | b; a | b; a | b; a | b; a | b; a | b; a | b; a | b; a | b;
  }
}

function bitwiseXor(a, b) {
  for (var i = 0; i < 1000; ++i) {
    a ^ b; a ^ b; a ^ b; a ^ b; a ^ b; a ^ b; a ^ b; a ^ b; a ^ b; a ^ b;
    a ^ b; a ^ b; a ^ b; a ^ b; a ^ b; a ^ b; a ^ b; a ^ b; a ^ b; a ^ b;
    a ^ b; a ^ b; a ^ b; a ^ b; a ^ b; a ^ b; a ^ b; a ^ b; a ^ b; a ^ b;
    a ^ b; a ^ b; a ^ b; a ^ b; a ^ b; a ^ b; a ^ b; a ^ b; a ^ b; a ^ b;
    a ^ b; a ^ b; a ^ b; a ^ b; a ^ b; a ^ b; a ^ b; a ^ b; a ^ b; a ^ b;
    a ^ b; a ^ b; a ^ b; a ^ b; a ^ b; a ^ b; a ^ b; a ^ b; a ^ b; a ^ b;
    a ^ b; a ^ b; a ^ b; a ^ b; a ^ b; a ^ b; a ^ b; a ^ b; a ^ b; a ^ b;
    a ^ b; a ^ b; a ^ b; a ^ b; a ^ b; a ^ b; a ^ b; a ^ b; a ^ b; a ^ b;
    a ^ b; a ^ b; a ^ b; a ^ b; a ^ b; a ^ b; a ^ b; a ^ b; a ^ b; a ^ b;
    a ^ b; a ^ b; a ^ b; a ^ b; a ^ b; a ^ b; a ^ b; a ^ b; a ^ b; a ^ b;
  }
}

function bitwiseAnd(a, b) {
  for (var i = 0; i < 1000; ++i) {
    a & b; a & b; a & b; a & b; a & b; a & b; a & b; a & b; a & b; a & b;
    a & b; a & b; a & b; a & b; a & b; a & b; a & b; a & b; a & b; a & b;
    a & b; a & b; a & b; a & b; a & b; a & b; a & b; a & b; a & b; a & b;
    a & b; a & b; a & b; a & b; a & b; a & b; a & b; a & b; a & b; a & b;
    a & b; a & b; a & b; a & b; a & b; a & b; a & b; a & b; a & b; a & b;
    a & b; a & b; a & b; a & b; a & b; a & b; a & b; a & b; a & b; a & b;
    a & b; a & b; a & b; a & b; a & b; a & b; a & b; a & b; a & b; a & b;
    a & b; a & b; a & b; a & b; a & b; a & b; a & b; a & b; a & b; a & b;
    a & b; a & b; a & b; a & b; a & b; a & b; a & b; a & b; a & b; a & b;
    a & b; a & b; a & b; a & b; a & b; a & b; a & b; a & b; a & b; a & b;
  }
}

function shiftLeft(a, b) {
  for (var i = 0; i < 1000; ++i) {
    a << b; a << b; a << b; a << b; a << b; a << b; a << b; a << b; a << b; a << b;
    a << b; a << b; a << b; a << b; a << b; a << b; a << b; a << b; a << b; a << b;
    a << b; a << b; a << b; a << b; a << b; a << b; a << b; a << b; a << b; a << b;
    a << b; a << b; a << b; a << b; a << b; a << b; a << b; a << b; a << b; a << b;
    a << b; a << b; a << b; a << b; a << b; a << b; a << b; a << b; a << b; a << b;
    a << b; a << b; a << b; a << b; a << b; a << b; a << b; a << b; a << b; a << b;
    a << b; a << b; a << b; a << b; a << b; a << b; a << b; a << b; a << b; a << b;
    a << b; a << b; a << b; a << b; a << b; a << b; a << b; a << b; a << b; a << b;
    a << b; a << b; a << b; a << b; a << b; a << b; a << b; a << b; a << b; a << b;
    a << b; a << b; a << b; a << b; a << b; a << b; a << b; a << b; a << b; a << b;
  }
}

function shiftRight(a, b) {
  for (var i = 0; i < 1000; ++i) {
    a >> b; a >> b; a >> b; a >> b; a >> b; a >> b; a >> b; a >> b; a >> b; a >> b;
    a >> b; a >> b; a >> b; a >> b; a >> b; a >> b; a >> b; a >> b; a >> b; a >> b;
    a >> b; a >> b; a >> b; a >> b; a >> b; a >> b; a >> b; a >> b; a >> b; a >> b;
    a >> b; a >> b; a >> b; a >> b; a >> b; a >> b; a >> b; a >> b; a >> b; a >> b;
    a >> b; a >> b; a >> b; a >> b; a >> b; a >> b; a >> b; a >> b; a >> b; a >> b;
    a >> b; a >> b; a >> b; a >> b; a >> b; a >> b; a >> b; a >> b; a >> b; a >> b;
    a >> b; a >> b; a >> b; a >> b; a >> b; a >> b; a >> b; a >> b; a >> b; a >> b;
    a >> b; a >> b; a >> b; a >> b; a >> b; a >> b; a >> b; a >> b; a >> b; a >> b;
    a >> b; a >> b; a >> b; a >> b; a >> b; a >> b; a >> b; a >> b; a >> b; a >> b;
    a >> b; a >> b; a >> b; a >> b; a >> b; a >> b; a >> b; a >> b; a >> b; a >> b;
  }
}

function shiftRightLogical(a, b) {
  for (var i = 0; i < 1000; ++i) {
    a >>> b; a >>> b; a >>> b; a >>> b; a >>> b; a >>> b; a >>> b; a >>> b; a >>> b; a >>> b;
    a >>> b; a >>> b; a >>> b; a >>> b; a >>> b; a >>> b; a >>> b; a >>> b; a >>> b; a >>> b;
    a >>> b; a >>> b; a >>> b; a >>> b; a >>> b; a >>> b; a >>> b; a >>> b; a >>> b; a >>> b;
    a >>> b; a >>> b; a >>> b; a >>> b; a >>> b; a >>> b; a >>> b; a >>> b; a >>> b; a >>> b;
    a >>> b; a >>> b; a >>> b; a >>> b; a >>> b; a >>> b; a >>> b; a >>> b; a >>> b; a >>> b;
    a >>> b; a >>> b; a >>> b; a >>> b; a >>> b; a >>> b; a >>> b; a >>> b; a >>> b; a >>> b;
    a >>> b; a >>> b; a >>> b; a >>> b; a >>> b; a >>> b; a >>> b; a >>> b; a >>> b; a >>> b;
    a >>> b; a >>> b; a >>> b; a >>> b; a >>> b; a >>> b; a >>> b; a >>> b; a >>> b; a >>> b;
    a >>> b; a >>> b; a >>> b; a >>> b; a >>> b; a >>> b; a >>> b; a >>> b; a >>> b; a >>> b;
    a >>> b; a >>> b; a >>> b; a >>> b; a >>> b; a >>> b; a >>> b; a >>> b; a >>> b; a >>> b;
  }
}

function orSmiConstant() {
  var a = 20;
  for (var i = 0; i < 1000; ++i) {
    a | 10; a | 10; a | 10; a | 10; a | 10; a | 10; a | 10; a | 10; a | 10; a | 10;
    a | 10; a | 10; a | 10; a | 10; a | 10; a | 10; a | 10; a | 10; a | 10; a | 10;
    a | 10; a | 10; a | 10; a | 10; a | 10; a | 10; a | 10; a | 10; a | 10; a | 10;
    a | 10; a | 10; a | 10; a | 10; a | 10; a | 10; a | 10; a | 10; a | 10; a | 10;
    a | 10; a | 10; a | 10; a | 10; a | 10; a | 10; a | 10; a | 10; a | 10; a | 10;
    a | 10; a | 10; a | 10; a | 10; a | 10; a | 10; a | 10; a | 10; a | 10; a | 10;
    a | 10; a | 10; a | 10; a | 10; a | 10; a | 10; a | 10; a | 10; a | 10; a | 10;
    a | 10; a | 10; a | 10; a | 10; a | 10; a | 10; a | 10; a | 10; a | 10; a | 10;
    a | 10; a | 10; a | 10; a | 10; a | 10; a | 10; a | 10; a | 10; a | 10; a | 10;
    a | 10; a | 10; a | 10; a | 10; a | 10; a | 10; a | 10; a | 10; a | 10; a | 10;
  }
}

function xorSmiConstant() {
  var a = 20;
  for (var i = 0; i < 1000; ++i) {
    a ^ 10; a ^ 10; a ^ 10; a ^ 10; a ^ 10; a ^ 10; a ^ 10; a ^ 10; a ^ 10; a ^ 10;
    a ^ 10; a ^ 10; a ^ 10; a ^ 10; a ^ 10; a ^ 10; a ^ 10; a ^ 10; a ^ 10; a ^ 10;
    a ^ 10; a ^ 10; a ^ 10; a ^ 10; a ^ 10; a ^ 10; a ^ 10; a ^ 10; a ^ 10; a ^ 10;
    a ^ 10; a ^ 10; a ^ 10; a ^ 10; a ^ 10; a ^ 10; a ^ 10; a ^ 10; a ^ 10; a ^ 10;
    a ^ 10; a ^ 10; a ^ 10; a ^ 10; a ^ 10; a ^ 10; a ^ 10; a ^ 10; a ^ 10; a ^ 10;
    a ^ 10; a ^ 10; a ^ 10; a ^ 10; a ^ 10; a ^ 10; a ^ 10; a ^ 10; a ^ 10; a ^ 10;
    a ^ 10; a ^ 10; a ^ 10; a ^ 10; a ^ 10; a ^ 10; a ^ 10; a ^ 10; a ^ 10; a ^ 10;
    a ^ 10; a ^ 10; a ^ 10; a ^ 10; a ^ 10; a ^ 10; a ^ 10; a ^ 10; a ^ 10; a ^ 10;
    a ^ 10; a ^ 10; a ^ 10; a ^ 10; a ^ 10; a ^ 10; a ^ 10; a ^ 10; a ^ 10; a ^ 10;
    a ^ 10; a ^ 10; a ^ 10; a ^ 10; a ^ 10; a ^ 10; a ^ 10; a ^ 10; a ^ 10; a ^ 10;
  }
}

function andSmiConstant() {
  var a = 20;
  for (var i = 0; i < 1000; ++i) {
    a & 10; a & 10; a & 10; a & 10; a & 10; a & 10; a & 10; a & 10; a & 10; a & 10;
    a & 10; a & 10; a & 10; a & 10; a & 10; a & 10; a & 10; a & 10; a & 10; a & 10;
    a & 10; a & 10; a & 10; a & 10; a & 10; a & 10; a & 10; a & 10; a & 10; a & 10;
    a & 10; a & 10; a & 10; a & 10; a & 10; a & 10; a & 10; a & 10; a & 10; a & 10;
    a & 10; a & 10; a & 10; a & 10; a & 10; a & 10; a & 10; a & 10; a & 10; a & 10;
    a & 10; a & 10; a & 10; a & 10; a & 10; a & 10; a & 10; a & 10; a & 10; a & 10;
    a & 10; a & 10; a & 10; a & 10; a & 10; a & 10; a & 10; a & 10; a & 10; a & 10;
    a & 10; a & 10; a & 10; a & 10; a & 10; a & 10; a & 10; a & 10; a & 10; a & 10;
    a & 10; a & 10; a & 10; a & 10; a & 10; a & 10; a & 10; a & 10; a & 10; a & 10;
    a & 10; a & 10; a & 10; a & 10; a & 10; a & 10; a & 10; a & 10; a & 10; a & 10;
  }
}

function shiftLeftSmiConstant() {
  var a = 20;
  for (var i = 0; i < 1000; ++i) {
    a << 10; a << 10; a << 10; a << 10; a << 10; a << 10; a << 10; a << 10; a << 10; a << 10;
    a << 10; a << 10; a << 10; a << 10; a << 10; a << 10; a << 10; a << 10; a << 10; a << 10;
    a << 10; a << 10; a << 10; a << 10; a << 10; a << 10; a << 10; a << 10; a << 10; a << 10;
    a << 10; a << 10; a << 10; a << 10; a << 10; a << 10; a << 10; a << 10; a << 10; a << 10;
    a << 10; a << 10; a << 10; a << 10; a << 10; a << 10; a << 10; a << 10; a << 10; a << 10;
    a << 10; a << 10; a << 10; a << 10; a << 10; a << 10; a << 10; a << 10; a << 10; a << 10;
    a << 10; a << 10; a << 10; a << 10; a << 10; a << 10; a << 10; a << 10; a << 10; a << 10;
    a << 10; a << 10; a << 10; a << 10; a << 10; a << 10; a << 10; a << 10; a << 10; a << 10;
    a << 10; a << 10; a << 10; a << 10; a << 10; a << 10; a << 10; a << 10; a << 10; a << 10;
    a << 10; a << 10; a << 10; a << 10; a << 10; a << 10; a << 10; a << 10; a << 10; a << 10;
  }
}

function shiftRightSmiConstant() {
  var a = 20;
  for (var i = 0; i < 1000; ++i) {
    a >> 10; a >> 10; a >> 10; a >> 10; a >> 10; a >> 10; a >> 10; a >> 10; a >> 10; a >> 10;
    a >> 10; a >> 10; a >> 10; a >> 10; a >> 10; a >> 10; a >> 10; a >> 10; a >> 10; a >> 10;
    a >> 10; a >> 10; a >> 10; a >> 10; a >> 10; a >> 10; a >> 10; a >> 10; a >> 10; a >> 10;
    a >> 10; a >> 10; a >> 10; a >> 10; a >> 10; a >> 10; a >> 10; a >> 10; a >> 10; a >> 10;
    a >> 10; a >> 10; a >> 10; a >> 10; a >> 10; a >> 10; a >> 10; a >> 10; a >> 10; a >> 10;
    a >> 10; a >> 10; a >> 10; a >> 10; a >> 10; a >> 10; a >> 10; a >> 10; a >> 10; a >> 10;
    a >> 10; a >> 10; a >> 10; a >> 10; a >> 10; a >> 10; a >> 10; a >> 10; a >> 10; a >> 10;
    a >> 10; a >> 10; a >> 10; a >> 10; a >> 10; a >> 10; a >> 10; a >> 10; a >> 10; a >> 10;
    a >> 10; a >> 10; a >> 10; a >> 10; a >> 10; a >> 10; a >> 10; a >> 10; a >> 10; a >> 10;
    a >> 10; a >> 10; a >> 10; a >> 10; a >> 10; a >> 10; a >> 10; a >> 10; a >> 10; a >> 10;
  }
}

function shiftRightLogicalSmiConstant() {
  var a = 20;
  for (var i = 0; i < 1000; ++i) {
    a >>> 10; a >>> 10; a >>> 10; a >>> 10; a >>> 10; a >>> 10; a >>> 10; a >>> 10; a >>> 10; a >>> 10;
    a >>> 10; a >>> 10; a >>> 10; a >>> 10; a >>> 10; a >>> 10; a >>> 10; a >>> 10; a >>> 10; a >>> 10;
    a >>> 10; a >>> 10; a >>> 10; a >>> 10; a >>> 10; a >>> 10; a >>> 10; a >>> 10; a >>> 10; a >>> 10;
    a >>> 10; a >>> 10; a >>> 10; a >>> 10; a >>> 10; a >>> 10; a >>> 10; a >>> 10; a >>> 10; a >>> 10;
    a >>> 10; a >>> 10; a >>> 10; a >>> 10; a >>> 10; a >>> 10; a >>> 10; a >>> 10; a >>> 10; a >>> 10;
    a >>> 10; a >>> 10; a >>> 10; a >>> 10; a >>> 10; a >>> 10; a >>> 10; a >>> 10; a >>> 10; a >>> 10;
    a >>> 10; a >>> 10; a >>> 10; a >>> 10; a >>> 10; a >>> 10; a >>> 10; a >>> 10; a >>> 10; a >>> 10;
    a >>> 10; a >>> 10; a >>> 10; a >>> 10; a >>> 10; a >>> 10; a >>> 10; a >>> 10; a >>> 10; a >>> 10;
    a >>> 10; a >>> 10; a >>> 10; a >>> 10; a >>> 10; a >>> 10; a >>> 10; a >>> 10; a >>> 10; a >>> 10;
    a >>> 10; a >>> 10; a >>> 10; a >>> 10; a >>> 10; a >>> 10; a >>> 10; a >>> 10; a >>> 10; a >>> 10;
  }
}

function orSmi() {
  bitwiseOr(10, 20);
}

function orNumber() {
  bitwiseOr(0.33, 0.5);
}

function xorSmi() {
  bitwiseXor(10, 20);
}

function xorNumber() {
  bitwiseXor(0.33, 0.5);
}

function andSmi() {
  bitwiseAnd(10, 20);
}

function andNumber() {
  bitwiseAnd(0.33, 0.5);
}

function shiftLeftSmi() {
  shiftLeft(10, 20);
}

function shiftLeftNumber() {
  shiftLeft(0.333, 0.5);
}

function shiftRightSmi() {
  shiftRight(10, 20);
}

function shiftRightNumber() {
  shiftRight(0.333, 0.5);
}

function shiftRightLogicalSmi() {
  shiftRightLogical(10, 20);
}

function shiftRightLogicalNumber() {
  shiftRightLogical(0.333, 0.5);
}
