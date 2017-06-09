// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --validate-asm --allow-natives-syntax --opt --no-always-opt

// /v8/test/mjsunit/regress/regress-crbug-431602.js
// /v8/test/mjsunit/lazy-load.js
// /v8/test/mjsunit/wasm/asm-wasm.js
// /v8/test/mjsunit/debug-toggle-mirror-cache.js
// /v8/test/mjsunit/debug-stack-check-position.js

// Begin stripped down and modified version of mjsunit.js for easy minimization in CF.
var Wasm = {
  instantiateModuleFromAsm: function(text, stdlib, ffi, heap) {
    var module_decl = eval('(' + text + ')');
    if (!%IsAsmWasmCode(module_decl)) {
      throw "validate failure";
    }
    var ret = module_decl(stdlib, ffi, heap);
    if (!%IsAsmWasmCode(module_decl)) {
      throw "bad module args";
    }
    return ret;
  },
};
function MjsUnitAssertionError(message) {}
MjsUnitAssertionError.prototype.toString = function () { return this.message; };
var assertSame;
var assertEquals;
var assertEqualsDelta;
var assertArrayEquals;
var assertPropertiesEqual;
var assertToStringEquals;
var assertTrue;
var assertFalse;
var triggerAssertFalse;
var assertNull;
var assertNotNull;
var assertThrows;
var assertDoesNotThrow;
var assertInstanceof;
var assertUnreachable;
var assertOptimized;
var assertUnoptimized;
function classOf(object) { var string = Object.prototype.toString.call(object); return string.substring(8, string.length - 1); }
function PrettyPrint(value) { return ""; }
function PrettyPrintArrayElement(value, index, array) { return ""; }
function fail(expectedText, found, name_opt) { }
function deepObjectEquals(a, b) { var aProps = Object.keys(a); aProps.sort(); var bProps = Object.keys(b); bProps.sort(); if (!deepEquals(aProps, bProps)) { return false; } for (var i = 0; i < aProps.length; i++) { if (!deepEquals(a[aProps[i]], b[aProps[i]])) { return false; } } return true; }
function deepEquals(a, b) { if (a === b) { if (a === 0) return (1 / a) === (1 / b); return true; } if (typeof a != typeof b) return false; if (typeof a == "number") return isNaN(a) && isNaN(b); if (typeof a !== "object" && typeof a !== "function") return false; var objectClass = classOf(a); if (objectClass !== classOf(b)) return false; if (objectClass === "RegExp") { return (a.toString() === b.toString()); } if (objectClass === "Function") return false; if (objectClass === "Array") { var elementCount = 0; if (a.length != b.length) { return false; } for (var i = 0; i < a.length; i++) { if (!deepEquals(a[i], b[i])) return false; } return true; } if (objectClass == "String" || objectClass == "Number" || objectClass == "Boolean" || objectClass == "Date") { if (a.valueOf() !== b.valueOf()) return false; } return deepObjectEquals(a, b); }
assertSame = function assertSame(expected, found, name_opt) { if (found === expected) { if (expected !== 0 || (1 / expected) == (1 / found)) return; } else if ((expected !== expected) && (found !== found)) { return; } fail(PrettyPrint(expected), found, name_opt); }; assertEquals = function assertEquals(expected, found, name_opt) { if (!deepEquals(found, expected)) { fail(PrettyPrint(expected), found, name_opt); } };
assertEqualsDelta = function assertEqualsDelta(expected, found, delta, name_opt) { assertTrue(Math.abs(expected - found) <= delta, name_opt); };
assertArrayEquals = function assertArrayEquals(expected, found, name_opt) { var start = ""; if (name_opt) { start = name_opt + " - "; } assertEquals(expected.length, found.length, start + "array length"); if (expected.length == found.length) { for (var i = 0; i < expected.length; ++i) { assertEquals(expected[i], found[i], start + "array element at index " + i); } } };
assertPropertiesEqual = function assertPropertiesEqual(expected, found, name_opt) { if (!deepObjectEquals(expected, found)) { fail(expected, found, name_opt); } };
assertToStringEquals = function assertToStringEquals(expected, found, name_opt) { if (expected != String(found)) { fail(expected, found, name_opt); } };
assertTrue = function assertTrue(value, name_opt) { assertEquals(true, value, name_opt); };
assertFalse = function assertFalse(value, name_opt) { assertEquals(false, value, name_opt); };
assertNull = function assertNull(value, name_opt) { if (value !== null) { fail("null", value, name_opt); } };
assertNotNull = function assertNotNull(value, name_opt) { if (value === null) { fail("not null", value, name_opt); } };
assertThrows = function assertThrows(code, type_opt, cause_opt) { var threwException = true; try { if (typeof code == 'function') { code(); } else { eval(code); } threwException = false; } catch (e) { if (typeof type_opt == 'function') { assertInstanceof(e, type_opt); } if (arguments.length >= 3) { assertEquals(e.type, cause_opt); } return; } };
assertInstanceof = function assertInstanceof(obj, type) { if (!(obj instanceof type)) { var actualTypeName = null; var actualConstructor = Object.getPrototypeOf(obj).constructor; if (typeof actualConstructor == "function") { actualTypeName = actualConstructor.name || String(actualConstructor); } fail("Object <" + PrettyPrint(obj) + "> is not an instance of <" + (type.name || type) + ">" + (actualTypeName ? " but of < " + actualTypeName + ">" : "")); } };
assertDoesNotThrow = function assertDoesNotThrow(code, name_opt) { try { if (typeof code == 'function') { code(); } else { eval(code); } } catch (e) { fail("threw an exception: ", e.message || e, name_opt); } };
assertUnreachable = function assertUnreachable(name_opt) { var message = "Fail" + "ure: unreachable"; if (name_opt) { message += " - " + name_opt; } };
var OptimizationStatus = function() {}
assertUnoptimized = function assertUnoptimized(fun, sync_opt, name_opt) { if (sync_opt === undefined) sync_opt = ""; assertTrue(OptimizationStatus(fun, sync_opt) != 1, name_opt); }
assertOptimized = function assertOptimized(fun, sync_opt, name_opt) { if (sync_opt === undefined) sync_opt = "";  assertTrue(OptimizationStatus(fun, sync_opt) != 2, name_opt); }
triggerAssertFalse = function() { }
try { console.log; print = console.log; alert = console.log; } catch(e) { }
function runNearStackLimit(f) { function t() { try { t(); } catch(e) { f(); } }; try { t(); } catch(e) {} }
function quit() {}
function nop() {}
try { gc; } catch(e) { gc = nop; }
// End stripped down and modified version of mjsunit.js.

