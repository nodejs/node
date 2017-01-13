// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-simd
// Flags: --allow-natives-syntax --expose-natives-as natives --noalways-opt

function lanesForType(typeName) {
  // The lane count follows the first 'x' in the type name, which begins with
  // 'float', 'int', or 'bool'.
  return Number.parseInt(typeName.substr(typeName.indexOf('x') + 1));
}


// Creates an instance that has been zeroed, so it can be used for equality
// testing.
function createInstance(type) {
  // Provide enough parameters for the longest type (currently 16). It's
  // important that instances be consistent to better test that different SIMD
  // types can't be compared and are never equal or the same in any sense.
  return SIMD[type](0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15);
}


function isValidSimdString(string, value, type, lanes) {
  var simdFn = SIMD[type],
      parseFn =
          type.indexOf('Float') === 0 ? Number.parseFloat : Number.parseInt,
      indexOfOpenParen = string.indexOf('(');
  // Check prefix (e.g. SIMD.Float32x4.)
  if (string.substr(0, indexOfOpenParen) !== 'SIMD.' + type)
    return false;
  // Remove type name (e.g. SIMD.Float32x4) and open parenthesis.
  string = string.substr(indexOfOpenParen + 1);
  var laneStrings = string.split(',');
  if (laneStrings.length !== lanes)
    return false;
  for (var i = 0; i < lanes; i++) {
    var fromString = parseFn(laneStrings[i]),
        fromValue = simdFn.extractLane(value, i);
    if (Math.abs(fromString - fromValue) > Number.EPSILON)
      return false;
  }
  return true;
}


var simdTypeNames = ['Float32x4', 'Int32x4', 'Uint32x4', 'Bool32x4',
                                  'Int16x8', 'Uint16x8', 'Bool16x8',
                                  'Int8x16', 'Uint8x16', 'Bool8x16'];

var nonSimdValues = [347, 1.275, NaN, "string", null, undefined, {},
                     function() {}];

function checkTypeMatrix(type, fn) {
  // Check against non-SIMD types.
  nonSimdValues.forEach(fn);
  // Check against SIMD values of a different type.
  for (var i = 0; i < simdTypeNames.length; i++) {
    var otherType = simdTypeNames[i];
    if (type != otherType) fn(createInstance(otherType));
  }
}


// Test different forms of constructor calls.
function TestConstructor(type, lanes) {
  var simdFn = SIMD[type];
  var instance = createInstance(type);

  assertFalse(Object === simdFn.prototype.constructor)
  assertFalse(simdFn === Object.prototype.constructor)
  assertSame(simdFn, simdFn.prototype.constructor)

  assertSame(simdFn, instance.__proto__.constructor)
  assertSame(simdFn, Object(instance).__proto__.constructor)
  assertSame(simdFn.prototype, instance.__proto__)
  assertSame(simdFn.prototype, Object(instance).__proto__)
}


function TestType(type, lanes) {
  var simdFn = SIMD[type];
  var instance = createInstance(type);
  var typeofString = type.charAt(0).toLowerCase() + type.slice(1);

  assertEquals(typeofString, typeof instance)
  assertTrue(typeof instance === typeofString)
  assertTrue(typeof Object(instance) === 'object')
  assertEquals(null, %_ClassOf(instance))
  assertEquals(type, %_ClassOf(Object(instance)))
}


function TestPrototype(type, lanes) {
  var simdFn = SIMD[type];
  var instance = createInstance(type);

  assertSame(Object.prototype, simdFn.prototype.__proto__)
  assertSame(simdFn.prototype, instance.__proto__)
  assertSame(simdFn.prototype, Object(instance).__proto__)
}


function TestValueOf(type, lanes) {
  var simdFn = SIMD[type];
  var instance = createInstance(type);

  assertTrue(instance === Object(instance).valueOf())
  assertTrue(instance === instance.valueOf())
  assertTrue(simdFn.prototype.valueOf.call(Object(instance)) === instance)
  assertTrue(simdFn.prototype.valueOf.call(instance) === instance)
}


