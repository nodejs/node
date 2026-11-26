// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function f1(arguments = 7) {
  return arguments;
}
assertEquals(7, f1());
assertEquals(42, f1(42));

function f2(a = arguments, arguments) {}
assertThrows(() => f2(), ReferenceError);

function f3(a = () => arguments, arguments) {
  return a();
}
assertEquals(undefined, f3());

function f3b(a = (() => arguments)(), arguments) {}
assertThrows(() => f3b(), ReferenceError);

function f4([arguments]) {
  return arguments;
}
assertEquals(5, f4([5]));

function f5(arguments, b = 1) {
  return eval('arguments');
}
assertEquals(3, f5(3));

function f6(x = 1, arguments) {
  return (() => typeof arguments)();
}
assertEquals('undefined', f6());
assertEquals('number', f6(1, 2));

function f7(...arguments) {
  return Array.isArray(arguments);
}
assertTrue(f7(1, 2));

function f8(a = eval('arguments'), arguments) {
  return a;
}
assertThrows(() => f8(), ReferenceError);

function f9(arguments = 7) {
  arguments = 42;
  return arguments;
}
assertEquals(42, f9());

function f10(arguments = 7) {
  eval('arguments = 99');
  return arguments;
}
assertEquals(99, f10());

function f11(arguments = 7, b = eval('arguments')) {
  return b;
}
assertEquals(7, f11());

// Regression test for Parser/PreParser PreparseData parity with lazy inner
// functions and source position collection (`new Error().stack`).
function f12(arguments = 7) {
  function dummy() {}
  const g = () => arguments;
  return [arguments, arguments, g(), new Error().stack];
}
assertEquals([7, 7, 7], f12().slice(0, 3));
assertEquals([42, 42, 42], f12(42).slice(0, 3));

function f13([arguments]) {
  function dummy() {}
  const g = () => arguments;
  return [arguments, arguments, g(), new Error().stack];
}
assertEquals([9, 9, 9], f13([9]).slice(0, 3));

function f14(...arguments) {
  function dummy() {}
  const g = () => arguments;
  return [arguments, arguments, g(), new Error().stack];
}
assertEquals([[1, 2], [1, 2], [1, 2]], f14(1, 2).slice(0, 3));

function f15(arguments) {
  function dummy() {}
  const g = () => arguments;
  return [arguments, arguments, g(), new Error().stack];
}
assertEquals([11, 11, 11], f15(11).slice(0, 3));