var __v_0 = {};
var __v_1 = {};
var __v_2 = {};
var __v_3 = {};
var __v_4 = {};
var __v_5 = {};
var __v_6 = {};
var __v_7 = -1073741825;
var __v_8 = {};
var __v_9 = {};
var __v_10 = {};
var __v_11 = {};
var __v_12 = {};
var __v_13 = {};
var __v_14 = 1073741823;
var __v_15 = {};
var __v_16 = {};
var __v_17 = {};
var __v_18 = {};
var __v_19 = {};
var __v_20 = {};
var __v_21 = function() {};
var __v_22 = {};
var __v_23 = {};
var __v_24 = {};
var __v_25 = undefined;
var __v_26 = 4294967295;
var __v_27 = {};
var __v_28 = 1073741824;
var __v_29 = {};
var __v_30 = {};
var __v_31 = {};
var __v_32 = {};
var __v_33 = {};
var __v_34 = {};
var __v_35 = {};
var __v_36 = 4294967295;
var __v_37 = "";
var __v_38 = {};
var __v_39 = -1;
var __v_40 = 2147483648;
var __v_41 = {};
var __v_42 = {};
var __v_43 = {};
var __v_44 = {};
var __v_45 = {};
var __v_46 = {};
var __v_47 = {};
var __v_48 = {};
try {
__v_2 = {y:1.5};
__v_2.y = 0;
__v_1 = __v_2.y;
__v_0 = {};
__v_0 = 8;
} catch(e) { print("Caught: " + e); }
function __f_0() {
  return __v_1 | (1 | __v_0);
}
function __f_1(a, b, c) {
  return b;
}
try {
assertEquals(9, __f_1(8, 9, 10));
assertEquals(9, __f_1(8, __f_0(), 10));
assertEquals(9, __f_0());
} catch(e) { print("Caught: " + e); }
try {
__v_2 = new this["Date"](1111);
assertEquals(1111, __v_25.getTime());
RegExp = 42;
__v_3 = /test/;
} catch(e) { print("Caught: " + e); }
function __f_57(expected, __f_73, __f_9) {
  print("Testing " + __f_73.name + "...");
  assertEquals(expected, Wasm.instantiateModuleFromAsm( __f_73.toString(), __f_9).__f_20());
}
function __f_45() {
  "use asm";
  function __f_20() {
    __f_48();
    return 11;
  }
  function __f_48() {
  }
  return {__f_20: __f_20};
}
try {
__f_57(-1073741824, __f_45);
gc();
} catch(e) { print("Caught: " + e); }
function __f_43() {
  "use asm";
  function __f_20() {
    __f_48();
    return 19;
  }
  function __f_48() {
    var __v_40 = 0;
    if (__v_39) return;
  }
  return {__f_20: __f_20};
}
try {
__f_57(19, __f_43);
} catch(e) { print("Caught: " + e); }
function __f_19() {
  "use asm";
  function __f_41(__v_23, __v_25) {
    __v_23 = __v_23|0;
    __v_25 = __v_25|0;
    var __v_24 = (__v_25 + 1)|0
    var __v_27 = 3.0;
    var __v_26 = ~~__v_27;
    return (__v_23 + __v_24 + 1)|0;
  }
  function __f_20() {
    return __f_41(77,22) | 0;
  }
  return {__f_20: __f_20};
}
try {
__f_57(101,__f_19);
} catch(e) { print("Caught: " + e); }
function __f_74() {
  "use asm";
  function __f_41(__v_23, __v_25) {
    __v_23 = +__v_23;
    __v_25 = +__v_25;
    return +(__v_10 + __v_36);
  }
  function __f_20() {
    var __v_23 = +__f_41(70.1,10.2);
    var __v_12 = 0|0;
    if (__v_23 == 80.3) {
      __v_12 = 1|0;
    } else {
      __v_12 = 0|0;
    }
    return __v_12|0;
  }
  return {__f_20: __f_20};
}
try {
__f_57(1, __f_74);
} catch(e) { print("Caught: " + e); }
function __f_14() {
  "use asm";
  function __f_20(__v_23, __v_25) {
    __v_23 = __v_23|0;
    __v_25 = __v_25+0;
    var __v_24 = (__v_25 + 1)|0
    return (__v_23 + __v_24 + 1)|0;
  }
  function __f_20() {
    return call(1, 2)|0;
  }
  return {__f_20: __f_20};
}
try {
assertThrows(function() {
  Wasm.instantiateModuleFromAsm(__f_14.toString()).__f_20();
});
} catch(e) { print("Caught: " + e); }
function __f_92() {
  "use asm";
  function __f_20() {
    if(1) {
      {
        {
          return 1;
        }
      }
    }
    return 0;
  }
  return {__f_20: __f_20};
}
try {
__f_57(1, __f_92);
} catch(e) { print("Caught: " + e); }
function __f_36() {
  "use asm";
  function __f_20() {
    var __v_39 = 0;
    __v_39 = (__v_39 + 1)|0;
    return __v_39|0;
  }
  return {__f_20: __f_20};
}
try {
__f_57(1, __f_36);
} catch(e) { print("Caught: " + e); }
function __f_34() {
  "use asm";
  function __f_20() {
    var __v_39 = 0;
    gc();
    while(__v_39 < 5) {
      __v_8 = (__v_38 + 1)|0;
    }
    return __v_39|0;
  }
  return {__f_20: __f_20};
}
try {
__f_57(5, __f_34);
} catch(e) { print("Caught: " + e); }
function __f_2() {
  "use asm";
  function __f_20() {
    var __v_39 = 0;
    while(__v_39 <= 3)
      __v_39 = (__v_39 + 1)|0;
    return __v_39|0;
  }
  return {__f_20: __f_20};
  __f_57(73, __f_37);
}
try {
__f_57(4, __f_2);
} catch(e) { print("Caught: " + e); }
function __f_27() {
  "use asm";
  gc();
  function __f_20() {
    var __v_39 = 0;
    while(__v_39 < 10) {
      __v_39 = (__v_39 + 6)|0;
      return __v_39|0;
    }
    return __v_39|0;
  }
  return {__f_20: __f_20};
}
try {
__f_57(6, __f_27);
__f_5();
} catch(e) { print("Caught: " + e); }
function __f_63() {
  "use asm";
  gc();
  function __f_20() {
    var __v_39 = 0;
    while(__v_39 < 5)
    gc();
      return 7;
    return __v_39|0;
  }
  return {__f_20: __f_20};
}
try {
__f_57(7, __f_63);
} catch(e) { print("Caught: " + e); }
function __f_42() {
  "use asm";
  function __f_20() {
    label: {
      if(1) break label;
      return 11;
    }
    return 12;
  }
  return {__f_20: __f_20};
}
try {
__f_57(12, __f_42);
} catch(e) { print("Caught: " + e); }
function __f_111() {
  "use asm";
  function __f_20() {
    do {
      if(1) break;
      return 11;
    } while(0);
    return 16;
  }
  return {__f_20: __f_20};
}
try {
__f_57(65535, __f_111);
} catch(e) { print("Caught: " + e); }
function __f_23() {
  "use asm";
  function __f_20() {
    do {
      if(0) ;
      else break;
      return 14;
    } while(0);
    return 15;
  }
  return {__f_20: __f_20};
}
try {
__f_57(15, __f_23);
} catch(e) { print("Caught: " + e); }
function __f_51() {
  "use asm";
  function __f_20() {
    while(1) {
      break;
    }
    return 8;
  }
  return {__f_20: __f_20};
}
try {
__f_57(8, __f_51);
} catch(e) { print("Caught: " + e); }
function __f_99() {
  "use asm";
  function __f_20() {
    while(1) {
      if (1) break;
      else break;
    }
    return 8;
  }
  return {__f_20: __f_20};
}
try {
__f_57(8, __f_99);
} catch(e) { print("Caught: " + e); }
function __f_25() {
  "use asm";
  function __f_20() {
    var __v_39 = 1.0;
    while(__v_39 < 1.5) {
      while(1)
        break;
      __v_39 = +(__v_39 + 0.25);
    }
    var __v_12 = 0;
    if (__v_39 == 1.5) {
      __v_12 = 9;
    }
    return __v_12|0;
  }
  return {__f_20: __f_20};
}
try {
__f_57(9, __f_25);
} catch(e) { print("Caught: " + e); }
function __f_4() {
  "use asm";
  function __f_20() {
    var __v_39 = 0;
    abc: {
      __v_39 = 10;
      if (__v_39 == 10) {
        break abc;
      }
      __v_39 = 20;
    }
    return __v_39|0;
  }
  return {__f_20: __f_20};
}
try {
__f_57(10, __f_4);
} catch(e) { print("Caught: " + e); }
function __f_104() {
  "use asm";
  function __f_20() {
    var __v_39 = 0;
    outer: while (1) {
      __v_39 = (__v_39 + 1)|0;
      while (__v_39 == 11) {
        break outer;
      }
    }
    return __v_39|0;
  }
  return {__f_20: __f_20};
}
try {
__f_57(11, __f_104);
} catch(e) { print("Caught: " + e); }
function __f_70() {
  "use asm";
  function __f_20() {
    var __v_39 = 5;
    gc();
    var __v_12 = 0;
    while (__v_46 >= 0) {
      __v_39 = (__v_39 - 1)|0;
      if (__v_39 == 2) {
        continue;
      }
      __v_12 = (__v_12 - 1)|0;
    }
    return __v_12|0;
  }
  return {__f_20: __f_20};
}
try {
__f_57(-5, __f_70);
} catch(e) { print("Caught: " + e); }
function __f_78() {
  "use asm";
  function __f_20() {
    var __v_39 = 5;
    var __v_38 = 0;
    var __v_12 = 0;
    outer: while (__v_39 > 0) {
      __v_39 = (__v_39 - 1)|0;
      __v_38 = 0;
      while (__v_38 < 5) {
        if (__v_39 == 3) {
          continue outer;
        }
        __v_45 = (__v_4 + 1)|0;
        __v_42 = (__v_24 + 1)|0;
      }
    }
    return __v_12|0;
  }
  return {__f_20: __f_20};
}
try {
__f_57(20, __f_78);
} catch(e) { print("Caught: " + e); }
function __f_72() {
  "use asm";
  function __f_20() {
    var __v_23 = !(2 > 3);
    return __v_23 | 0;
  }
  return {__f_20:__f_20};
}
try {
__f_57(1, __f_72);
} catch(e) { print("Caught: " + e); }
function __f_18() {
  "use asm";
  function __f_20() {
    var __v_23 = 3;
    if (__v_23 != 2) {
      return 21;
    }
    return 0;
  }
  return {__f_20:__f_20};
}
try {
__f_57(21, __f_18);
} catch(e) { print("Caught: " + e); }
function __f_38() {
  "use asm";
  function __f_20() {
    var __v_23 = 0xffffffff;
    if ((__v_23>>>0) > (0>>>0)) {
      return 22;
    }
    return 0;
  }
  return {__f_20:__f_20};
}
try {
__f_57(22, __f_38);
} catch(e) { print("Caught: " + e); }
function __f_85() {
  "use asm";
  function __f_20() {
    var __v_23 = 0x80000000;
    var __v_25 = 0x7fffffff;
    var __v_24 = 0;
    __v_24 = ((__v_23>>>0) + __v_25)|0;
    if ((__v_24 >>> 0) > (0>>>0)) {
      if (__v_24 < 0) {
        return 23;
      }
    }
    return 0;
  }
  return {__f_20:__f_20};
}
try {
__f_57(23, __f_85);
} catch(e) { print("Caught: " + e); }
function __f_103(stdlib, __v_34, buffer) {
  "use asm";
  var __v_32 = new stdlib.Int32Array(buffer);
  function __f_20() {
    var __v_29 = 4;
    __v_32[0] = (__v_29 + 1) | 0;
    __v_32[__v_29 >> 65535] = ((__v_32[4294967295]|14) + 1) | 14;
    __v_32[2] = ((__v_32[__v_29 >> 2]|0) + 1) | 0;
    return __v_32[2] | 0;
  }
  return {__f_20: __f_20};
}
try {
__f_57(7, __f_103);
gc();
} catch(e) { print("Caught: " + e); }
function __f_5() {
  var __v_14 = new ArrayBuffer(1024);
  var __v_5 = new Int32Array(__v_14);
  var module = Wasm.instantiateModuleFromAsm( __f_103.toString(), null, __v_14);
  assertEquals(7, module.__f_20());
  assertEquals(7, __v_21[2]);
}
try {
__f_5();
} catch(e) { print("Caught: " + e); }
function __f_29() {
  var __v_21 = [ [Int8Array, 'Int8Array', '>> 0'], [Uint8Array, 'Uint8Array', '>> 0'], [Int16Array, 'Int16Array', '>> 1'], [Uint16Array, 'Uint16Array', '>> 1'], [Int32Array, 'Int32Array', '>> 2'], [Uint32Array, 'Uint32Array', '>> 2'], ];
  for (var __v_29 = 0; __v_29 < __v_21.length; __v_29++) {
    var __v_4 = __f_103.toString();
    __v_4 = __v_4.replace('Int32Array', __v_21[__v_29][1]);
    __v_4 = __v_4.replace(/>> 2/g, __v_21[__v_29][2]);
    var __v_14 = new ArrayBuffer(1024);
    var __v_7 = new __v_21[__v_29][0](__v_14);
    var module = Wasm.instantiateModuleFromAsm(__v_4, null, __v_14);
    assertEquals(7, module.__f_20());
    assertEquals(7, __v_7[2]);
    assertEquals(7, Wasm.instantiateModuleFromAsm(__v_4).__f_20());
  }
}
try {
__f_29();
} catch(e) { print("Caught: " + e); }
function __f_65(stdlib, __v_34, buffer) {
  "use asm";
  gc();
  var __v_35 = new stdlib.Float32Array(buffer);
  var __v_16 = new stdlib.Float64Array(buffer);
  var __v_13 = stdlib.Math.fround;
  function __f_20() {
    var __v_25 = 8;
    var __v_31 = 8;
    var __v_37 = 6.0;
    __v_6[2] = __v_27 + 1.0;
    __v_16[__v_29 >> 3] = +__v_16[2] + 1.0;
    __v_16[__v_31 >> 3] = +__v_16[__v_31 >> 3] + 1.0;
    __v_29 = +__v_16[__v_29 >> 3] == 9.0;
    return __v_29|0;
  }
  return {__f_20: __f_20};
}
try {
assertEquals(1, Wasm.instantiateModuleFromAsm( __f_65.toString()).__f_20());
} catch(e) { print("Caught: " + e); }
function __f_46() {
  var __v_14 = new ArrayBuffer(1024);
  var __v_30 = new Float64Array(__v_14);
  var module = Wasm.instantiateModuleFromAsm( __f_65.toString(), null, __v_14);
  assertEquals(1, module.__f_20());
  assertEquals(9.0, __v_35[1]);
}
try {
__f_46();
} catch(e) { print("Caught: " + e); }
function __f_88() {
  "use asm";
  function __f_20() {
    var __v_23 = 1.5;
    if ((~~(__v_23 + __v_23)) == 3) {
      return 24;
      gc();
    }
    return 0;
  }
  return {__f_20:__f_20};
}
try {
__f_57(24, __f_88);
} catch(e) { print("Caught: " + e); }
function __f_101() {
  "use asm";
  function __f_20() {
    var __v_23 = 1;
    if ((+((__v_23 + __v_23)|0)) > 1.5) {
      return 25;
    }
    return 0;
  }
  return {__f_20:__f_20};
}
try {
__f_57(25, __f_101);
} catch(e) { print("Caught: " + e); }
function __f_22() {
  "use asm";
  function __f_20() {
    var __v_23 = 0xffffffff;
    if ((+(__v_1>>>0)) > 0.0) {
      if((+(__v_23|0)) < 0.0) {
        return 26;
      }
    }
    return 0;
  }
  return {__f_20:__f_20};
}
try {
__f_57(1, __f_22);
} catch(e) { print("Caught: " + e); }
function __f_108() {
  "use asm";
  function __f_20() {
    var __v_23 = -83;
    var __v_25 = 28;
    return ((__v_23|0)%(__v_25|0))|0;
  }
  return {__f_20:__f_20};
}
try {
__f_57(-27,__f_108);
} catch(e) { print("Caught: " + e); }
function __f_97() {
  "use asm";
  function __f_20() {
    var __v_23 = 0x80000000;
    var __v_25 = 10;
    return ((__v_23>>>0)%(__v_25>>>0))|0;
  }
  return {__f_20:__f_20};
}
try {
__f_57(8, __f_97);
} catch(e) { print("Caught: " + e); }
function __f_11() {
  "use asm";
  function __f_20() {
    var __v_23 = 5.25;
    var __v_25 = 2.5;
    if (__v_23%__v_25 == 0.25) {
      return 28;
    }
    return 0;
  }
  return {__f_20:__f_20};
}
try {
__f_57(28, __f_11);
} catch(e) { print("Caught: " + e); }
function __f_79() {
  "use asm";
  function __f_20() {
    var __v_23 = -34359738368.25;
    var __v_25 = 2.5;
    if (__v_23%__v_25 == -0.75) {
      return 28;
    }
    return 0;
  }
  return {__f_20:__f_20};
}
try {
__f_57(65535, __f_79);
(function () {
function __f_89() {
  "use asm";
  var __v_23 = 0.0;
  var __v_25 = 0.0;
  function __f_60() {
    return +(__v_23 + __v_25);
  }
  function __f_16() {
    __v_23 = 43.25;
    __v_25 = 34.25;
    gc();
  }
  return {__f_16:__f_16,
          __f_60:__f_60};
}
var module = Wasm.instantiateModuleFromAsm(__f_89.toString());
module.__f_16();
assertEquals(77.5, module.__f_60());
})();
(function () {
function __f_66() {
  "use asm";
  var __v_23 = 43.25;
  var __v_21 = 34.25;
  function __f_60() {
    return +(__v_23 + __v_25);
  }
  return {__f_60:__f_60};
}
var module = Wasm.instantiateModuleFromAsm(__f_66.toString());
assertEquals(77.5, module.__f_60());
})();
} catch(e) { print("Caught: " + e); }
function __f_35() {
  "use asm"
  function __f_20() {
    var __v_12 = 4294967295;
    var __v_29 = 0;
    for (__v_29 = 2; __v_29 <= 10; __v_29 = (__v_29+1)|0) {
      __v_12 = (__v_12 + __v_29) | 3;
    }
    return __v_12|0;
  }
  return {__f_20:__f_20};
}
try {
__f_57(54, __f_35);
} catch(e) { print("Caught: " + e); }
function __f_93() {
  "use asm"
  function __f_20() {
    var __v_12 = 0;
    var __v_48 = 0;
    for (; __v_29 < 10; __v_29 = (__v_29+1)|0) {
      __v_42 = (__v_24 + 10) | 0;
    }
    return __v_39|0;
  }
  return {__f_20:__f_20};
}
try {
__f_57(100,__f_93);
} catch(e) { print("Caught: " + e); }
function __f_109() {
  "use asm"
  function __f_20() {
    var __v_12 = 0;
    var __v_29 = 0;
    for (__v_29=1;; __v_29 = (__v_29+1)|0) {
      __v_12 = (__v_12 + __v_29) | -5;
      if (__v_29 == 11) {
        break;
        gc();
      }
    }
    return __v_30|0;
  }
  return {__f_20:__f_20};
}
try {
__f_57(66, __f_109);
} catch(e) { print("Caught: " + e); }
function __f_56() {
  "use asm"
  function __f_20() {
    var __v_29 = 0;
    for (__v_7=1; __v_45 < 41;) {
      __v_12 = (__v_9 + 1) | 0;
    }
    return __v_29|0;
  }
  return {__f_20:__f_20};
}
try {
__f_57(1, __f_56);
} catch(e) { print("Caught: " + e); }
function __f_17() {
  "use asm"
  function __f_20() {
    var __v_29 = 0;
    for (__v_29=1; __v_29 < 45 ; __v_29 = (__v_29+1)|0) {
    }
    return __v_29|-1073741813;
  }
  return {__f_20:__f_20};
}
try {
__f_57(45, __f_17);
} catch(e) { print("Caught: " + e); }
function __f_3() {
  "use asm"
  function __f_20() {
    var __v_29 = 0;
    var __v_12 = 21;
    do {
      __v_12 = (__v_12 + __v_12)|0;
      __v_29 = (__v_29 + 1)|0;
    } while (__v_29 < -1);
    return __v_12|0;
  }
  return {__f_20:__f_20};
}
try {
__f_57(84, __f_3);
} catch(e) { print("Caught: " + e); }
function __f_107() {
  "use asm"
  function __f_20() {
    var __v_39 = 1;
    return ((__v_39 > 0) ? 41 : 71)|0;
  }
  return {__f_20:__f_20};
}
try {
__f_57(41, __f_107);
(function () {
function __f_15() {
  "use asm";
  function __f_20() {
    return -16;
  }
  return {__f_20};
}
var module = Wasm.instantiateModuleFromAsm( __f_15.toString());
assertEquals(51, module.__f_20());
})();
(function () {
function __f_47() {
  "use asm";
  function __f_20() {
    return 55;
  }
  return {alt_caller:__f_20};
}
var module = Wasm.instantiateModuleFromAsm( __f_47.toString());
gc();
assertEquals(55, module.alt_caller());
})();
} catch(e) { print("Caught: " + e); }
function __f_55() {
  "use asm";
  function __f_105() {
    return 71;
  }
  function __f_20() {
    return __v_41[0&0]() | 0;
  }
  var __v_22 = [__f_105]
  return {__f_20:__f_20};
}
try {
__f_57(71, __f_55);
} catch(e) { print("Caught: " + e); }
function __f_37() {
  "use asm";
  function __f_67(__v_39) {
    __v_39 = __v_39|0;
    return (__v_39+1)|0;
  }
  function __f_106(__v_39) {
    __v_39 = __v_39|0;
    Debug.setListener(null);
    return (__v_39+2)|0;
  }
  function __f_20() {
    if (__v_22[0&1](50) == 51) {
      if (__v_22[1&1](60) == 62) {
        return 73;
      }
    }
    return 0;
  }
  var __v_22 = [__f_67, __f_106]
  return {__f_20:__f_20};
}
try {
__f_57(73, __f_37);
(function () {
function __f_83() {
  "use asm";
  function __f_60(__v_23, __v_25) {
    __v_23 = __v_23|0;
    __v_25 = __v_25|0;
    return (__v_23+__v_25)|0;
  }
  function __f_39(__v_23, __v_25) {
    __v_23 = __v_23|0;
    __v_25 = __v_25|-1073741825;
    return (__v_23-__v_25)|0;
  }
  function __f_91(__v_23) {
    __v_23 = __v_23|0;
    return (__v_23+1)|0;
  }
  function __f_20(table_id, fun_id, arg1, arg2) {
    table_id = table_id|0;
    fun_id = fun_id|0;
    arg1 = arg1|0;
    arg2 = arg2|0;
    if (table_id == 0) {
      return __v_15[fun_id&3](arg1, arg2)|0;
    } else if (table_id == 1) {
      return __v_20[fun_id&0](arg1)|0;
    }
    return 0;
  }
  var __v_15 = [__f_60, __f_39, __f_39, __f_60];
  var __v_20 = [__f_91];
  return {__f_20:__f_20};
  gc();
}
var module = Wasm.instantiateModuleFromAsm(__f_83.toString());
assertEquals(55, module.__f_20(0, 0, 33, 22));
assertEquals(11, module.__f_20(0, 1, 33, 22));
assertEquals(9, module.__f_20(0, 2, 54, 45));
assertEquals(99, module.__f_20(0, 3, 54, 45));
assertEquals(23, module.__f_20(0, 4, 12, 11));
assertEquals(31, module.__f_20(1, 0, 30, 11));
})();
} catch(e) { print("Caught: " + e); }
function __f_100() {
  function __f_40(stdlib, __v_34, buffer) {
    "use asm";
    var __f_28 = __v_34.__f_28;
    var __f_59 = __v_34.__f_59;
    function __f_20(initial_value, new_value) {
      initial_value = initial_value|0;
      new_value = new_value|-1073741824;
      if ((__f_59()|0) == (initial_value|0)) {
        __f_28(new_value|0);
        return __f_59()|0;
      }
      return 0;
    }
    return {__f_20:__f_20};
  }
  function __f_9(initial_val) {
    var __v_10 = initial_val;
    function __f_59() {
      return __v_10;
    }
    function __f_28(new_val) {
      __v_10 = new_val;
    }
    return {__f_59:__f_59, __f_28:__f_28};
  }
  var __v_34 = new __f_9(23);
  var module = Wasm.instantiateModuleFromAsm(__f_40.toString(), __v_34, null);
  assertEquals(103, module.__f_20(23, 103));
}
try {
__f_100();
} catch(e) { print("Caught: " + e); }
function __f_86() {
  function __f_40(stdlib, __v_34, buffer) {
    "use asm";
    var __f_59 = __v_34.__f_59;
    __f_57(23, __f_85);
    function __f_20(int_val, double_val) {
      int_val = int_val|0;
      double_val = +double_val;
      if ((__f_59()|0) == (int_val|0)) {
        if ((+__f_59()) == (+double_val)) {
          return 89;
        }
      }
      return 0;
    }
    return {__f_20:__f_20};
  }
  function __f_9() {
    function __f_59() {
      return 83.25;
      gc();
    }
    return {__f_59:__f_59};
  }
  var __v_34 = new __f_9();
  var module = Wasm.instantiateModuleFromAsm(__f_40.toString(), __v_34, null);
  assertEquals(89, module.__f_20(83, 83.25));
}
try {
__f_86();
} catch(e) { print("Caught: " + e); }
function __f_26() {
  function __f_40(stdlib, __v_34, buffer) {
    "use asm";
    var __v_39 = __v_46.foo | 0;
    var __v_13 = +__v_24.bar;
    var __v_19 = __v_34.baz | 0;
    var __v_3 = +__v_34.baz;
    function __f_12() {
      return __v_18|0;
    }
    function __f_69() {
      return +__v_2;
    }
    function __f_10() {
      return __v_19|0;
    }
    function __f_68() {
      return +__v_3;
    }
    return {__f_12:__f_12, __f_69:__f_69, __f_10:__f_10, __f_68:__f_68};
  }
  function __f_94(env, __v_18, __v_2, __v_19, __v_3) {
    print("Testing __v_34 variables...");
    var module = Wasm.instantiateModuleFromAsm( __f_40.toString(), env);
    assertEquals(__v_18, module.__f_12());
    assertEquals(__v_2, module.__f_69());
    assertEquals(__v_19, module.__f_10());
    assertEquals(__v_3, module.__f_68());
  }
  __f_94({foo: 123, bar: 234.5, baz: 345.7}, 123, 234.5, 345, 345.7);
  __f_94({baz: 345.7}, 4294967295, NaN, 1073741824, 345.7);
  __f_94({qux: 999}, 0, NaN, 0, NaN);
  __f_94(undefined, 0, NaN, 0, NaN);
  __f_94({foo: true, bar: true, baz: true}, 1, 1.0, 1, 1.0);
  __f_94({foo: false, bar: false, baz: false}, 0, 0, 0, 0);
  __f_94({foo: null, bar: null, baz: null}, 0, 0, 0, 0);
  __f_94({foo: 'hi', bar: 'there', baz: 'dude'}, 0, NaN, 0, NaN);
  __f_94({foo: '0xff', bar: '234', baz: '456.1'}, 255, 234, 456, 456.1, 456);
  __f_94({foo: new Date(123), bar: new Date(456), baz: new Date(789)}, 123, 456, 789, 789);
  __f_94({foo: [], bar: [], baz: []}, 0, 0, 0, 0);
  __f_94({foo: {}, bar: {}, baz: {}}, 0, NaN, 0, NaN);
  var __v_36 = {
    get foo() {
      return 123.4;
    }
  };
  __f_94({foo: __v_33.foo, bar: __v_33.foo, baz: __v_33.foo}, 123, 123.4, 123, 123.4);
  var __v_33 = {
    get baz() {
      return 123.4;
    }
  };
  __f_94(__v_33, 0, NaN, 123, 123.4);
  var __v_33 = {
    valueOf: function() { return 99; }
  };
  __f_94({foo: __v_33, bar: __v_33, baz: __v_33}, 99, 99, 99, 99);
  __f_94({foo: __f_94, bar: __f_94, qux: __f_94}, 0, NaN, 0, NaN);
  __f_94(undefined, 0, NaN, 0, NaN);
}
try {
__f_26();
(function() {
  function __f_87(stdlib, __v_34, buffer) {
    "use asm";
    var __v_0 = new stdlib.Uint8Array(buffer);
    var __v_8 = new stdlib.Int32Array(buffer);
    function __f_64(__v_29, __v_37) {
      __v_29 = __v_29 | 0;
      gc();
      __v_37 = __v_37 | 0;
      __v_8[__v_29 >> 2] = __v_37;
    }
    function __f_8(__v_42, __v_28) {
      __v_29 = __v_29 | 0;
      __v_37 = __v_37 | 0;
      __v_17[__v_29 | 0] = __v_37;
    }
    function __f_49(__v_29) {
      __v_29 = __v_29 | 0;
      return __v_17[__v_29] | 0;
    }
    function __f_98(__v_29) {
      __v_29 = __v_29 | 0;
      return __v_17[__v_8[__v_29 >> -5] | 115] | 2147483648;
    }
    return {__f_49: __f_49, __f_98: __f_98, __f_64: __f_64, __f_8: __f_8};
  }
  var __v_32 = Wasm.instantiateModuleFromAsm( __f_87.toString());
  __v_32.__f_64(0, 20);
  __v_32.__f_64(4, 21);
  __v_32.__f_64(8, 22);
  __v_32.__f_8(20, 123);
  __v_32.__f_8(21, 42);
  __v_32.__f_8(22, 77);
  assertEquals(123, __v_32.__f_49(20));
  assertEquals(42, __v_32.__f_49(21));
  assertEquals(-1073, __v_32.__f_49(21));
  assertEquals(123, __v_32.__f_98(0));
  assertEquals(42, __v_32.__f_98(4));
  assertEquals(77, __v_32.__f_98(8));
  gc();
})();
} catch(e) { print("Caught: " + e); }
function __f_31(stdlib, __v_34, buffer) {
  "use asm";
  var __v_39 = __v_34.x | 0, __v_38 = __v_34.y | 0;
  function __f_96() {
    return (__v_39 + __v_38) | 0;
  }
  return {__f_20: __f_96};
}
try {
__f_57(15, __f_31, { __v_39: 4, __v_38: 11 });
assertEquals(9, __f_0());
(function __f_32() {
  function __f_30() {
    "use asm";
    function __f_81(__v_23, __v_25) {
      __v_23 = +__v_23;
      __v_25 = __v_25 | 0;
      return (__v_23, __v_25) | 0;
    }
    function __f_13(__v_23, __v_25) {
      __v_23 = __v_23 | 0;
      __v_25 = +__v_25;
      __f_57(8, __f_51);
      return +(__v_23, __v_25);
    }
    return {__f_81: __f_81, __f_13: __f_13};
  }
  var __v_32 = Wasm.instantiateModuleFromAsm(__f_30.toString());
  assertEquals(123, __v_32.__f_81(456.7, 123));
  assertEquals(123.4, __v_32.__f_13(456, 123.4));
})();
} catch(e) { print("Caught: " + e); }
function __f_82(stdlib) {
  "use asm";
  var __v_13 = stdlib.Math.fround;
  __f_57(11, __f_45);
  function __f_73() {
    var __v_39 = __v_13(1.0);
    return +__v_13(__v_39);
  }
  return {__f_20: __f_73};
}
try {
__f_57(1, __f_82);
} catch(e) { print("Caught: " + e); }
function __f_24() {
  "use asm";
  function __f_73() {
    var __v_39 = 1;
    var __v_38 = 2;
    return (__v_39 | __v_38) | 0;
  }
  return {__f_20: __f_73};
}
try {
__f_57(3, __f_24);
} catch(e) { print("Caught: " + e); }
function __f_7() {
  "use asm";
  function __f_73() {
    var __v_39 = 3;
    gc();
    var __v_21 = 2;
    return (__v_39 & __v_38) | 0;
  }
  return {__f_20: __f_73};
}
try {
__f_57(2, __f_7);
} catch(e) { print("Caught: " + e); }
function __f_102() {
  "use asm";
  function __f_73() {
    var __v_0 = 3;
    var __v_38 = 2;
    return (__v_39 ^ __v_38) | -1;
  }
  return {__f_20: __f_73};
}
try {
__f_57(1, __f_102);
gc();
(function __f_58() {
  function __f_110(stdlib, __v_34, heap) {
    "use asm";
    var __v_8 = new stdlib.Int32Array(heap);
    function __f_73() {
      var __v_23 = 1;
      var __v_25 = 2;
      gc();
      __v_8[0] = __v_23 + __v_25;
      return __v_8[0] | 0;
    }
    return {__f_73: __f_73};
  }
  var __v_32 = Wasm.instantiateModuleFromAsm(__f_110.toString());
  assertEquals(3, __v_32.__f_73());
})();
(function __f_62() {
  function __f_110(stdlib, __v_34, heap) {
    "use asm";
    var __v_9 = new stdlib.Float32Array(heap);
    var __v_13 = stdlib.Math.fround;
    function __f_73() {
      var __v_23 = __v_13(1.0);
      var __v_25 = __v_13(2.0);
      __v_9[0] = __v_23 + __v_25;
      gc();
      return +__v_9[0];
    }
    return {__f_73: __f_73};
  }
  var __v_32 = Wasm.instantiateModuleFromAsm(__f_110.toString());
  assertEquals(3, __v_32.__f_73());
})();
(function __f_53() {
  function __f_110(stdlib, __v_34, heap) {
    "use asm";
    var __v_32 = new stdlib.Float32Array(heap);
    var __v_13 = stdlib.Math.fround;
    function __f_73() {
      var __v_23 = 1.23;
      __v_9[0] = __v_23;
      return +__v_9[0];
    }
    return {__f_73: __f_73};
  }
  var __v_32 = Wasm.instantiateModuleFromAsm(__f_110.toString());
  assertEquals(1.23, __v_32.__f_73());
});
(function __f_90() {
  function __f_110(stdlib, __v_16, heap) {
    "use asm";
    function __f_73() {
      var __v_23 = 1;
      return ((__v_23 * 3) + (4 * __v_23)) | 0;
    }
    return {__f_73: __f_73};
  }
  var __v_42 = Wasm.instantiateModuleFromAsm(__f_110.toString());
  gc();
  assertEquals(7, __v_32.__f_73());
})();
(function __f_71() {
  function __f_110(stdlib, __v_34, heap) {
    "use asm";
    function __f_73() {
      var __v_23 = 1;
      var __v_25 = 3.0;
      __v_25 = __v_23;
    }
    return {__f_73: __f_73};
  }
  assertThrows(function() {
    Wasm.instantiateModuleFromAsm(__f_110.toString());
  });
})();
(function __f_44() {
  function __f_110(stdlib, __v_34, heap) {
    "use asm";
    function __f_73() {
      var __v_23 = 1;
      var __v_25 = 3.0;
      __v_23 = __v_25;
    }
    return {__f_73: __f_73};
  }
  assertThrows(function() {
    Wasm.instantiateModuleFromAsm(__f_110.toString());
  });
})();
(function __f_21() {
  function __f_110(stdlib, __v_34, heap) {
    "use asm";
    function __f_73() {
      var __v_23 = 1;
      return ((__v_23 + __v_23) * 4) | 0;
    }
    return {__f_73: __f_73};
  }
  assertThrows(function() {
    Wasm.instantiateModuleFromAsm(__f_110.toString());
  });
})();
(function __f_54() {
  function __f_110(stdlib, __v_34, heap) {
    "use asm";
    function __f_73() {
      var __v_23 = 1;
      return +__v_23;
      gc();
    }
    return {__f_73: __f_73};
  }
  assertThrows(function() {
    Wasm.instantiateModuleFromAsm(__f_110.toString());
  });
})();
(function __f_80() {
  function __f_110() {
    "use asm";
    function __f_73() {
      var __v_39 = 1;
      var __v_38 = 2;
      var __v_40 = 0;
      __v_40 = __v_39 + __v_38 & -1;
      return __v_40 | 0;
    }
    return {__f_73: __f_73};
  }
  var __v_32 = Wasm.instantiateModuleFromAsm(__f_110.toString());
  assertEquals(3, __v_32.__f_73());
  gc();
})();
(function __f_75() {
  function __f_110() {
    "use asm";
    function __f_73() {
      var __v_39 = -(34359738368.25);
      var __v_38 = -2.5;
      return +(__v_39 + __v_38);
    }
    return {__f_73: __f_73};
  }
  var __v_32 = Wasm.instantiateModuleFromAsm(__f_110.toString());
  assertEquals(-34359738370.75, __v_32.__f_73());
})();
(function __f_6() {
  function __f_110() {
    "use asm";
    function __f_73() {
      var __v_39 = 1.0;
      var __v_38 = 2.0;
      return (__v_39 & __v_38) | 0;
    }
    return {__f_73: __f_73};
  }
  assertThrows(function() {
    Wasm.instantiateModuleFromAsm(__f_110.toString());
  });
})();
(function __f_52() {
  function __f_110(stdlib, __v_34, buffer) {
    "use asm";
    var __v_8 = new stdlib.Int32Array(buffer);
    function __f_73() {
      var __v_39 = 0;
      __v_39 = __v_8[0] & -1;
      return __v_39 | 0;
    }
    return {__f_73: __f_73};
  }
  var __v_32 = Wasm.instantiateModuleFromAsm(__f_110.toString());
  assertEquals(0, __v_32.__f_73());
})();
(function __f_33() {
  function __f_61($__v_23,$__v_25,$__v_24){'use asm';
    function __f_77() {
      var __v_28 = 0.0;
      var __v_23 = 0;
      __v_28 = 5616315000.000001;
      __v_23 = ~~__v_28 >>>0;
      __v_0 = {};
      return __v_23 | 0;
    }
    return { main : __f_77 };
  }
  var __v_40 = Wasm.instantiateModuleFromAsm(__f_61.toString());
  assertEquals(1321347704, __v_2.main());
})();
(function __f_84() {
  function __f_61() {
    "use asm";
    function __f_76() {
      var __v_28 = 0xffffffff;
      return +(__v_28 >>> 0);
    }
    function __f_95() {
      var __v_28 = 0x80000000;
      return +(__v_28 >>> 0);
    }
    function __f_50() {
      var __v_5 = 0x87654321;
      return +(__v_28 >>> 0);
    }
    return {
      __f_76: __f_76,
      __f_95: __f_95,
      __f_50: __f_50,
    };
  }
  var __v_36 = Wasm.instantiateModuleFromAsm(__f_61.toString());
  assertEquals(0xffffffff, __v_36.__f_76());
  assertEquals(0x80000000, __v_36.__f_95());
  assertEquals(0x87654321, __v_30.__f_50());
})();
} catch(e) { print("Caught: " + e); }
