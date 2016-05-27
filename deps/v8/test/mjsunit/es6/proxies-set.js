// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


function sloppyDefaultSet(o, p, v) { return o[p] = v }
function sloppyReflectSet(o, p, v) { return Reflect.set(o, p, v) }
function strictDefaultSet(o, p, v) { "use strict"; return o[p] = v }
function strictReflectSet(o, p, v) { "use strict"; return Reflect.set(o, p, v) }

sloppyDefaultSet.shouldThrow = false;
sloppyReflectSet.shouldThrow = false;
strictDefaultSet.shouldThrow = true;
strictReflectSet.shouldThrow = false;

sloppyDefaultSet.returnsBool = false;
sloppyReflectSet.returnsBool = true;
strictDefaultSet.returnsBool = false;
strictReflectSet.returnsBool = true;


function assertTrueIf(flag, x) { if (flag) assertTrue(x) }
function assertFalseIf(flag, x) { if (flag) assertFalse(x) }
function assertSetFails(mySet, o, p, v) {
  if (mySet.shouldThrow) {
    assertThrows(() => mySet(o, p, v), TypeError);
  } else {
    assertFalseIf(mySet.returnsBool, mySet(o, p, v));
  }
}


function dataDescriptor(x) {
  return {value: x, writable: true, enumerable: true, configurable: true};
}


function toKey(x) {
  if (typeof x === "symbol") return x;
  return String(x);
}


var properties =
    ["bla", "0", 1, Symbol(), {[Symbol.toPrimitive]() {return "a"}}];


function TestForwarding(handler, mySet) {
  assertTrue(undefined == handler.set);
  assertTrue(undefined == handler.getOwnPropertyDescriptor);
  assertTrue(undefined == handler.defineProperty);

  var target = {};
  var proxy = new Proxy(target, handler);

  // Property does not exist on target.
  for (var p of properties) {
    assertTrueIf(mySet.returnsBool, mySet(proxy, p, 42));
    assertSame(42, target[p]);
  }

  // Property exists as writable data on target.
  for (var p of properties) {
    assertTrueIf(mySet.returnsBool, mySet(proxy, p, 0));
    assertSame(0, target[p]);
  }

  // Property exists as non-writable data on target.
  for (var p of properties) {
    Object.defineProperty(target, p,
        {value: 42, configurable: true, writable: false});
    assertSetFails(mySet, proxy, p, 42);
    assertSetFails(mySet, proxy, p, 0);
    assertEquals(42, target[p]);
  }
};

(function () {
  // No trap.
  var handler = {};
  TestForwarding(handler, sloppyDefaultSet);
  TestForwarding(handler, sloppyReflectSet);
  TestForwarding(handler, strictDefaultSet);
  TestForwarding(handler, strictReflectSet);
})();

(function () {
  // "Undefined" trap.
  var handler = { set: null };
  TestForwarding(handler, sloppyDefaultSet);
  TestForwarding(handler, sloppyReflectSet);
  TestForwarding(handler, strictDefaultSet);
  TestForwarding(handler, strictReflectSet);
})();


function TestForwarding2(mySet) {
  // Check that setting on a proxy without "set" trap correctly triggers its
  // "getOwnProperty" trap and its "defineProperty" trap.

  var target = {};
  var handler = {};
  var observations = [];
  var proxy = new Proxy(target, handler);

  handler.getOwnPropertyDescriptor = function() {
      observations.push(arguments);
      return Reflect.getOwnPropertyDescriptor(...arguments);
  }

  handler.defineProperty = function() {
      observations.push(arguments);
      return Reflect.defineProperty(...arguments);
  }

  for (var p of properties) {
    mySet(proxy, p, 42);
    assertEquals(2, observations.length)
    assertArrayEquals([target, toKey(p)], observations[0]);
    assertSame(target, observations[0][0]);
    assertArrayEquals([target, toKey(p), dataDescriptor(42)], observations[1]);
    assertSame(target, observations[1][0]);
    observations = [];

    mySet(proxy, p, 42);
    assertEquals(2, observations.length)
    assertArrayEquals([target, toKey(p)], observations[0]);
    assertSame(target, observations[0][0]);
    assertArrayEquals([target, toKey(p), {value: 42}], observations[1]);
    assertSame(target, observations[1][0]);
    observations = [];
  }
}

TestForwarding2(sloppyDefaultSet);
TestForwarding2(sloppyReflectSet);
TestForwarding2(strictDefaultSet);
TestForwarding2(strictReflectSet);


function TestInvalidTrap(proxy, mySet) {
  for (var p of properties) {
    assertThrows(() => mySet(proxy, p, 42), TypeError);
  }
}

(function () {
  var target = {};
  var handler = { set: true };
  var proxy = new Proxy(target, handler);

  TestInvalidTrap(proxy, sloppyDefaultSet);
  TestInvalidTrap(proxy, sloppyReflectSet);
  TestInvalidTrap(proxy, strictDefaultSet);
  TestInvalidTrap(proxy, strictReflectSet);
})();


