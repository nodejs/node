// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function addBenchmark(name, test) {
  new BenchmarkSuite(name, [1000],
      [
        new Benchmark(name, false, false, 0, test)
      ]);
}

addBenchmark('Smi-Add', addSmi);
addBenchmark('Number-Add', addNumber);
addBenchmark('Number-Oddball-Add', addOddball);
addBenchmark('String-Add', addString);
addBenchmark('Number-String-Add', addNumberString);
addBenchmark('Object-Add', addObject);
addBenchmark('Smi-Sub', subSmi);
addBenchmark('Number-Sub', subNumber);
addBenchmark('Number-Oddball-Sub', subOddball);
addBenchmark('Object-Sub', subObject);
addBenchmark('Smi-Mul', mulSmi);
addBenchmark('Number-Mul', mulNumber);
addBenchmark('Number-Oddball-Mul', mulOddball);
addBenchmark('Object-Mul', mulObject);
addBenchmark('Smi-Div', divSmi);
addBenchmark('Number-Div', divNumber);
addBenchmark('Number-Oddball-Div', divOddball);
addBenchmark('Object-Div', divObject);
addBenchmark('Smi-Mod', modSmi);
addBenchmark('Number-Mod', modNumber);
addBenchmark('Number-Oddball-Mod', modOddball);
addBenchmark('Object-Mod', modObject);
addBenchmark('Smi-Constant-Add', addSmiConstant);
addBenchmark('Smi-Constant-Sub', subSmiConstant);
addBenchmark('Smi-Constant-Mul', mulSmiConstant);
addBenchmark('Smi-Constant-Div', divSmiConstant);
addBenchmark('Smi-Constant-Mod', modSmiConstant);
addBenchmark('Smi-Increment', SmiIncrement);
addBenchmark('Number-Increment', NumberIncrement);
addBenchmark('Smi-Decrement', SmiDecrement);
addBenchmark('Number-Decrement', NumberDecrement);


function add(a, b) {
  for (var i = 0; i < 1000; ++i) {
    a + b; a + b; a + b; a + b; a + b; a + b; a + b; a + b; a + b; a + b;
    a + b; a + b; a + b; a + b; a + b; a + b; a + b; a + b; a + b; a + b;
    a + b; a + b; a + b; a + b; a + b; a + b; a + b; a + b; a + b; a + b;
    a + b; a + b; a + b; a + b; a + b; a + b; a + b; a + b; a + b; a + b;
    a + b; a + b; a + b; a + b; a + b; a + b; a + b; a + b; a + b; a + b;
    a + b; a + b; a + b; a + b; a + b; a + b; a + b; a + b; a + b; a + b;
    a + b; a + b; a + b; a + b; a + b; a + b; a + b; a + b; a + b; a + b;
    a + b; a + b; a + b; a + b; a + b; a + b; a + b; a + b; a + b; a + b;
    a + b; a + b; a + b; a + b; a + b; a + b; a + b; a + b; a + b; a + b;
    a + b; a + b; a + b; a + b; a + b; a + b; a + b; a + b; a + b; a + b;
  }
}

function sub(a, b) {
  for (var i = 0; i < 1000; ++i) {
    a - b; a - b; a - b; a - b; a - b; a - b; a - b; a - b; a - b; a - b;
    a - b; a - b; a - b; a - b; a - b; a - b; a - b; a - b; a - b; a - b;
    a - b; a - b; a - b; a - b; a - b; a - b; a - b; a - b; a - b; a - b;
    a - b; a - b; a - b; a - b; a - b; a - b; a - b; a - b; a - b; a - b;
    a - b; a - b; a - b; a - b; a - b; a - b; a - b; a - b; a - b; a - b;
    a - b; a - b; a - b; a - b; a - b; a - b; a - b; a - b; a - b; a - b;
    a - b; a - b; a - b; a - b; a - b; a - b; a - b; a - b; a - b; a - b;
    a - b; a - b; a - b; a - b; a - b; a - b; a - b; a - b; a - b; a - b;
    a - b; a - b; a - b; a - b; a - b; a - b; a - b; a - b; a - b; a - b;
    a - b; a - b; a - b; a - b; a - b; a - b; a - b; a - b; a - b; a - b;
  }
}