function TestGet(type, lanes) {
  var simdFn = SIMD[type];
  var instance = createInstance(type);

  assertEquals(undefined, instance.a)
  assertEquals(undefined, instance["a" + "b"])
  assertEquals(undefined, instance["" + "1"])
  assertEquals(undefined, instance[42])
}


function TestToBoolean(type, lanes) {
  var simdFn = SIMD[type];
  var instance = createInstance(type);

  assertTrue(Boolean(Object(instance)))
  assertFalse(!Object(instance))
  assertTrue(Boolean(instance).valueOf())
  assertFalse(!instance)
  assertTrue(!!instance)
  assertTrue(instance && true)
  assertFalse(!instance && false)
  assertTrue(!instance || true)
  assertEquals(1, instance ? 1 : 2)
  assertEquals(2, !instance ? 1 : 2)
  if (!instance) assertUnreachable();
  if (instance) {} else assertUnreachable();
}


function TestToString(type, lanes) {
  var simdFn = SIMD[type];
  var instance = createInstance(type);

  assertEquals(instance.toString(), String(instance))
  assertTrue(isValidSimdString(instance.toString(), instance, type, lanes))
  assertTrue(
      isValidSimdString(Object(instance).toString(), instance, type, lanes))
  assertTrue(isValidSimdString(
      simdFn.prototype.toString.call(instance), instance, type, lanes))
}


function TestToNumber(type, lanes) {
  var simdFn = SIMD[type];
  var instance = createInstance(type);

  assertThrows(function() { Number(Object(instance)) }, TypeError)
  assertThrows(function() { +Object(instance) }, TypeError)
  assertThrows(function() { Number(instance) }, TypeError)
  assertThrows(function() { instance + 0 }, TypeError)
}


