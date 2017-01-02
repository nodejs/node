// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// script-level tests
var ox, oy = {}, oz;
({
  x: ox,
  y: oy.value,
  y2: oy["value2"],
  z: ({ set v(val) { oz = val; } }).v
} = {
  x: "value of x",
  y: "value of y1",
  y2: "value of y2",
  z: "value of z"
});
assertEquals("value of x", ox);
assertEquals("value of y1", oy.value);
assertEquals("value of y2", oy.value2);
assertEquals("value of z", oz);

[ox, oy.value, oy["value2"], ...{ set v(val) { oz = val; } }.v] = [
  1007,
  798432,
  555,
  1, 2, 3, 4, 5
];
assertEquals(ox, 1007);
assertEquals(oy.value, 798432);
assertEquals(oy.value2, 555);
assertEquals(oz, [1, 2, 3, 4, 5]);


(function testInFunction() {
  var x, y = {}, z;
  ({
    x: x,
    y: y.value,
    y2: y["value2"],
    z: ({ set v(val) { z = val; } }).v
  } = {
    x: "value of x",
    y: "value of y1",
    y2: "value of y2",
    z: "value of z"
  });
  assertEquals("value of x", x);
  assertEquals("value of y1", y.value);
  assertEquals("value of y2", y.value2);
  assertEquals("value of z", z);

  [x, y.value, y["value2"], ...{ set v(val) { z = val; } }.v] = [
    1007,
    798432,
    555,
    1, 2, 3, 4, 5
  ];
  assertEquals(x, 1007);
  assertEquals(y.value, 798432);
  assertEquals(y.value2, 555);
  assertEquals(z, [1, 2, 3, 4, 5]);
})();


(function testArrowFunctionInitializers() {
  var fn = (config = {
    value: defaults.value,
    nada: { nada: defaults.nada } = { nada: "nothing" }
  } = { value: "BLAH" }) => config;
  var defaults = {};
  assertEquals({ value: "BLAH" }, fn());
  assertEquals("BLAH", defaults.value);
  assertEquals("nothing", defaults.nada);
})();


(function testArrowFunctionInitializers2() {
  var fn = (config = [
    defaults.value,
    { nada: defaults.nada } = { nada: "nothing" }
  ] = ["BLAH"]) => config;
  var defaults = {};
  assertEquals(["BLAH"], fn());
  assertEquals("BLAH", defaults.value);
  assertEquals("nothing", defaults.nada);
})();


(function testFunctionInitializers() {
  function fn(config = {
    value: defaults.value,
    nada: { nada: defaults.nada } = { nada: "nothing" }
  } = { value: "BLAH" }) {
    return config;
  }
  var defaults = {};
  assertEquals({ value: "BLAH" }, fn());
  assertEquals("BLAH", defaults.value);
  assertEquals("nothing", defaults.nada);
})();


(function testFunctionInitializers2() {
  function fn(config = [
    defaults.value,
    { nada: defaults.nada } = { nada: "nothing" }
  ] = ["BLAH"]) { return config; }
  var defaults = {};
  assertEquals(["BLAH"], fn());
  assertEquals("BLAH", defaults.value);
  assertEquals("nothing", defaults.nada);
})();


(function testDeclarationInitializers() {
  var defaults = {};
  var { value } = { value: defaults.value } = { value: "BLAH" };
  assertEquals("BLAH", value);
  assertEquals("BLAH", defaults.value);
})();


(function testDeclarationInitializers2() {
  var defaults = {};
  var [value] = [defaults.value] = ["BLAH"];
  assertEquals("BLAH", value);
  assertEquals("BLAH", defaults.value);
})();


(function testObjectLiteralProperty() {
  var ext = {};
  var obj = {
    a: { b: ext.b, c: ext["c"], d: { set v(val) { ext.d = val; } }.v } = {
      b: "b", c: "c", d: "d" }
  };
  assertEquals({ b: "b", c: "c", d: "d" }, ext);
  assertEquals({ a: { b: "b", c: "c", d: "d" } }, obj);
})();


(function testArrayLiteralProperty() {
  var ext = {};
  var obj = [
    ...[ ext.b, ext["c"], { set v(val) { ext.d = val; } }.v ] = [
      "b", "c", "d" ]
  ];
  assertEquals({ b: "b", c: "c", d: "d" }, ext);
  assertEquals([ "b", "c", "d" ], obj);
})();