function mul(a, b) {
  for (var i = 0; i < 1000; ++i) {
    a * b; a * b; a * b; a * b; a * b; a * b; a * b; a * b; a * b; a * b;
    a * b; a * b; a * b; a * b; a * b; a * b; a * b; a * b; a * b; a * b;
    a * b; a * b; a * b; a * b; a * b; a * b; a * b; a * b; a * b; a * b;
    a * b; a * b; a * b; a * b; a * b; a * b; a * b; a * b; a * b; a * b;
    a * b; a * b; a * b; a * b; a * b; a * b; a * b; a * b; a * b; a * b;
    a * b; a * b; a * b; a * b; a * b; a * b; a * b; a * b; a * b; a * b;
    a * b; a * b; a * b; a * b; a * b; a * b; a * b; a * b; a * b; a * b;
    a * b; a * b; a * b; a * b; a * b; a * b; a * b; a * b; a * b; a * b;
    a * b; a * b; a * b; a * b; a * b; a * b; a * b; a * b; a * b; a * b;
    a * b; a * b; a * b; a * b; a * b; a * b; a * b; a * b; a * b; a * b;
  }
}

function div(a, b) {
  for (var i = 0; i < 1000; ++i) {
    a / b; a / b; a / b; a / b; a / b; a / b; a / b; a / b; a / b; a / b;
    a / b; a / b; a / b; a / b; a / b; a / b; a / b; a / b; a / b; a / b;
    a / b; a / b; a / b; a / b; a / b; a / b; a / b; a / b; a / b; a / b;
    a / b; a / b; a / b; a / b; a / b; a / b; a / b; a / b; a / b; a / b;
    a / b; a / b; a / b; a / b; a / b; a / b; a / b; a / b; a / b; a / b;
    a / b; a / b; a / b; a / b; a / b; a / b; a / b; a / b; a / b; a / b;
    a / b; a / b; a / b; a / b; a / b; a / b; a / b; a / b; a / b; a / b;
    a / b; a / b; a / b; a / b; a / b; a / b; a / b; a / b; a / b; a / b;
    a / b; a / b; a / b; a / b; a / b; a / b; a / b; a / b; a / b; a / b;
    a / b; a / b; a / b; a / b; a / b; a / b; a / b; a / b; a / b; a / b;
  }
}

function mod(a, b) {
  for (var i = 0; i < 1000; ++i) {
    a % b; a % b; a % b; a % b; a % b; a % b; a % b; a % b; a % b; a % b;
    a % b; a % b; a % b; a % b; a % b; a % b; a % b; a % b; a % b; a % b;
    a % b; a % b; a % b; a % b; a % b; a % b; a % b; a % b; a % b; a % b;
    a % b; a % b; a % b; a % b; a % b; a % b; a % b; a % b; a % b; a % b;
    a % b; a % b; a % b; a % b; a % b; a % b; a % b; a % b; a % b; a % b;
    a % b; a % b; a % b; a % b; a % b; a % b; a % b; a % b; a % b; a % b;
    a % b; a % b; a % b; a % b; a % b; a % b; a % b; a % b; a % b; a % b;
    a % b; a % b; a % b; a % b; a % b; a % b; a % b; a % b; a % b; a % b;
    a % b; a % b; a % b; a % b; a % b; a % b; a % b; a % b; a % b; a % b;
    a % b; a % b; a % b; a % b; a % b; a % b; a % b; a % b; a % b; a % b;
  }
}

function addSmiConstant(a) {
  var a = 20;
  for (var i = 0; i < 1000; ++i) {
    a + 10; a + 10; a + 10; a + 10; a + 10; a + 10; a + 10; a + 10; a + 10; a + 10;
    a + 10; a + 10; a + 10; a + 10; a + 10; a + 10; a + 10; a + 10; a + 10; a + 10;
    a + 10; a + 10; a + 10; a + 10; a + 10; a + 10; a + 10; a + 10; a + 10; a + 10;
    a + 10; a + 10; a + 10; a + 10; a + 10; a + 10; a + 10; a + 10; a + 10; a + 10;
    a + 10; a + 10; a + 10; a + 10; a + 10; a + 10; a + 10; a + 10; a + 10; a + 10;
    a + 10; a + 10; a + 10; a + 10; a + 10; a + 10; a + 10; a + 10; a + 10; a + 10;
    a + 10; a + 10; a + 10; a + 10; a + 10; a + 10; a + 10; a + 10; a + 10; a + 10;
    a + 10; a + 10; a + 10; a + 10; a + 10; a + 10; a + 10; a + 10; a + 10; a + 10;
    a + 10; a + 10; a + 10; a + 10; a + 10; a + 10; a + 10; a + 10; a + 10; a + 10;
    a + 10; a + 10; a + 10; a + 10; a + 10; a + 10; a + 10; a + 10; a + 10; a + 10;
  }
}

