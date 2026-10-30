// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --js-decorators

// Top-level (eager) class with instance, static, and computed auto-accessors.
class TopLevelClass {
  accessor x = 1;
  static accessor y = 2;
  accessor ['z'] = 3;
}

const topX = Object.getOwnPropertyDescriptor(TopLevelClass.prototype, 'x');
assertEquals('function get x() { [native code] }', topX.get.toString());
assertEquals('function set x() { [native code] }', topX.set.toString());

const topY = Object.getOwnPropertyDescriptor(TopLevelClass, 'y');
assertEquals('function get y() { [native code] }', topY.get.toString());
assertEquals('function set y() { [native code] }', topY.set.toString());

const topZ = Object.getOwnPropertyDescriptor(TopLevelClass.prototype, 'z');
assertEquals('function () { [native code] }', topZ.get.toString());
assertEquals('function () { [native code] }', topZ.set.toString());

// Lazy function containing classes (base and derived) with auto-accessors.
function testLazy() {
  class Base {
    accessor a = 10;
    static accessor b = 20;
  }

  class Derived extends Base {
    accessor c = 30;
    static accessor d = 40;
  }

  const descA = Object.getOwnPropertyDescriptor(Base.prototype, 'a');
  assertEquals('function get a() { [native code] }', descA.get.toString());
  assertEquals('function set a() { [native code] }', descA.set.toString());

  const descB = Object.getOwnPropertyDescriptor(Base, 'b');
  assertEquals('function get b() { [native code] }', descB.get.toString());
  assertEquals('function set b() { [native code] }', descB.set.toString());

  const descC = Object.getOwnPropertyDescriptor(Derived.prototype, 'c');
  assertEquals('function get c() { [native code] }', descC.get.toString());
  assertEquals('function set c() { [native code] }', descC.set.toString());

  const descD = Object.getOwnPropertyDescriptor(Derived, 'd');
  assertEquals('function get d() { [native code] }', descD.get.toString());
  assertEquals('function set d() { [native code] }', descD.set.toString());

  const inst = new Derived();
  assertEquals(10, inst.a);
  assertEquals(30, inst.c);
  inst.a = 11;
  inst.c = 31;
  assertEquals(11, inst.a);
  assertEquals(31, inst.c);
  assertEquals(20, Base.b);
  assertEquals(40, Derived.d);
  Base.b = 21;
  Derived.d = 41;
  assertEquals(21, Base.b);
  assertEquals(41, Derived.d);
}

testLazy();