// TODO(caitp): add similar test for ArrayPatterns, once Proxies support
// delegating symbol-keyed get/set.
(function testObjectPatternOperationOrder() {
  var steps = [];
  var store = {};
  function computePropertyName(name) {
    steps.push("compute name: " + name);
    return name;
  }
  function loadValue(descr, value) {
    steps.push("load: " + descr + " > " + value);
    return value;
  }
  function storeValue(descr, name, value) {
    steps.push("store: " + descr + " = " + value);
    store[name] = value;
  }
  var result = {
    get a() { assertUnreachable(); },
    set a(value) { storeValue("result.a", "a", value); },
    get b() { assertUnreachable(); },
    set b(value) { storeValue("result.b", "b", value); }
  };

  ({
    obj: {
      x: result.a = 10,
      [computePropertyName("y")]: result.b = false,
    } = {}
  } = { obj: {
    get x() { return loadValue(".temp.obj.x", undefined); },
    set x(value) { assertUnreachable(); },
    get y() { return loadValue(".temp.obj.y", undefined); },
    set y(value) { assertUnreachable(); }
  }});

  assertPropertiesEqual({
    a: 10,
    b: false
  }, store);

  assertArrayEquals([
    "load: .temp.obj.x > undefined",
    "store: result.a = 10",

    "compute name: y",
    "load: .temp.obj.y > undefined",
    "store: result.b = false"
  ], steps);

  steps = [];

  ({
    obj: {
      x: result.a = 50,
      [computePropertyName("y")]: result.b = "hello",
    } = {}
  } = { obj: {
    get x() { return loadValue(".temp.obj.x", 20); },
    set x(value) { assertUnreachable(); },
    get y() { return loadValue(".temp.obj.y", true); },
    set y(value) { assertUnreachable(); }
  }});

  assertPropertiesEqual({
    a: 20,
    b: true
  }, store);

  assertArrayEquals([
    "load: .temp.obj.x > 20",
    "store: result.a = 20",
    "compute name: y",
    "load: .temp.obj.y > true",
    "store: result.b = true",
  ], steps);
})();

// Credit to Mike Pennisi and other Test262 contributors for originally writing
// the testse the following are based on.
(function testArrayElision() {
  var value = [1, 2, 3, 4, 5, 6, 7, 8, 9];
  var a, obj = {};
  var result = [, a, , obj.b, , ...obj["rest"]] = value;

  assertEquals(result, value);
  assertEquals(2, a);
  assertEquals(4, obj.b);
  assertArrayEquals([6, 7, 8, 9], obj.rest);
})();

(function testArrayElementInitializer() {
  function test(value, initializer, expected) {
    var a, obj = {};
    var initialized = false;
    var shouldBeInitialized = value[0] === undefined;
    assertEquals(value, [ a = (initialized = true, initializer) ] = value);
    assertEquals(expected, a);
    assertEquals(shouldBeInitialized, initialized);

    var initialized2 = false;
    assertEquals(value, [ obj.a = (initialized2 = true, initializer) ] = value);
    assertEquals(expected, obj.a);
    assertEquals(shouldBeInitialized, initialized2);
  }

  test([], "BAM!", "BAM!");
  test([], "BOOP!", "BOOP!");
  test([null], 123, null);
  test([undefined], 456, 456);
  test([,], "PUPPIES", "PUPPIES");

  (function accept_IN() {
    var value = [], x;
    assertEquals(value, [ x = 'x' in {} ] = value);
    assertEquals(false, x);
  })();

  (function ordering() {
    var x = 0, a, b, value = [];
    assertEquals(value, [ a = x += 1, b = x *= 2 ] = value);
    assertEquals(1, a);
    assertEquals(2, b);
    assertEquals(2, x);
  })();

  (function yieldExpression() {
    var value = [], it, result, x;
    it = (function*() {
      result = [ x = yield ] = value;
    })();
    var next = it.next();

    assertEquals(undefined, result);
    assertEquals(undefined, next.value);
    assertEquals(false, next.done);
    assertEquals(undefined, x);

    next = it.next(86);

    assertEquals(value, result);
    assertEquals(undefined, next.value);
    assertEquals(true, next.done);
    assertEquals(86, x);
  })();

  (function yieldIdentifier() {
    var value = [], yield = "BOOP!", x;
    assertEquals(value, [ x = yield ] = value);
    assertEquals("BOOP!", x);
  })();

  assertThrows(function let_TDZ() {
    "use strict";
    var x;
    [ x = y ] = [];
    let y;
  }, ReferenceError);
})();