function subSmiConstant() {
  var a = 20;
  for (var i = 0; i < 1000; ++i) {
    a - 10; a - 10; a - 10; a - 10; a - 10; a - 10; a - 10; a - 10; a - 10; a - 10;
    a - 10; a - 10; a - 10; a - 10; a - 10; a - 10; a - 10; a - 10; a - 10; a - 10;
    a - 10; a - 10; a - 10; a - 10; a - 10; a - 10; a - 10; a - 10; a - 10; a - 10;
    a - 10; a - 10; a - 10; a - 10; a - 10; a - 10; a - 10; a - 10; a - 10; a - 10;
    a - 10; a - 10; a - 10; a - 10; a - 10; a - 10; a - 10; a - 10; a - 10; a - 10;
    a - 10; a - 10; a - 10; a - 10; a - 10; a - 10; a - 10; a - 10; a - 10; a - 10;
    a - 10; a - 10; a - 10; a - 10; a - 10; a - 10; a - 10; a - 10; a - 10; a - 10;
    a - 10; a - 10; a - 10; a - 10; a - 10; a - 10; a - 10; a - 10; a - 10; a - 10;
    a - 10; a - 10; a - 10; a - 10; a - 10; a - 10; a - 10; a - 10; a - 10; a - 10;
    a - 10; a - 10; a - 10; a - 10; a - 10; a - 10; a - 10; a - 10; a - 10; a - 10;
  }
}

function mulSmiConstant() {
  var a = 20;
  for (var i = 0; i < 1000; ++i) {
    a * 10; a * 10; a * 10; a * 10; a * 10; a * 10; a * 10; a * 10; a * 10; a * 10;
    a * 10; a * 10; a * 10; a * 10; a * 10; a * 10; a * 10; a * 10; a * 10; a * 10;
    a * 10; a * 10; a * 10; a * 10; a * 10; a * 10; a * 10; a * 10; a * 10; a * 10;
    a * 10; a * 10; a * 10; a * 10; a * 10; a * 10; a * 10; a * 10; a * 10; a * 10;
    a * 10; a * 10; a * 10; a * 10; a * 10; a * 10; a * 10; a * 10; a * 10; a * 10;
    a * 10; a * 10; a * 10; a * 10; a * 10; a * 10; a * 10; a * 10; a * 10; a * 10;
    a * 10; a * 10; a * 10; a * 10; a * 10; a * 10; a * 10; a * 10; a * 10; a * 10;
    a * 10; a * 10; a * 10; a * 10; a * 10; a * 10; a * 10; a * 10; a * 10; a * 10;
    a * 10; a * 10; a * 10; a * 10; a * 10; a * 10; a * 10; a * 10; a * 10; a * 10;
    a * 10; a * 10; a * 10; a * 10; a * 10; a * 10; a * 10; a * 10; a * 10; a * 10;
  }
}

function divSmiConstant() {
  var a = 20;
  for (var i = 0; i < 1000; ++i) {
    a / 10; a / 10; a / 10; a / 10; a / 10; a / 10; a / 10; a / 10; a / 10; a / 10;
    a / 10; a / 10; a / 10; a / 10; a / 10; a / 10; a / 10; a / 10; a / 10; a / 10;
    a / 10; a / 10; a / 10; a / 10; a / 10; a / 10; a / 10; a / 10; a / 10; a / 10;
    a / 10; a / 10; a / 10; a / 10; a / 10; a / 10; a / 10; a / 10; a / 10; a / 10;
    a / 10; a / 10; a / 10; a / 10; a / 10; a / 10; a / 10; a / 10; a / 10; a / 10;
    a / 10; a / 10; a / 10; a / 10; a / 10; a / 10; a / 10; a / 10; a / 10; a / 10;
    a / 10; a / 10; a / 10; a / 10; a / 10; a / 10; a / 10; a / 10; a / 10; a / 10;
    a / 10; a / 10; a / 10; a / 10; a / 10; a / 10; a / 10; a / 10; a / 10; a / 10;
    a / 10; a / 10; a / 10; a / 10; a / 10; a / 10; a / 10; a / 10; a / 10; a / 10;
    a / 10; a / 10; a / 10; a / 10; a / 10; a / 10; a / 10; a / 10; a / 10; a / 10;
  }
}

