// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


var properties =
    ["bla", "0", 1, Symbol(), {[Symbol.toPrimitive]() {return "a"}}];


function TestForwarding(handler, myDelete, shouldThrow) {
  var target = {};
  var proxy = new Proxy(target, handler);

  assertFalse(target.hasOwnProperty("doesnotexist"));
  assertTrue(myDelete(proxy, "doesnotexist"));

  for (p of properties) {
    target[p] = 42;
    assertTrue(myDelete(proxy, p));
    assertFalse(target.hasOwnProperty(p));
  }

  for (p of properties) {
    Object.defineProperty(target, p, {value: 42, configurable: false});
    if (shouldThrow) {
      assertThrows(() => myDelete(proxy, p), TypeError);
    } else {
      assertFalse(myDelete(proxy, p));
    }
    assertTrue(target.hasOwnProperty(p));
  }
};


(function () {
  // No trap.

  var handler = {};

  TestForwarding(handler,
      (o, p) => delete o[p], false);
  TestForwarding(handler,
      (o, p) => Reflect.deleteProperty(o, p), false);
  TestForwarding(handler,
      (o, p) => {"use strict"; return delete o[p]}, true);
  TestForwarding(handler,
      (o, p) => {"use strict"; return Reflect.deleteProperty(o, p)}, false);
})();


(function () {
  // "Undefined" trap.

  var handler = { deleteProperty: null };

  TestForwarding(handler,
      (o, p) => delete o[p], false);
  TestForwarding(handler,
      (o, p) => Reflect.deleteProperty(o, p), false);
  TestForwarding(handler,
      (o, p) => {"use strict"; return delete o[p]}, true);
  TestForwarding(handler,
      (o, p) => {"use strict"; return Reflect.deleteProperty(o, p)}, false);
})();


(function () {
  // Invalid trap.

  var target = {};
  var handler = { deleteProperty: true };
  var proxy = new Proxy(target, handler);

  assertThrows(() => delete proxy[0], TypeError);
  assertThrows(() => Reflect.deleteProperty(proxy, 0), TypeError);
})();


function TestTrappingTrueish(myDelete) {
  var handler = { deleteProperty() {return 42} };
  var target = {};
  var proxy = new Proxy(target, handler);

  // Trap returns trueish and target doesn't own property.
  for (p of properties) {
    assertTrue(myDelete(proxy, p));
  }

  // Trap returns trueish and target property is configurable.
  for (p of properties) {
    target[p] = 42;
    assertTrue(myDelete(proxy, p));
  }

  // Trap returns trueish but target property is not configurable.
  for (p of properties) {
    Object.defineProperty(target, p, {value: 42, configurable: false});
    assertThrows(() => myDelete(proxy, p), TypeError);
  }
};


TestTrappingTrueish(
    (o, p) => delete o[p]);
TestTrappingTrueish(
    (o, p) => Reflect.deleteProperty(o, p));
TestTrappingTrueish(
    (o, p) => {"use strict"; return delete o[p]});
TestTrappingTrueish(
    (o, p) => {"use strict"; return Reflect.deleteProperty(o, p)});


function TestTrappingTrueish2(myDelete) {
  var handler = {
      deleteProperty(target, p) {
          Object.defineProperty(target, p, {configurable: false});
          return 42
      }
  };
  var target = {};
  var proxy = new Proxy(target, handler);

  // Trap returns trueish but target property is not configurable.  In contrast
  // to above, here the target property was configurable before the trap call.
  for (p of properties) {
    target[p] = 42;
    assertThrows(() => myDelete(proxy, p), TypeError);
  }
};


TestTrappingTrueish2(
    (o, p) => delete o[p]);
TestTrappingTrueish2(
    (o, p) => Reflect.deleteProperty(o, p));
TestTrappingTrueish2(
    (o, p) => {"use strict"; return delete o[p]});
TestTrappingTrueish2(
    (o, p) => {"use strict"; return Reflect.deleteProperty(o, p)});


function TestTrappingFalsish(myDelete, shouldThrow) {
  var handler = { deleteProperty() {return ""} };
  var target = {};
  var proxy = new Proxy(target, handler);

  var properties =
      ["bla", "0", 1, Symbol(), {[Symbol.toPrimitive]() {return "a"}}];

  // Trap returns falsish and target doesn't own property.
  for (p of properties) {
    if (shouldThrow) {
      assertThrows(() => myDelete(proxy, p), TypeError);
    } else {
      assertFalse(myDelete(proxy, p));
    }
  }

  // Trap returns falsish and target property is configurable.
  for (p of properties) {
    target[p] = 42;
    if (shouldThrow) {
      assertThrows(() => myDelete(proxy, p), TypeError);
    } else {
      assertFalse(myDelete(proxy, p));
    }
  }

  // Trap returns falsish and target property is not configurable.
  for (p of properties) {
    Object.defineProperty(target, p, {value: 42, configurable: false});
    if (shouldThrow) {
      assertThrows(() => myDelete(proxy, p), TypeError);
    } else {
      assertFalse(myDelete(proxy, p));
    }
  }
};


TestTrappingFalsish(
    (o, p) => delete o[p], false);
TestTrappingFalsish(
    (o, p) => Reflect.deleteProperty(o, p), false);
TestTrappingFalsish(
    (o, p) => {"use strict"; return delete o[p]}, true);
TestTrappingFalsish(
    (o, p) => {"use strict"; return Reflect.deleteProperty(o, p)}, false);
