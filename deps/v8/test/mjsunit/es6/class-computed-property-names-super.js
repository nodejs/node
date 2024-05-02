// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax


function ID(x) {
  return x;
}

function assertMethodDescriptor(object, name) {
  var descr = Object.getOwnPropertyDescriptor(object, name);
  assertTrue(descr.configurable);
  assertFalse(descr.enumerable);
  assertTrue(descr.writable);
  assertEquals('function', typeof descr.value);
  assertFalse('prototype' in descr.value);
  assertEquals("" + name, descr.value.name);
}


function assertGetterDescriptor(object, name) {
  var descr = Object.getOwnPropertyDescriptor(object, name);
  assertTrue(descr.configurable);
  assertFalse(descr.enumerable);
  assertEquals('function', typeof descr.get);
  assertFalse('prototype' in descr.get);
  assertEquals(undefined, descr.set);
  assertEquals("get " + name, descr.get.name);
}


function assertSetterDescriptor(object, name) {
  var descr = Object.getOwnPropertyDescriptor(object, name);
  assertTrue(descr.configurable);
  assertFalse(descr.enumerable);
  assertEquals(undefined, descr.get);
  assertEquals('function', typeof descr.set);
  assertFalse('prototype' in descr.set);
  assertEquals("set " + name, descr.set.name);
}


function assertAccessorDescriptor(object, name) {
  var descr = Object.getOwnPropertyDescriptor(object, name);
  assertTrue(descr.configurable);
  assertFalse(descr.enumerable);
  assertEquals('function', typeof descr.get);
  assertEquals('function', typeof descr.set);
  assertFalse('prototype' in descr.get);
  assertFalse('prototype' in descr.set);
  assertEquals("get " + name, descr.get.name);
  assertEquals("set " + name, descr.set.name);
}



(function TestComputedMethodSuper() {
  class Base {
    m() {
      return ' base m';
    }
  }
  class Derived extends Base {
    ['a']() { return 'a' + super.m(); }
    [ID('b')]() { return 'b' + super.m(); }
    [0]() { return '0' + super.m(); }
    [ID(1)]() { return '1' + super.m(); }
    [ID(2147483649)]() { return '2147483649' + super.m(); }
    [ID(4294967294)]() { return '4294967294' + super.m(); }
    [ID(4294967295)]() { return '4294967295' + super.m(); }
  }

  assertMethodDescriptor(Derived.prototype, "a");
  assertMethodDescriptor(Derived.prototype, "b");
  assertMethodDescriptor(Derived.prototype, 0);
  assertMethodDescriptor(Derived.prototype, 1);
  assertMethodDescriptor(Derived.prototype, 2147483649);
  assertMethodDescriptor(Derived.prototype, 4294967294);
  assertMethodDescriptor(Derived.prototype, 4294967295);

  assertEquals('a base m', new Derived().a());
  assertEquals('b base m', new Derived().b());
  assertEquals('0 base m', new Derived()[0]());
  assertEquals('1 base m', new Derived()[1]());
  assertEquals('2147483649 base m', new Derived()[2147483649]());
  assertEquals('4294967294 base m', new Derived()[4294967294]());
  assertEquals('4294967295 base m', new Derived()[4294967295]());
})();


(function TestComputedGetterSuper() {
  class Base {
    m() {
      return ' base m';
    }
  }
  class Derived extends Base {
    get ['a']() { return 'a' + super.m(); }
    get [ID('b')]() { return 'b' + super.m(); }
    get [0]() { return '0' + super.m(); }
    get [ID(1)]() { return '1' + super.m(); }
    get [ID(2147483649)]() { return '2147483649' + super.m(); }
    get [ID(4294967294)]() { return '4294967294' + super.m(); }
    get [ID(4294967295)]() { return '4294967295' + super.m(); }
  }

  assertGetterDescriptor(Derived.prototype, "a");
  assertGetterDescriptor(Derived.prototype, "b");
  assertGetterDescriptor(Derived.prototype, 0);
  assertGetterDescriptor(Derived.prototype, 1);
  assertGetterDescriptor(Derived.prototype, 2147483649);
  assertGetterDescriptor(Derived.prototype, 4294967294);
  assertGetterDescriptor(Derived.prototype, 4294967295);

  assertEquals('a base m', new Derived().a);
  assertEquals('b base m', new Derived().b);
  assertEquals('0 base m', new Derived()[0]);
  assertEquals('1 base m', new Derived()[1]);
  assertEquals('2147483649 base m', new Derived()[2147483649]);
  assertEquals('4294967294 base m', new Derived()[4294967294]);
  assertEquals('4294967295 base m', new Derived()[4294967295]);
})();


(function TestComputedSetterSuper() {
  var value;
  class Base {
    m(name, v) {
      value = name + ' ' + v;
    }
  }
  class Derived extends Base {
    set ['a'](v) { super.m('a', v); }
    set [ID('b')](v) { super.m('b', v); }
    set [0](v) { super.m('0', v); }
    set [ID(1)](v) { super.m('1', v); }
    set [ID(2147483649)](v) { super.m('2147483649', v); }
    set [ID(4294967294)](v) { super.m('4294967294', v); }
    set [ID(4294967295)](v) { super.m('4294967295', v); }
  }
  assertSetterDescriptor(Derived.prototype, "a");
  assertSetterDescriptor(Derived.prototype, "b");
  assertSetterDescriptor(Derived.prototype, 0);
  assertSetterDescriptor(Derived.prototype, 1);
  assertSetterDescriptor(Derived.prototype, 2147483649);
  assertSetterDescriptor(Derived.prototype, 4294967294);
  assertSetterDescriptor(Derived.prototype, 4294967295);

  new Derived().a = 2;
  assertEquals('a 2', value);
  new Derived().b = 3;
  assertEquals('b 3', value);
  new Derived()[0] = 4;
  assertEquals('0 4', value);
  new Derived()[1] = 5;
  assertEquals('1 5', value);
  new Derived()[2147483649] = 6;
  assertEquals('2147483649 6', value);
  new Derived()[4294967294] = 7;
  assertEquals('4294967294 7', value);
  new Derived()[4294967295] = 8;
  assertEquals('4294967295 8', value);
})();