function modSmiConstant() {
  var a = 20;
  for (var i = 0; i < 1000; ++i) {
    a % 10; a % 10; a % 10; a % 10; a % 10; a % 10; a % 10; a % 10; a % 10; a % 10;
    a % 10; a % 10; a % 10; a % 10; a % 10; a % 10; a % 10; a % 10; a % 10; a % 10;
    a % 10; a % 10; a % 10; a % 10; a % 10; a % 10; a % 10; a % 10; a % 10; a % 10;
    a % 10; a % 10; a % 10; a % 10; a % 10; a % 10; a % 10; a % 10; a % 10; a % 10;
    a % 10; a % 10; a % 10; a % 10; a % 10; a % 10; a % 10; a % 10; a % 10; a % 10;
    a % 10; a % 10; a % 10; a % 10; a % 10; a % 10; a % 10; a % 10; a % 10; a % 10;
    a % 10; a % 10; a % 10; a % 10; a % 10; a % 10; a % 10; a % 10; a % 10; a % 10;
    a % 10; a % 10; a % 10; a % 10; a % 10; a % 10; a % 10; a % 10; a % 10; a % 10;
    a % 10; a % 10; a % 10; a % 10; a % 10; a % 10; a % 10; a % 10; a % 10; a % 10;
    a % 10; a % 10; a % 10; a % 10; a % 10; a % 10; a % 10; a % 10; a % 10; a % 10;
  }
}

function inc(a) {
  for (var i = 0; i < 1000; ++i) {
    ++a; ++a; ++a; ++a; ++a; ++a; ++a; ++a; ++a; ++a;
    ++a; ++a; ++a; ++a; ++a; ++a; ++a; ++a; ++a; ++a;
    ++a; ++a; ++a; ++a; ++a; ++a; ++a; ++a; ++a; ++a;
    ++a; ++a; ++a; ++a; ++a; ++a; ++a; ++a; ++a; ++a;
    ++a; ++a; ++a; ++a; ++a; ++a; ++a; ++a; ++a; ++a;
    ++a; ++a; ++a; ++a; ++a; ++a; ++a; ++a; ++a; ++a;
    ++a; ++a; ++a; ++a; ++a; ++a; ++a; ++a; ++a; ++a;
    ++a; ++a; ++a; ++a; ++a; ++a; ++a; ++a; ++a; ++a;
    ++a; ++a; ++a; ++a; ++a; ++a; ++a; ++a; ++a; ++a;
    ++a; ++a; ++a; ++a; ++a; ++a; ++a; ++a; ++a; ++a;
    // To ensure it is always in SmiRange for Smi operation.
    a -= 1000;
  }
}

function dec(a) {
  for (var i = 0; i < 1000; ++i) {
    --a; --a; --a; --a; --a; --a; --a; --a; --a; --a;
    --a; --a; --a; --a; --a; --a; --a; --a; --a; --a;
    --a; --a; --a; --a; --a; --a; --a; --a; --a; --a;
    --a; --a; --a; --a; --a; --a; --a; --a; --a; --a;
    --a; --a; --a; --a; --a; --a; --a; --a; --a; --a;
    --a; --a; --a; --a; --a; --a; --a; --a; --a; --a;
    --a; --a; --a; --a; --a; --a; --a; --a; --a; --a;
    --a; --a; --a; --a; --a; --a; --a; --a; --a; --a;
    --a; --a; --a; --a; --a; --a; --a; --a; --a; --a;
    --a; --a; --a; --a; --a; --a; --a; --a; --a; --a;
    // To ensure it is always in SmiRange for Smi operation.
    a += 1000;
  }
}

function addSmi() {
 add(10, 20);
}

function addNumber() {
 add(0.333, 0.5);
}

function addOddball() {
 add(0.333, true);
}

function addString() {
 add("abc", "def");
}

function addNumberString() {
 add("abc", 1.23);
}

function addObject() {
 add({x: 1, y:2}, {x:3, y:4});
}

function subSmi() {
 sub(10, 20);
}

function subNumber() {
 sub(0.333, 0.5);
}

function subOddball() {
 sub(0.333, true);
}

function subObject() {
 sub({x: 1, y:2}, {x:3, y:4});
}

function mulSmi() {
 mul(10, 20);
}

function mulNumber() {
 mul(0.333, 0.5);
}

function mulOddball() {
 mul(0.333, true);
}

function mulObject() {
 mul({x: 1, y:2}, {x:3, y:4});
}

function divSmi() {
 div(10, 20);
}

function divNumber() {
 div(0.333, 0.5);
}

function divOddball() {
 div(0.333, true);
}

function divObject() {
 div({x: 1, y:2}, {x:3, y:4});
}

function modSmi() {
 mod(10, 20);
}

function modNumber() {
 mod(0.333, 0.5);
}

function modOddball() {
 mod(0.333, true);
}

function modObject() {
 mod({x: 1, y:2}, {x:3, y:4});
}

function SmiIncrement() {
 inc(3);
}

function NumberIncrement() {
 inc(0.33);
}

function SmiDecrement() {
 dec(3);
}

function NumberDecrement() {
 dec(0.33);
}
