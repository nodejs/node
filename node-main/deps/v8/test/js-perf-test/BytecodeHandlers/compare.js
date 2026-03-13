// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function addBenchmark(name, test) {
  new BenchmarkSuite(name, [1000],
      [
        new Benchmark(name, false, false, 0, test)
      ]);
}

addBenchmark('Smi-StrictEquals-True', SmiStrictEqualsTrue);
addBenchmark('Smi-StrictEquals-False', SmiStrictEqualsFalse);
addBenchmark('Number-StrictEquals-True', NumberStrictEqualsTrue);
addBenchmark('Number-StrictEquals-False', NumberStrictEqualsFalse);
addBenchmark('String-StrictEquals-True', StringStrictEqualsTrue);
addBenchmark('String-StrictEquals-False', StringStrictEqualsFalse);
addBenchmark('SmiString-StrictEquals', MixedStrictEquals);
addBenchmark('Boolean-StrictEquals', BooleanStrictEquals);
addBenchmark('Smi-Equals-True', SmiEqualsTrue);
addBenchmark('Smi-Equals-False', SmiEqualsFalse);
addBenchmark('Number-Equals-True', NumberEqualsTrue);
addBenchmark('Number-Equals-False', NumberEqualsFalse);
addBenchmark('String-Equals-True', StringEqualsTrue);
addBenchmark('String-Equals-False', StringEqualsFalse);
addBenchmark('SmiString-Equals', MixedEquals);
addBenchmark('ObjectNull-Equals', ObjectEqualsNull);
addBenchmark('Smi-RelationalCompare', SmiRelationalCompare);
addBenchmark('Number-RelationalCompare', NumberRelationalCompare);
addBenchmark('String-RelationalCompare', StringRelationalCompare);
addBenchmark('SmiString-RelationalCompare', MixedRelationalCompare);

var null_object;

function strictEquals(a, b) {
  for (var i = 0; i < 1000; ++i) {
    a === b; a === b; a === b; a === b; a === b; a === b; a === b; a === b; a === b; a === b;
    a === b; a === b; a === b; a === b; a === b; a === b; a === b; a === b; a === b; a === b;
    a === b; a === b; a === b; a === b; a === b; a === b; a === b; a === b; a === b; a === b;
    a === b; a === b; a === b; a === b; a === b; a === b; a === b; a === b; a === b; a === b;
    a === b; a === b; a === b; a === b; a === b; a === b; a === b; a === b; a === b; a === b;
    a === b; a === b; a === b; a === b; a === b; a === b; a === b; a === b; a === b; a === b;
    a === b; a === b; a === b; a === b; a === b; a === b; a === b; a === b; a === b; a === b;
    a === b; a === b; a === b; a === b; a === b; a === b; a === b; a === b; a === b; a === b;
    a === b; a === b; a === b; a === b; a === b; a === b; a === b; a === b; a === b; a === b;
    a === b; a === b; a === b; a === b; a === b; a === b; a === b; a === b; a === b; a === b;
  }
}

function strictEqualsBoolean(a) {
  var ret;
  for (var i = 0; i < 1000; ++i) {
    if (a === true) ret = true;
    if (a === true) ret = true;
    if (a === true) ret = true;
    if (a === true) ret = true;
    if (a === true) ret = true;
    if (a === true) ret = true;
    if (a === true) ret = true;
    if (a === true) ret = true;
    if (a === true) ret = true;
    if (a === true) ret = true;
    if (a === true) ret = true;
    if (a === true) ret = true;
    if (a === true) ret = true;
    if (a === true) ret = true;
    if (a === true) ret = true;
    if (a === true) ret = true;
    if (a === true) ret = true;
    if (a === true) ret = true;
    if (a === true) ret = true;
    if (a === true) ret = true;
    if (a === true) ret = true;
    if (a === true) ret = true;
    if (a === true) ret = true;
    if (a === true) ret = true;
    if (a === true) ret = true;
    if (a === true) ret = true;
    if (a === true) ret = true;
    if (a === true) ret = true;
    if (a === true) ret = true;
    if (a === true) ret = true;
    if (a === true) ret = true;
    if (a === true) ret = true;
    if (a === true) ret = true;
    if (a === true) ret = true;
    if (a === true) ret = true;
    if (a === true) ret = true;
    if (a === true) ret = true;
    if (a === true) ret = true;
    if (a === true) ret = true;
    if (a === true) ret = true;
    if (a === true) ret = true;
    if (a === true) ret = true;
    if (a === true) ret = true;
    if (a === true) ret = true;
    if (a === true) ret = true;
    if (a === true) ret = true;
    if (a === true) ret = true;
    if (a === true) ret = true;
    if (a === true) ret = true;
    if (a === true) ret = true;
    if (a === false) ret = true;
    if (a === false) ret = true;
    if (a === false) ret = true;
    if (a === false) ret = true;
    if (a === false) ret = true;
    if (a === false) ret = true;
    if (a === false) ret = true;
    if (a === false) ret = true;
    if (a === false) ret = true;
    if (a === false) ret = true;
    if (a === false) ret = true;
    if (a === false) ret = true;
    if (a === false) ret = true;
    if (a === false) ret = true;
    if (a === false) ret = true;
    if (a === false) ret = true;
    if (a === false) ret = true;
    if (a === false) ret = true;
    if (a === false) ret = true;
    if (a === false) ret = true;
    if (a === false) ret = true;
    if (a === false) ret = true;
    if (a === false) ret = true;
    if (a === false) ret = true;
    if (a === false) ret = true;
    if (a === false) ret = true;
    if (a === false) ret = true;
    if (a === false) ret = true;
    if (a === false) ret = true;
    if (a === false) ret = true;
    if (a === false) ret = true;
    if (a === false) ret = true;
    if (a === false) ret = true;
    if (a === false) ret = true;
    if (a === false) ret = true;
    if (a === false) ret = true;
    if (a === false) ret = true;
    if (a === false) ret = true;
    if (a === false) ret = true;
    if (a === false) ret = true;
    if (a === false) ret = true;
    if (a === false) ret = true;
    if (a === false) ret = true;
    if (a === false) ret = true;
    if (a === false) ret = true;
    if (a === false) ret = true;
    if (a === false) ret = true;
    if (a === false) ret = true;
    if (a === false) ret = true;
    if (a === false) ret = true;
  }
  return ret;
}