function TestTrappingFalsish(mySet) {
  var target = {};
  var handler = { set() {return ""} };
  var proxy = new Proxy(target, handler);

  for (var p of properties) {
    assertSetFails(mySet, proxy, p, 42);
  }
}

TestTrappingFalsish(sloppyDefaultSet);
TestTrappingFalsish(sloppyReflectSet);
TestTrappingFalsish(strictDefaultSet);
TestTrappingFalsish(strictReflectSet);


function TestTrappingTrueish(mySet) {
  var target = {};
  var handler = { set() {return 42} };
  var proxy = new Proxy(target, handler);

  // Trap returns trueish and property does not exist in target.
  for (var p of properties) {
    assertTrueIf(mySet.returnsBool, mySet(proxy, p, 0));
  }

  // Trap returns trueish and target property is configurable or writable data.
  for (var p of properties) {
    Object.defineProperty(target, p, {configurable: true, writable: true});
    assertTrueIf(mySet.returnsBool, mySet(proxy, p, 0));
    Object.defineProperty(target, p, {configurable: true, writable: false});
    assertTrueIf(mySet.returnsBool, mySet(proxy, p, 0));
    Object.defineProperty(target, p, {configurable: false, writable: true});
    assertTrueIf(mySet.returnsBool, mySet(proxy, p, 0));
  }
}

TestTrappingTrueish(sloppyDefaultSet);
TestTrappingTrueish(sloppyReflectSet);
TestTrappingTrueish(strictDefaultSet);
TestTrappingTrueish(strictReflectSet);


function TestTrappingTrueish2(mySet) {
  var target = {};
  var handler = { set() {return 42} };
  var proxy = new Proxy(target, handler);

  // Trap returns trueish but target property is frozen data.
  for (var p of properties) {
    Object.defineProperty(target, p, {
        configurable: false, writable: false, value: 0
    });
    assertThrows(() => mySet(proxy, p, 666), TypeError);  // New value.
    assertTrueIf(mySet.returnsBool, mySet(proxy, p, 0));  // Old value.
  }
};

TestTrappingTrueish2(sloppyDefaultSet);
TestTrappingTrueish2(sloppyReflectSet);
TestTrappingTrueish2(strictDefaultSet);
TestTrappingTrueish2(strictReflectSet);


function TestTrappingTrueish3(mySet) {
  var target = {};
  var handler = { set() {return 42} };
  var proxy = new Proxy(target, handler);

  // Trap returns trueish and target property is configurable accessor.
  for (var p of properties) {
    Object.defineProperty(target, p, { configurable: true, set: undefined });
    assertTrueIf(mySet.returnsBool, mySet(proxy, p, 0));
  }

  // Trap returns trueish and target property is non-configurable accessor.
  for (var p of properties) {
    Object.defineProperty(target, p, { configurable: false, set: undefined });
    assertThrows(() => mySet(proxy, p, 0));
  }
};

TestTrappingTrueish3(sloppyDefaultSet);
TestTrappingTrueish3(sloppyReflectSet);
TestTrappingTrueish3(strictDefaultSet);
TestTrappingTrueish3(strictReflectSet);


function TestTrapReceiverArgument(mySet) {
  var target = {};
  var handler = {};
  var observations = [];
  var proxy = new Proxy(target, handler);
  var object = Object.create(proxy);

  handler.set = function() {
      observations.push(arguments);
      return Reflect.set(...arguments);
  }

  for (var p of properties) {
    mySet(object, p, 42);
    assertEquals(1, observations.length)
    assertArrayEquals([target, toKey(p), 42, object], observations[0]);
    assertSame(target, observations[0][0]);
    assertSame(object, observations[0][3]);
    observations = [];
  }
};

TestTrapReceiverArgument(sloppyDefaultSet);
TestTrapReceiverArgument(sloppyReflectSet);
TestTrapReceiverArgument(strictDefaultSet);
TestTrapReceiverArgument(strictReflectSet);


(function TestTrapReceiverArgument2() {
  // Check that non-object receiver is passed through as well.

  var target = {};
  var handler = {};
  var observations = [];
  var proxy = new Proxy(target, handler);

  handler.set = function() {
      observations.push(arguments);
      return Reflect.set(...arguments);
  }

  for (var p of properties) {
    for (var receiver of [null, undefined, 1]) {
      Reflect.set(proxy, p, 42, receiver);
      assertEquals(1, observations.length)
      assertArrayEquals([target, toKey(p), 42, receiver], observations[0]);
      assertSame(target, observations[0][0]);
      assertSame(receiver, observations[0][3]);
      observations = [];
    }
  }

  var object = Object.create(proxy);
  for (var p of properties) {
    for (var receiver of [null, undefined, 1]) {
      Reflect.set(object, p, 42, receiver);
      assertEquals(1, observations.length);
      assertArrayEquals([target, toKey(p), 42, receiver], observations[0]);
      assertSame(target, observations[0][0]);
      assertSame(receiver, observations[0][3]);
      observations = [];
    }
  }
})();