function TestCoercions(type, lanes) {
  var simdFn = SIMD[type];
  var instance = createInstance(type);
  // Test that setting a lane to value 'a' results in a lane with value 'b'.
  function test(a, b) {
    for (var i = 0; i < lanes; i++) {
      var ainstance = simdFn.replaceLane(instance, i, a);
      var lane_value = simdFn.extractLane(ainstance, i);
      assertSame(b, lane_value);
    }
  }

  switch (type) {
    case 'Float32x4':
      test(0, 0);
      test(-0, -0);
      test(NaN, NaN);
      test(null, 0);
      test(undefined, NaN);
      test("5.25", 5.25);
      test(Number.MAX_VALUE, Infinity);
      test(-Number.MAX_VALUE, -Infinity);
      test(Number.MIN_VALUE, 0);
      break;
    case 'Int32x4':
      test(Infinity, 0);
      test(-Infinity, 0);
      test(NaN, 0);
      test(0, 0);
      test(-0, 0);
      test(Number.MIN_VALUE, 0);
      test(-Number.MIN_VALUE, 0);
      test(0.1, 0);
      test(-0.1, 0);
      test(1, 1);
      test(1.1, 1);
      test(-1, -1);
      test(-1.6, -1);
      test(2147483647, 2147483647);
      test(2147483648, -2147483648);
      test(2147483649, -2147483647);
      test(4294967295, -1);
      test(4294967296, 0);
      test(4294967297, 1);
      break;
    case 'Uint32x4':
      test(Infinity, 0);
      test(-Infinity, 0);
      test(NaN, 0);
      test(0, 0);
      test(-0, 0);
      test(Number.MIN_VALUE, 0);
      test(-Number.MIN_VALUE, 0);
      test(0.1, 0);
      test(-0.1, 0);
      test(1, 1);
      test(1.1, 1);
      test(-1, 4294967295);
      test(-1.6, 4294967295);
      test(4294967295, 4294967295);
      test(4294967296, 0);
      test(4294967297, 1);
      break;
    case 'Int16x8':
      test(Infinity, 0);
      test(-Infinity, 0);
      test(NaN, 0);
      test(0, 0);
      test(-0, 0);
      test(Number.MIN_VALUE, 0);
      test(-Number.MIN_VALUE, 0);
      test(0.1, 0);
      test(-0.1, 0);
      test(1, 1);
      test(1.1, 1);
      test(-1, -1);
      test(-1.6, -1);
      test(32767, 32767);
      test(32768, -32768);
      test(32769, -32767);
      test(65535, -1);
      test(65536, 0);
      test(65537, 1);
      break;
    case 'Uint16x8':
      test(Infinity, 0);
      test(-Infinity, 0);
      test(NaN, 0);
      test(0, 0);
      test(-0, 0);
      test(Number.MIN_VALUE, 0);
      test(-Number.MIN_VALUE, 0);
      test(0.1, 0);
      test(-0.1, 0);
      test(1, 1);
      test(1.1, 1);
      test(-1, 65535);
      test(-1.6, 65535);
      test(65535, 65535);
      test(65536, 0);
      test(65537, 1);
      break;
    case 'Int8x16':
      test(Infinity, 0);
      test(-Infinity, 0);
      test(NaN, 0);
      test(0, 0);
      test(-0, 0);
      test(Number.MIN_VALUE, 0);
      test(-Number.MIN_VALUE, 0);
      test(0.1, 0);
      test(-0.1, 0);
      test(1, 1);
      test(1.1, 1);
      test(-1, -1);
      test(-1.6, -1);
      test(127, 127);
      test(128, -128);
      test(129, -127);
      test(255, -1);
      test(256, 0);
      test(257, 1);
      break;
    case 'Uint8x16':
      test(Infinity, 0);
      test(-Infinity, 0);
      test(NaN, 0);
      test(0, 0);
      test(-0, 0);
      test(Number.MIN_VALUE, 0);
      test(-Number.MIN_VALUE, 0);
      test(0.1, 0);
      test(-0.1, 0);
      test(1, 1);
      test(1.1, 1);
      test(-1, 255);
      test(-1.6, 255);
      test(255, 255);
      test(256, 0);
      test(257, 1);
      break;
    case 'Bool32x4':
    case 'Bool16x8':
    case 'Bool8x16':
      test(true, true);
      test(false, false);
      test(0, false);
      test(1, true);
      test(0.1, true);
      test(NaN, false);
      test(null, false);
      test("", false);
      test("false", true);
      break;
  }
}


function TestEquality(type, lanes) {
  var simdFn = SIMD[type];
  var instance = createInstance(type);

  // Every SIMD value should equal itself, and non-strictly equal its wrapper.
  assertSame(instance, instance)
  assertEquals(instance, instance)
  assertTrue(Object.is(instance, instance))
  assertTrue(instance === instance)
  assertTrue(instance == instance)
  assertFalse(instance === Object(instance))
  assertFalse(Object(instance) === instance)
  assertTrue(instance == Object(instance))
  assertTrue(Object(instance) == instance)
  assertTrue(instance === instance.valueOf())
  assertTrue(instance.valueOf() === instance)
  assertTrue(instance == instance.valueOf())
  assertTrue(instance.valueOf() == instance)
  assertFalse(Object(instance) === Object(instance))
  assertEquals(Object(instance).valueOf(), Object(instance).valueOf())

  function notEqual(other) {
    assertFalse(instance === other)
    assertFalse(other === instance)
    assertFalse(instance == other)
    assertFalse(other == instance)
  }

  // SIMD values should not be equal to instances of different types.
  checkTypeMatrix(type, function(other) {
    assertFalse(instance === other)
    assertFalse(other === instance)
    assertFalse(instance == other)
    assertFalse(other == instance)
  });

  // Test that f(a, b) is the same as f(SIMD(a), SIMD(b)) for equality and
  // strict equality, at every lane.
  function test(a, b) {
    for (var i = 0; i < lanes; i++) {
      var aval = simdFn.replaceLane(instance, i, a);
      var bval = simdFn.replaceLane(instance, i, b);
      assertSame(a == b, aval == bval);
      assertSame(a === b, aval === bval);
    }
  }

  switch (type) {
    case 'Float32x4':
      test(1, 2.5);
      test(1, 1);
      test(0, 0);
      test(-0, +0);
      test(+0, -0);
      test(-0, -0);
      test(0, NaN);
      test(NaN, NaN);
      break;
    case 'Int32x4':
    case 'Uint32x4':
    case 'Int16x8':
    case 'Uint16x8':
    case 'Int8x16':
    case 'Uint8x16':
      test(1, 2);
      test(1, 1);
      test(1, -1);
      break;
    case 'Bool32x4':
    case 'Bool16x8':
    case 'Bool8x16':
      test(true, false);
      test(false, true);
      break;
  }
}


