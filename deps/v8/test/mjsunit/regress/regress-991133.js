// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc --allow-natives-syntax --gc-interval=416
// Flags: --no-lazy --no-lazy-feedback-allocation --stress-lazy-source-positions

"use strict";
function WasmModuleBuilder() {}
(function () {
  try {
    BigIntPrototypeValueOf = BigInt.prototype.valueOf;
  } catch (e) {}
  failWithMessage = function failWithMessage(message) {
    throw new MjsUnitAssertionError(message);
  }
  assertSame = function assertSame(expected, found, name_opt) {
    if (Object.is(expected, found)) return;
    fail(prettyPrinted(expected), found, name_opt);
  };
  assertArrayEquals = function assertArrayEquals(expected, found, name_opt) {
    var start = "";
    if (name_opt) {
      start = name_opt + " - ";
    }
    assertEquals(expected.length, found.length, start + "array length");
    if (expected.length === found.length) {
      for (var i = 0; i < expected.length; ++i) {
        assertEquals(expected[i], found[i],
                     start + "array element at index " + i);
      }
    }
  };
  assertPropertiesEqual = function assertPropertiesEqual(expected, found,
                                                         name_opt) {
    if (!deepObjectEquals(expected, found)) {
      fail(expected, found, name_opt);
    }
  };
  assertToStringEquals = function assertToStringEquals(expected, found,
                                                       name_opt) {
    if (expected !== String(found)) {
      fail(expected, found, name_opt);
    }
  };
  assertTrue = function assertTrue(value, name_opt) {
    assertEquals(true, value, name_opt);
  };
  assertFalse = function assertFalse(value, name_opt) {
    assertEquals(false, value, name_opt);
  };
  assertNull = function assertNull(value, name_opt) {
    if (value !== null) {
      fail("null", value, name_opt);
    }
  };
  assertNotNull = function assertNotNull(value, name_opt) {
    if (value === null) {
      fail("not null", value, name_opt);
    }
  };
})();

function getRandomProperty(v, rand) { var properties = Object.getOwnPropertyNames(v); var proto = Object.getPrototypeOf(v); if (proto) { properties = properties.concat(Object.getOwnPropertyNames(proto)); } if (properties.includes("constructor") && v.constructor.hasOwnProperty("__proto__")) { properties = properties.concat(Object.getOwnPropertyNames(v.constructor.__proto__)); } if (properties.length == 0) { return "0"; } return properties[rand % properties.length]; }


var __v_11 = { Float32Array() {}, Uint8Array() {} };
var __v_17 = {};
try {
} catch(e) { print("Caught: " + e); }
function __f_0(){
}
function __f_3(a, b) {
    (a | 0) + (b | 0);
    return a;
}
function __f_23(expected, __f_20, ffi) {
}
try {
(function() {
  function __f_12(__v_11, foreign, buffer) {
    "use asm";
    var __v_18 = new __v_11.Uint8Array(buffer);
    var __v_8 = new __v_9.Int32Array(buffer);
    function __f_24(__v_23, __v_28) {
      __v_23 = __v_23 | 0;
      __v_28 = __v_28 | 0;
      __v_16[__v_23 >> 2] = __v_28;
    }
    function __f_19(__v_23, __v_28) {
      __v_21 = __v_19 | 0;
      __v_28 = __v_28 | 0;
      __v_18[__v_23 | 0] = __v_28;
    }
    function __f_10(__v_23) {
      __v_0 = __v_10 | 0;
      return __v_18[__v_23] | 0;
      gc();
    }
    function __f_13(__v_23) {
      __v_23 = __v_17 | 0;
      return __v_18[__v_16[__v_23 >> 2] | 0] | 0;
    }
    return {__f_10: __f_10, __f_13: __f_13, __f_24: __f_24, __f_19: __f_19};
  }
  var __v_15 = new ArrayBuffer(__v_17);
  var __v_13 = eval('(' + __f_3.toString() + ')');
  var __v_26 = __v_13(__v_11, null, __v_15);
  var __v_27 = { __f_10() {} };
  __f_3(__v_13);
  assertNotEquals(123, __v_27.__f_10(20));
  assertNotEquals(42, __v_27.__f_10(21));
  assertNotEquals(77, __v_27.__f_10(22));
  __v_26.__p_711994219 = __v_26[getRandomProperty(__v_26, 711994219)];
  __v_26.__defineGetter__(getRandomProperty(__v_26, 477679072), function() {  gc(); __v_16[getRandomProperty(__v_16, 1106800630)] = __v_1[getRandomProperty(__v_1, 1151799043)]; return __v_26.__p_711994219; });
  assertNotEquals(123, __v_27.__f_10(0));
  assertNotEquals(42, __v_27.__f_10(4));
  assertNotEquals(77, __v_27.__f_10(8));
})();
} catch(e) { print("Caught: " + e); }
function __f_18(__v_11, foreign, heap) {
  "use asm";
  var __v_12 = new __v_11.Float32Array(heap);
  var __v_14 = __v_11.Math.fround;
  function __f_20() {
    var __v_21 = 1.23;
    __v_12[0] = __v_21;
    return +__v_12[0];
  }
  return {__f_14: __f_20};
}
try {
__f_23(Math.fround(1.23), __f_3);
} catch(e) { print("Caught: " + e); }
try {
} catch(e) { print("Caught: " + e); }
function __f_25(
    imported_module_name, imported_function_name) {
  var __v_11 = new WasmModuleBuilder();
  var __v_25 = new WasmModuleBuilder();
  let imp = i => i + 3;
}
try {
__f_25('mod', 'foo');
__f_25('mod', '☺☺happy☺☺');
__f_25('☺☺happy☺☺', 'foo');
__f_25('☺☺happy☺☺', '☼+☃=☹');
} catch(e) { print("Caught: " + e); }
function __f_26(
    internal_name_mul, exported_name_mul, internal_name_add,
    exported_name_add) {
  var __v_25 = new WasmModuleBuilder();
}
try {
__f_26('☺☺mul☺☺', 'mul', '☺☺add☺☺', 'add');
__f_26('☺☺mul☺☺', '☺☺mul☺☺', '☺☺add☺☺', '☺☺add☺☺');
(function __f_27() {
  var __v_25 = new WasmModuleBuilder();
  __v_25.addFunction('three snowmen: ☃☃☃').addBody([]).exportFunc();
  assertThrows( () => __v_25.instantiate(), WebAssembly.CompileError, /Compiling function #0:"three snowmen: ☃☃☃" failed: /);
});
(function __f_28() {
  var __v_25 = new WasmModuleBuilder();
  __v_25.addImport('three snowmen: ☃☃☃', 'foo');
  assertThrows( () => __v_25.instantiate({}), TypeError, /WebAssembly.Instance\(\): Import #0 module="three snowmen: ☃☃☃" error: /);
});
(function __f_29() {
  __v_25.__defineGetter__(getRandomProperty(__v_25, 539246294), function() { gc(); return __f_21; });
  var __v_25 = new WasmModuleBuilder();
  __v_25.addImport('mod', 'three snowmen: ☃☃☃');
  assertThrows(
      () => __v_14.instantiate({mod: {}}), WebAssembly.LinkError,
      'WebAssembly.Instance\(\): Import #0 module="mod" function="three ' +
          'snowmen: ☃☃☃" error: function import requires a callable');
});
} catch(e) { print("Caught: " + e); }