function equals(a, b) {
  for (var i = 0; i < 1000; ++i) {
    a == b; a == b; a == b; a == b; a == b; a == b; a == b; a == b; a == b; a == b;
    a == b; a == b; a == b; a == b; a == b; a == b; a == b; a == b; a == b; a == b;
    a == b; a == b; a == b; a == b; a == b; a == b; a == b; a == b; a == b; a == b;
    a == b; a == b; a == b; a == b; a == b; a == b; a == b; a == b; a == b; a == b;
    a == b; a == b; a == b; a == b; a == b; a == b; a == b; a == b; a == b; a == b;
    a == b; a == b; a == b; a == b; a == b; a == b; a == b; a == b; a == b; a == b;
    a == b; a == b; a == b; a == b; a == b; a == b; a == b; a == b; a == b; a == b;
    a == b; a == b; a == b; a == b; a == b; a == b; a == b; a == b; a == b; a == b;
    a == b; a == b; a == b; a == b; a == b; a == b; a == b; a == b; a == b; a == b;
    a == b; a == b; a == b; a == b; a == b; a == b; a == b; a == b; a == b; a == b;
  }
}

// Relational comparison handlers are similar, so use one benchmark to measure
// all of them.
function relationalCompare(a, b) {
  for (var i = 0; i < 1000; ++i) {
    a > b; a > b; a > b; a > b; a > b; a > b; a > b; a > b; a > b; a > b;
    a > b; a > b; a > b; a > b; a > b; a > b; a > b; a > b; a > b; a > b;
    a > b; a > b; a > b; a > b; a > b; a > b; a > b; a > b; a > b; a > b;
    a >= b; a >= b; a >= b; a >= b; a >= b; a >= b; a >= b; a >= b; a >= b; a >= b;
    a >= b; a >= b; a >= b; a >= b; a >= b; a >= b; a >= b; a >= b; a >= b; a >= b;
    a >= b; a >= b; a >= b; a >= b; a >= b; a >= b; a >= b; a >= b; a >= b; a >= b;
    a < b; a < b; a < b; a < b; a < b; a < b; a < b; a < b; a < b; a < b;
    a < b; a < b; a < b; a < b; a < b; a < b; a < b; a < b; a < b; a < b;
    a < b; a < b; a < b; a < b; a < b; a < b; a < b; a < b; a < b; a < b;
    a <= b; a <= b; a <= b; a <= b; a <= b; a <= b; a <= b; a <= b; a <= b; a <= b;
    a <= b; a <= b; a <= b; a <= b; a <= b; a <= b; a <= b; a <= b; a <= b; a <= b;
    a <= b; a <= b; a <= b; a <= b; a <= b; a <= b; a <= b; a <= b; a <= b; a <= b;
  }
}

function SmiStrictEqualsFalse() {
 strictEquals(10, 20);
}

function SmiStrictEqualsTrue() {
 strictEquals(10, 10);
}

function NumberStrictEqualsFalse() {
 strictEquals(0.3333, 0.3334);
}

function NumberStrictEqualsTrue() {
 strictEquals(0.3333, 0.3333);
}

function StringStrictEqualsFalse() {
 strictEquals("abc", "def");
}

function StringStrictEqualsTrue() {
 strictEquals("abc", "abc");
}

function BooleanStrictEquals() {
  strictEqualsBoolean("a");
  strictEqualsBoolean(true);
  strictEqualsBoolean(false);
}

function MixedStrictEquals() {
 strictEquals(10, "10");
}

function SmiEqualsFalse() {
 equals(10, 20);
}

function SmiEqualsTrue() {
 equals(10, 10);
}

function NumberEqualsFalse() {
 equals(0.3333, 0.3334);
}

function NumberEqualsTrue() {
 equals(0.3333, 0.3333);
}

function StringEqualsFalse() {
 equals("abc", "def");
}

function StringEqualsTrue() {
 equals("abc", "abc");
}

function MixedEquals() {
 equals(10, "10");
}

function ObjectEqualsNull(null_object) {
 equals(null_object, null);
}

function SmiRelationalCompare() {
 relationalCompare(10, 20);
}

function NumberRelationalCompare() {
 relationalCompare(0.3333, 0.3334);
}

function StringRelationalCompare() {
 relationalCompare("abc", "def");
}

function MixedRelationalCompare() {
 relationalCompare(10, "10");
}