function TestSameValue(type, lanes) {
  var simdFn = SIMD[type];
  var instance = createInstance(type);
  var sameValue = Object.is
  var sameValueZero = function(x, y) { return %SameValueZero(x, y); }

  // SIMD values should not be the same as instances of different types.
  checkTypeMatrix(type, function(other) {
    assertFalse(sameValue(instance, other));
    assertFalse(sameValueZero(instance, other));
  });

  // Test that f(a, b) is the same as f(SIMD(a), SIMD(b)) for sameValue and
  // sameValueZero, at every lane.
  function test(a, b) {
    for (var i = 0; i < lanes; i++) {
      var aval = simdFn.replaceLane(instance, i, a);
      var bval = simdFn.replaceLane(instance, i, b);
      assertSame(sameValue(a, b), sameValue(aval, bval));
      assertSame(sameValueZero(a, b), sameValueZero(aval, bval));
    }
  }

  switch (type) {
    case 'Float32x4':
      test(1, 2.5);
      test(1, 1);
      test(0, 0);
      test(-0, +0);
      test(+0, -0);
      test(-0, -0);
      test(0, NaN);
      test(NaN, NaN);
      break;
    case 'Int32x4':
    case 'Uint32x4':
    case 'Int16x8':
    case 'Uint16x8':
    case 'Int8x16':
    case 'Uint8x16':
      test(1, 2);
      test(1, 1);
      test(1, -1);
      break;
    case 'Bool32x4':
    case 'Bool16x8':
    case 'Bool8x16':
      test(true, false);
      test(false, true);
      break;
  }
}


function TestComparison(type, lanes) {
  var simdFn = SIMD[type];
  var a = createInstance(type), b = createInstance(type);

  function compare(other) {
    var throwFuncs = [
      function lt() { a < b; },
      function gt() { a > b; },
      function le() { a <= b; },
      function ge() { a >= b; },
      function lt_same() { a < a; },
      function gt_same() { a > a; },
      function le_same() { a <= a; },
      function ge_same() { a >= a; },
    ];

    for (var f of throwFuncs) {
      assertThrows(f, TypeError);
      %OptimizeFunctionOnNextCall(f);
      assertThrows(f, TypeError);
      assertThrows(f, TypeError);
    }
  }

  // Test comparison against the same SIMD type.
  compare(b);
  // Test comparison against other types.
  checkTypeMatrix(type, compare);
}


// Test SIMD value wrapping/boxing over non-builtins.
function TestCall(type, lanes) {
  var simdFn = SIMD[type];
  var instance = createInstance(type);
  simdFn.prototype.getThisProto = function () {
    return Object.getPrototypeOf(this);
  }
  assertTrue(instance.getThisProto() === simdFn.prototype)
}


function TestAsSetKey(type, lanes, set) {
  var simdFn = SIMD[type];
  var instance = createInstance(type);

  function test(set, key) {
    assertFalse(set.has(key));
    assertFalse(set.delete(key));
    if (!(set instanceof WeakSet)) {
      assertSame(set, set.add(key));
      assertTrue(set.has(key));
      assertTrue(set.delete(key));
    } else {
      // SIMD values can't be used as keys in WeakSets.
      assertThrows(function() { set.add(key) });
    }
    assertFalse(set.has(key));
    assertFalse(set.delete(key));
    assertFalse(set.has(key));
  }

  test(set, instance);
}


