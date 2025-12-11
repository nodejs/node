// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// For rest parameters.
function f1(...x) {
  { var x; }
  function x(){}
  assertEquals('function', typeof x);
  var x = 10;
  assertEquals(10, x);
};
f1(0);

function f2(...x) {
  var x;
  function x(){}
  assertEquals('function', typeof x);
  var x = 10;
  assertEquals(10, x);
};
f2(0);

function f3(...x) {
  var x;
  assertEquals('object', typeof x);
}
f3(1);

function f4(...x) {
  var x = 10;
  assertEquals(10, x);
}
f4(1);

function f5(...x) {
  function x(){}
  assertEquals('function', typeof x);
}
f5(1);

// For simple parameters.
function f6(x) {
  var x = 10;
  function x(){}
  assertEquals(10, x);
}
f6(1);

function f7(x) {
  var x;
  function x(){}
  assertEquals('function', typeof x);
}
f7(1);

function f8(x) {
  var x;
  assertEquals(1, x);
}
f8(1);

function f9(x) {
  var x = 10;
  assertEquals(10, x);
}
f9(1);

function f10(x) {
  function x(){}
  assertEquals('function', typeof x);
}
f10(1);

// For default parameters.
function f11(x = 2) {
  var x;
  function x(){}
  assertEquals('function', typeof x);
}
f11();
f11(1);

function f12(x = 2) {
  var x;
  function x(){}
  assertEquals('function', typeof x);
  var x = 10;
  assertEquals(10, x);
}
f12();
f12(1);

function f13(x = 2) {
  var x;
  assertEquals(2, x);
}
f13();

function f14(x = 2) {
  var x = 1;
  assertEquals(1, x);
}
f14();
f14(3);

function f15(x = 2) {
  function x(){}
  assertEquals('function', typeof x);
}
f15();