(function testArrayElementNestedPattern() {
  assertThrows(function nestedArrayRequireObjectCoercibleNull() {
    var x; [ [ x ] ] = [ null ];
  }, TypeError);

  assertThrows(function nestedArrayRequireObjectCoercibleUndefined() {
    var x; [ [ x ] ] = [ undefined ];
  }, TypeError);

  assertThrows(function nestedArrayRequireObjectCoercibleUndefined2() {
    var x; [ [ x ] ] = [ ];
  }, TypeError);

  assertThrows(function nestedArrayRequireObjectCoercibleUndefined3() {
    var x; [ [ x ] ] = [ , ];
  }, TypeError);

  assertThrows(function nestedObjectRequireObjectCoercibleNull() {
    var x; [ { x } ] = [ null ];
  }, TypeError);

  assertThrows(function nestedObjectRequireObjectCoercibleUndefined() {
    var x; [ { x } ] = [ undefined ];
  }, TypeError);

  assertThrows(function nestedObjectRequireObjectCoercibleUndefined2() {
    var x; [ { x } ] = [ ];
  }, TypeError);

  assertThrows(function nestedObjectRequireObjectCoercibleUndefined3() {
    var x; [ { x } ] = [ , ];
  }, TypeError);

  (function nestedArray() {
    var x, value = [ [ "zap", "blonk" ] ];
    assertEquals(value, [ [ , x ] ] = value);
    assertEquals("blonk", x);
  })();

  (function nestedObject() {
    var x, value = [ { a: "zap", b: "blonk" } ];
    assertEquals(value, [ { b: x } ] = value);
    assertEquals("blonk", x);
  })();
})();

(function testArrayRestElement() {
  (function testBasic() {
    var x, rest, array = [1, 2, 3];
    assertEquals(array, [x, ...rest] = array);
    assertEquals(1, x);
    assertEquals([2, 3], rest);

    array = [4, 5, 6];
    assertEquals(array, [, ...rest] = array);
    assertEquals([5, 6], rest);

  })();

  (function testNestedRestObject() {
    var value = [1, 2, 3], x;
    assertEquals(value, [...{ 1: x }] = value);
    assertEquals(2, x);
  })();

  (function iterable() {
    var count = 0;
    var x, y, z;
    function* g() {
      count++;
      yield;
      count++;
      yield;
      count++;
      yield;
    }
    var it = g();
    assertEquals(it, [...x] = it);
    assertEquals([undefined, undefined, undefined], x);
    assertEquals(3, count);

    it = [g()];
    assertEquals(it, [ [...y] ] = it);
    assertEquals([undefined, undefined, undefined], y);
    assertEquals(6, count);

    it = { a: g() };
    assertEquals(it, { a: [...z] } = it);
    assertEquals([undefined, undefined, undefined], z);
    assertEquals(9, count);
  })();
})();

(function testRequireObjectCoercible() {
  assertThrows(() => ({} = undefined), TypeError);
  assertThrows(() => ({} = null), TypeError);
  assertThrows(() => [] = undefined, TypeError);
  assertThrows(() => [] = null, TypeError);
  assertEquals("test", ({} = "test"));
  assertEquals("test", [] = "test");
  assertEquals(123, ({} = 123));
})();

(function testConstReassignment() {
  "use strict";
  const c = "untouchable";
  assertThrows(() => { [ c ] = [ "nope!" ]; }, TypeError);
  assertThrows(() => { [ [ c ] ]  = [ [ "nope!" ] ]; }, TypeError);
  assertThrows(() => { [ { c } ]  = [ { c: "nope!" } ]; }, TypeError);
  assertThrows(() => { ({ c } = { c: "nope!" }); }, TypeError);
  assertThrows(() => { ({ a: { c } } = { a: { c: "nope!" } }); }, TypeError);
  assertThrows(() => { ({ a: [ c ] } = { a: [ "nope!" ] }); }, TypeError);
  assertEquals("untouchable", c);
})();

(function testForIn() {
  var log = [];
  var x = {};
  var object = {
    "Apenguin": 1,
    "\u{1F382}cake": 2,
    "Bpuppy": 3,
    "Cspork": 4
  };
  for ([x.firstLetter, ...x.rest] in object) {
    if (x.firstLetter === "A") {
      assertEquals(["p", "e", "n", "g", "u", "i", "n"], x.rest);
      continue;
    }
    if (x.firstLetter === "C") {
      assertEquals(["s", "p", "o", "r", "k"], x.rest);
      break;
    }
    log.push({ firstLetter: x.firstLetter, rest: x.rest });
  }
  assertEquals([
    { firstLetter: "\u{1F382}", rest: ["c", "a", "k", "e"] },
    { firstLetter: "B", rest: ["p", "u", "p", "p", "y"] },
  ], log);
})();

(function testForOf() {
  var log = [];
  var x = {};
  var names = [
    "Apenguin",
    "\u{1F382}cake",
    "Bpuppy",
    "Cspork"
  ];
  for ([x.firstLetter, ...x.rest] of names) {
    if (x.firstLetter === "A") {
      assertEquals(["p", "e", "n", "g", "u", "i", "n"], x.rest);
      continue;
    }
    if (x.firstLetter === "C") {
      assertEquals(["s", "p", "o", "r", "k"], x.rest);
      break;
    }
    log.push({ firstLetter: x.firstLetter, rest: x.rest });
  }
  assertEquals([
    { firstLetter: "\u{1F382}", rest: ["c", "a", "k", "e"] },
    { firstLetter: "B", rest: ["p", "u", "p", "p", "y"] },
  ], log);
})();