function TestAsMapKey(type, lanes, map) {
  var simdFn = SIMD[type];
  var instance = createInstance(type);

  function test(map, key, value) {
    assertFalse(map.has(key));
    assertSame(undefined, map.get(key));
    assertFalse(map.delete(key));
    if (!(map instanceof WeakMap)) {
      assertSame(map, map.set(key, value));
      assertSame(value, map.get(key));
      assertTrue(map.has(key));
      assertTrue(map.delete(key));
    } else {
      // SIMD values can't be used as keys in WeakMaps.
      assertThrows(function() { map.set(key, value) });
    }
    assertFalse(map.has(key));
    assertSame(undefined, map.get(key));
    assertFalse(map.delete(key));
    assertFalse(map.has(key));
    assertSame(undefined, map.get(key));
  }

  test(map, instance, {});
}


// Test SIMD type with Harmony reflect-apply.
function TestReflectApply(type) {
  var simdFn = SIMD[type];
  var instance = createInstance(type);

  function returnThis() { return this; }
  function returnThisStrict() { 'use strict'; return this; }
  function noop() {}
  function noopStrict() { 'use strict'; }
  var R = void 0;

  assertSame(SIMD[type].prototype,
             Object.getPrototypeOf(
                Reflect.apply(returnThis, instance, [])));
  assertSame(instance, Reflect.apply(returnThisStrict, instance, []));

  assertThrows(
      function() { 'use strict'; Reflect.apply(instance); }, TypeError);
  assertThrows(
      function() { Reflect.apply(instance); }, TypeError);
  assertThrows(
      function() { Reflect.apply(noopStrict, R, instance); }, TypeError);
  assertThrows(
      function() { Reflect.apply(noop, R, instance); }, TypeError);
}


function TestSIMDTypes() {
  for (var i = 0; i < simdTypeNames.length; ++i) {
    var type = simdTypeNames[i],
        lanes = lanesForType(type);
    TestConstructor(type, lanes);
    TestType(type, lanes);
    TestPrototype(type, lanes);
    TestValueOf(type, lanes);
    TestGet(type, lanes);
    TestToBoolean(type, lanes);
    TestToString(type, lanes);
    TestToNumber(type, lanes);
    TestCoercions(type, lanes);
    TestEquality(type, lanes);
    TestSameValue(type, lanes);
    TestComparison(type, lanes);
    TestCall(type, lanes);
    TestAsSetKey(type, lanes, new Set);
    TestAsSetKey(type, lanes, new WeakSet);
    TestAsMapKey(type, lanes, new Map);
    TestAsMapKey(type, lanes, new WeakMap);
    TestReflectApply(type);
  }
}
TestSIMDTypes();

// Tests for the global SIMD object.
function TestSIMDObject() {
  assertSame(typeof SIMD, 'object');
  assertSame(SIMD.constructor, Object);
  assertSame(Object.getPrototypeOf(SIMD), Object.prototype);
  assertSame(SIMD + "", "[object SIMD]");
  // The SIMD object is mutable.
  SIMD.foo = "foo";
  assertSame(SIMD.foo, "foo");
  delete SIMD.foo;
  delete SIMD.Bool8x16;
  assertSame(SIMD.Bool8x16, undefined);
}
TestSIMDObject()


function TestStringify(expected, input) {
  assertEquals(expected, JSON.stringify(input));
  assertEquals(expected, JSON.stringify(input, (key, value) => value));
  assertEquals(JSON.stringify(input, null, "="),
               JSON.stringify(input, (key, value) => value, "="));
}

TestStringify(undefined, SIMD.Float32x4(1, 2, 3, 4));
TestStringify('[null]', [SIMD.Float32x4(1, 2, 3, 4)]);
TestStringify('[{}]', [Object(SIMD.Float32x4(1, 2, 3, 4))]);
var simd_wrapper = Object(SIMD.Float32x4(1, 2, 3, 4));
TestStringify('{}', simd_wrapper);
simd_wrapper.a = 1;
TestStringify('{"a":1}', simd_wrapper);
