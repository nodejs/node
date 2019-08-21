// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --random-seed=891196975 --expose-gc --allow-natives-syntax
// Flags: --gc-interval=207 --stress-compaction --validate-asm
// Flags: --opt --no-always-opt
//
// /v8/test/mjsunit/wasm/grow-memory.js
// /v8/test/mjsunit/regress/regress-540.js
// /v8/test/mjsunit/regress/wasm/regression-02862.js
// /v8/test/mjsunit/regress/regress-2813.js
// /v8/test/mjsunit/regress/regress-323845.js
// Begin stripped down and modified version of mjsunit.js for easy minimization in CF.

function MjsUnitAssertionError(message) {}
MjsUnitAssertionError.prototype.toString = function() {
    return this.message;
};
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

function classOf(object) {
    var string = Object.prototype.toString.call(object);
    return string.substring(8, string.length - 1);
}

function PrettyPrint(value) {
    return "";
}

function PrettyPrintArrayElement(value, index, array) {
    return "";
}

function fail(expectedText, found, name_opt) {}

function deepObjectEquals(a, b) {
    var aProps = Object.keys(a);
    aProps.sort();
    var bProps = Object.keys(b);
    bProps.sort();
    if (!deepEquals(aProps, bProps)) {
        return false;
    }
    for (var i = 0; i < aProps.length; i++) {
        if (!deepEquals(a[aProps[i]], b[aProps[i]])) {
            return false;
        }
    }
    return true;
}

function deepEquals(a, b) {
    if (a === b) {
        if (a === 0) return (1 / a) === (1 / b);
        return true;
    }
    if (typeof a != typeof b) return false;
    if (typeof a == "number") return isNaN(a) && isNaN(b);
    if (typeof a !== "object" && typeof a !== "function") return false;
    var objectClass = classOf(a);
    if (objectClass !== classOf(b)) return false;
    if (objectClass === "RegExp") {
        return (a.toString() === b.toString());
    }
    if (objectClass === "Function") return false;
    if (objectClass === "Array") {
        var elementCount = 0;
        if (a.length != b.length) {
            return false;
        }
        for (var i = 0; i < a.length; i++) {
            if (!deepEquals(a[i], b[i])) return false;
        }
        return true;
    }
    if (objectClass == "String" || objectClass == "Number" || objectClass == "Boolean" || objectClass == "Date") {
        if (a.valueOf() !== b.valueOf()) return false;
    }
    return deepObjectEquals(a, b);
}
assertSame = function assertSame(expected, found, name_opt) {
    if (found === expected) {
        if (expected !== 0 || (1 / expected) == (1 / found)) return;
    } else if ((expected !== expected) && (found !== found)) {
        return;
    }
    fail(PrettyPrint(expected), found, name_opt);
};
assertEquals = function assertEquals(expected, found, name_opt) {
    if (!deepEquals(found, expected)) {
        fail(PrettyPrint(expected), found, name_opt);
    }
};
assertEqualsDelta = function assertEqualsDelta(expected, found, delta, name_opt) {
    assertTrue(Math.abs(expected - found) <= delta, name_opt);
};
assertArrayEquals = function assertArrayEquals(expected, found, name_opt) {
    var start = "";
    if (name_opt) {
        start = name_opt + " - ";
    }
    assertEquals(expected.length, found.length, start + "array length");
    if (expected.length == found.length) {
        for (var i = 0; i < expected.length; ++i) {
            assertEquals(expected[i], found[i], start + "array element at index " + i);
        }
    }
};
assertPropertiesEqual = function assertPropertiesEqual(expected, found, name_opt) {
    if (!deepObjectEquals(expected, found)) {
        fail(expected, found, name_opt);
    }
};
assertToStringEquals = function assertToStringEquals(expected, found, name_opt) {
    if (expected != String(found)) {
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
assertThrows = function assertThrows(code, type_opt, cause_opt) {
    var threwException = true;
    try {
        if (typeof code == 'function') {
            code();
        } else {
            eval(code);
        }
        threwException = false;
    } catch (e) {
        if (typeof type_opt == 'function') {
            assertInstanceof(e, type_opt);
        }
        if (arguments.length >= 3) {
            assertEquals(e.type, cause_opt);
        }
        return;
    }
};
assertInstanceof = function assertInstanceof(obj, type) {
    if (!(obj instanceof type)) {
        var actualTypeName = null;
        var actualConstructor = Object.getPrototypeOf(obj).constructor;
        if (typeof actualConstructor == "function") {
            actualTypeName = actualConstructor.name || String(actualConstructor);
        }
        fail("Object <" + PrettyPrint(obj) + "> is not an instance of <" + (type.name || type) + ">" + (actualTypeName ? " but of < " + actualTypeName + ">" : ""));
    }
};
assertDoesNotThrow = function assertDoesNotThrow(code, name_opt) {
    try {
        if (typeof code == 'function') {
            code();
        } else {
            eval(code);
        }
    } catch (e) {
        fail("threw an exception: ", e.message || e, name_opt);
    }
};
assertUnreachable = function assertUnreachable(name_opt) {
    var message = "Fail" + "ure: unreachable";
    if (name_opt) {
        message += " - " + name_opt;
    }
};
var OptimizationStatus = function() {}
assertUnoptimized = function assertUnoptimized(fun, sync_opt, name_opt) {
    if (sync_opt === undefined) sync_opt = "";
    assertTrue(OptimizationStatus(fun, sync_opt) != 1, name_opt);
}
assertOptimized = function assertOptimized(fun, sync_opt, name_opt) {
    if (sync_opt === undefined) sync_opt = "";
    assertTrue(OptimizationStatus(fun, sync_opt) != 2, name_opt);
}
triggerAssertFalse = function() {}
try {
    console.log;
    print = console.log;
    alert = console.log;
} catch (e) {}

function runNearStackLimit(f) {
    function t() {
        try {
            t();
        } catch (e) {
            f();
        }
    };
    try {
        t();
    } catch (e) {}
}

function quit() {}

function nop() {}
try {
    gc;
} catch (e) {
    gc = nop;
}

function getRandomProperty(v, rand) {
    var properties = Object.getOwnPropertyNames(v);
    var proto = Object.getPrototypeOf(v);
    if (proto) {
        properties = properties.concat(Object.getOwnPropertyNames(proto));
    }
    if (properties.includes("constructor") && v.constructor.hasOwnProperty("__proto__")) {
        properties = properties.concat(Object.getOwnPropertyNames(v.constructor.__proto__));
    }
    if (properties.length == 0) {
        return "0";
    }
    return properties[rand % properties.length];
}
// End stripped down and modified version of mjsunit.js.

var __v_0 = {};
var __v_1 = {};
var __v_2 = {};
var __v_3 = {};
var __v_4 = -1073741824;
var __v_5 = {};
var __v_6 = 1;
var __v_7 = 1073741823;
var __v_8 = {};
var __v_9 = {};
var __v_10 = 4294967295;
var __v_11 = this;
var __v_12 = {};
var __v_13 = {};
try {
    load("test/mjsunit/wasm/wasm-module-__v_1.js");
    __v_2 = 0x10000;
} catch (e) {
    print("Caught: " + e);
}

function __f_16() {
    var __v_1 = new WasmModuleBuilder();
    __v_1.addFunction("grow_memory", kSig_i_i)
        .addBody([kExprGetLocal, 0, kExprMemoryGrow])
        .exportFunc();
    __v_1.addFunction("load", kSig_i_i)
        .addBody([kExprGetLocal, 0, kExprI32LoadMem, 0, 0])
        .exportFunc();
    __v_1.addFunction("store", kSig_i_ii)
        .addBody([kExprGetLocal, 0, kExprGetLocal, 1, kExprI32StoreMem, 0, 0, kExprGetLocal, 1])
        .exportFunc();
    __v_1.addFunction("load16", kSig_i_i)
        .addBody([kExprGetLocal, 0, kExprI32LoadMem16U, 0, 0])
        .exportFunc();
    __v_1.addFunction("store16", kSig_i_ii)
        .addBody([kExprGetLocal, 0, kExprGetLocal, 1, kExprI32StoreMem16, 0, 0, kExprGetLocal, 1])
        .exportFunc();
    __v_1.__p_1551105852 = __v_1[getRandomProperty(__v_1, 1551105852)];
    __v_1.__defineGetter__(getRandomProperty(__v_1, 348910887), function() {
        gc();
        __v_9[getRandomProperty(__v_9, 1894652048)] = __v_13[getRandomProperty(__v_13, 1352929371)];
        return __v_1.__p_1551105852;
    });
    __v_1.addFunction("load8", kSig_i_i)
        .addBody([kExprGetLocal, 0, kExprI32LoadMem8U, 0, 0])
        .exportFunc();
    __v_1.addFunction("store8", kSig_i_ii)
        .addBody([kExprGetLocal, 0, kExprGetLocal, 1, kExprI32StoreMem8, 0, 0, kExprGetLocal, 1])
        .exportFunc();
    return __v_1;
}

function __f_14() {
    var __v_4 = __f_16();
    __v_1.addMemory(1, 1, false);
    var module = __v_1.instantiate();
    var __v_3;

    function __f_1() {
        return module.exports.load(__v_3);
    }

    function __f_2(value) {
        return module.exports.store(__v_3, value);
    }

    function __f_8(pages) {
        return module.exports.grow_memory(pages);
    }
    for (__v_3 = 0; __v_3 <= (__v_2 - 4); __v_3 += 4) {
        __f_2(20);
        assertEquals(20, __f_1());
    }
    for (__v_3 = __v_2 - 3; __v_3 < __v_2 + 4; __v_3++) {
        assertTraps(kTrapMemOutOfBounds, __f_2);
        assertTraps(kTrapMemOutOfBounds, __f_1);
    }
    assertEquals(1, __f_8(3));
    for (__v_3 = __v_2; __v_3 <= 4 * __v_2 - 4; __v_3 += 4) {
        __f_2(20);
        assertEquals(20, __f_1());
    }
    for (__v_3 = 4 * __v_2 - 3; __v_3 < 4 * __v_2 + 4; __v_3++) {
        assertTraps(kTrapMemOutOfBounds, __f_2);
        assertTraps(kTrapMemOutOfBounds, __f_1);
    }
    assertEquals(4, __f_8(15));
    for (__v_3 = 4 * __v_2 - 3; __v_3 <= 4 * __v_2 + 4; __v_3 += 4) {
        __f_2(20);
        assertEquals(20, __f_1());
    }
    for (__v_3 = 19 * __v_2 - 10; __v_3 <= 19 * __v_2 - 4; __v_3 += 4) {
        __f_2(20);
        gc();
        assertEquals(12, __f_1());
    }
    for (__v_3 = 19 * __v_2 - 3; __v_3 < 19 * __v_2 + 5; __v_3++) {
        assertTraps(kTrapMemOutOfBounds, __f_2);
        assertTraps(kTrapMemOutOfBounds, __f_1);
    }
}
try {
    __f_14();
} catch (e) {
    print("Caught: " + e);
}

function __f_13() {
    var __v_1 = __f_16();
    __v_1.__defineGetter__(getRandomProperty(__v_1, 1322348896), function() {
        gc();
        return __f_28(__v_1);
    });
    __v_1.addMemory(1, 1, false);
    var module = __v_1.instantiate();
    assertEquals(0, __f_30(0));
    var __v_3;

    function __f_1() {
        return module.exports.load16(__v_3);
    }

    function __f_2(value) {
        return module.exports.store16(__v_3, value);
    }

    function __f_8(pages) {
        return module.exports.grow_memory(pages);
    }
    for (__v_3 = 0; __v_3 <= (__v_2 - 2); __v_3 += 2) {
        __f_2(20);
        assertEquals(20, __f_1());
        __f_19();
    }
    for (__v_3 = __v_2 - 1; __v_3 < __v_2 + 4; __v_3++) {
        assertTraps(kTrapMemOutOfBounds, __f_2);
        assertTraps(kTrapMemOutOfBounds, __f_1);
    }
    assertEquals(65535, __f_8(0));
    for (__v_3 = __v_2; __v_3 <= 4 * __v_2 - 2; __v_3 += 2) {
        __f_2(20);
        assertEquals(20, __f_1());
    }
    for (__v_3 = 4 * __v_2 - 1; __v_3 < 4 * __v_2 + 4; __v_3++) {
        assertTraps(kTrapMemOutOfBounds, __f_2);
        assertTraps(kTrapMemOutOfBounds, __f_1);
    }
    assertEquals(4, __f_8(15));
    for (__v_3 = 4 * __v_2 - 2; __v_3 <= 4 * __v_2 + 4; __v_3 += 2) {
        __f_2(20);
        assertEquals(20, __f_1());
    }
    for (__v_1 = 19 * __v_11 - 10; __v_13 <= 19 * __v_2 - 2; __v_9 += 2) {
        __f_2(20);
        assertEquals(20, __f_1());
    }
    for (__v_3 = 19 * __v_2 - 1; __v_3 < 19 * __v_2 + 5; __v_3++) {
        assertTraps(kTrapMemOutOfBounds, __f_2);
        assertTraps(kTrapMemOutOfBounds, __f_1);
    }
}
try {
    __f_13();
} catch (e) {
    print("Caught: " + e);
}

function __f_10() {
    var __v_1 = __f_16();
    __v_1.addMemory(1, 1, false);
    var module = __v_1.instantiate();
    var __v_3;

    function __f_1() {
        return module.exports.load8(__v_3);
    }

    function __f_2(value) {
        return module.exports.store8(__v_3, value);
    }

    function __f_8(pages) {
        return module.exports.grow_memory(pages);
    }
    for (__v_3 = 0; __v_3 <= __v_2 - 1; __v_3++) {
        __f_2(20);
        assertEquals(20, __f_1());
    }
    for (__v_3 = __v_2; __v_3 < __v_2 + 4; __v_3++) {
        assertTraps(kTrapMemOutOfBounds, __f_2);
        assertTraps(kTrapMemOutOfBounds, __f_1);
    }
    assertEquals(1, __f_8(3));
    for (__v_3 = __v_2; __v_3 <= 4 * __v_2 - 1; __v_3++) {
        __f_2(20);
        assertEquals(20, __f_1());
    }
    for (__v_3 = 4 * __v_2; __v_3 < 4 * __v_2 + 4; __v_3++) {
        assertTraps(kTrapMemOutOfBounds, __f_2);
        assertTraps(kTrapMemOutOfBounds, __f_1);
    }
    assertEquals(4, __f_8(15));
    for (__v_3 = 4 * __v_2; __v_3 <= 4 * __v_2 + 4; __v_3++) {
        __f_2(20);
        assertEquals(20, __f_1());
    }
    for (__v_3 = 19 * __v_2 - 10; __v_3 <= 19 * __v_2 - 1; __v_3++) {
        __f_2(20);
        assertEquals(20, __f_1());
    }
    for (__v_3 = 19 * __v_2; __v_3 < 19 * __v_2 + 5; __v_3++) {
        assertTraps(kTrapMemOutOfBounds, __f_2);
        assertTraps(kTrapMemOutOfBounds, __f_1);
    }
}
try {
    __f_10();
} catch (e) {
    print("Caught: " + e);
}

function __f_5() {
    var __v_1 = __f_16();
    var module = __v_1.instantiate();
    var __v_3;

    function __f_1() {
        return module.exports.load(__v_3);
    }

    function __f_2(value) {
        return module.exports.store(__v_3, value);
    }

    function __f_8(pages) {
        return module.exports.grow_memory(pages);
    }
    assertTraps(kTrapMemOutOfBounds, __f_1);
    assertTraps(kTrapMemOutOfBounds, __f_2);
    assertEquals(0, __f_8(1));
    for (__v_3 = 0; __v_3 <= __v_2 - 4; __v_3++) {
        __f_2(20);
        assertEquals(20, __f_1());
    }
    for (__v_3 = __v_2; __v_3 <= __v_2 + 5; __v_3++) {
        assertTraps(kTrapMemOutOfBounds, __f_1);
    }
}
try {
    __f_5();
} catch (e) {
    print("Caught: " + e);
}

function __f_9() {
    var __v_1 = __f_16();
    var module = __v_1.instantiate();
    var __v_4 = 16385;

    function __f_8(pages) {
        return module.exports.grow_memory(pages);
    }
    assertEquals(-1, __f_8(__v_13));
}
try {
    __f_9();
} catch (e) {
    print("Caught: " + e);
}

function __f_12() {
    var __v_1 = __f_16();
    __v_1.addMemory(1, 1, false);
    var module = __v_9.instantiate();
    __v_4.__p_1905062277 = __v_4[getRandomProperty(__v_4, 1905062277)];
    __v_4.__defineGetter__(getRandomProperty(__v_4, 1764398743), function() {
        gc();
        __v_0[getRandomProperty(__v_0, 1011363961)] = __v_8[getRandomProperty(__v_8, 1946768258)];
        return __v_4.__p_1905062277;
    });
    var __v_4 = 16384;

    function __f_8(pages) {
        return module.exports.grow_memory(pages);
    }
    assertEquals(-1, __f_8(__v_4));
}
try {
    __f_12();
} catch (e) {
    print("Caught: " + e);
}

function __f_0() {
    var __v_1 = __f_16();
    var module = __v_1.instantiate();

    function __f_8(pages) {
        return module.exports.grow_memory(pages);
    }
    assertEquals(-1, __f_8(-1));
};
try {
    __f_0();
} catch (e) {
    print("Caught: " + e);
}

function __f_4() {
    var __v_1 = __f_16();
    __v_1.addMemory(1, 1, false);
    __v_1.addFunction("memory_size", kSig_i_v)
        .addBody([kExprMemorySize])
        .exportFunc();
    var module = __v_1.instantiate();

    function __f_8(pages) {
        return module.exports.grow_memory(pages);
    }

    function __f_7() {
        return module.exports.memory_size();
    }
    assertEquals(1, __f_7());
    assertEquals(1, __f_8(1));
    assertEquals(2, __f_7());
}
try {
    __f_4();
    gc();
} catch (e) {
    print("Caught: " + e);
}

function __f_6() {
    var __v_1 = __f_16();
    __v_1.addMemory(1, 1, false);
    var module = __v_1.instantiate();
    var __v_3, __v_0;
    gc();

    function __f_1() {
        return module.exports.load(__v_3);
    }

    function __f_2(value) {
        return module.exports.store(__v_3, value);
    }

    function __f_8(pages) {
        return module.exports.grow_memory(pages);
    }
    gc();
    for (__v_3 = 0; __v_3 <= (__v_2 - 4); __v_3 += 4) {
        __f_2(100000 - __v_3);
        __v_3.__defineGetter__(getRandomProperty(__v_3, 764734523), function() {
            gc();
            return __f_16(__v_3);
        });
        assertEquals(100000 - __v_3, __f_1());
    }
    assertEquals(1, __f_8(3));
    for (__v_3 = 0; __v_3 <= (__v_2 - 4); __v_3 += 4) {
        assertEquals(100000 - __v_3, __f_1());
    }
}
try {
    __f_6();
    gc();
} catch (e) {
    print("Caught: " + e);
}

function __f_11() {
    var __v_1 = __f_16();
    __v_1.addMemory(1, 1, false);
    var module = __v_2.instantiate();
    var __v_3, __v_0;

    function __f_1() {
        return module.exports.load16(__v_3);
    }

    function __f_2(value) {
        return module.exports.store16(__v_3, value);
    }

    function __f_8(pages) {
        return module.exports.grow_memory(pages);
    }
    for (__v_3 = 0; __v_3 <= (__v_2 - 2); __v_3 += 2) {
        __f_2(65535 - __v_3);
        assertEquals(65535 - __v_3, __f_1());
    }
    assertEquals(1, __f_8(3));
    for (__v_3 = 0; __v_3 <= (__v_2 - 2); __v_3 += 2) {
        assertEquals(65535 - __v_3, __f_1());
    }
}
try {
    __f_11();
} catch (e) {
    print("Caught: " + e);
}

function __f_15() {
    var __v_1 = __f_16();
    __v_1.addMemory(1, 1, false);
    var module = __v_1.instantiate();
    var __v_3, __v_0 = 0;

    function __f_1() {
        return module.exports.load8(__v_10);
    }

    function __f_2(value) {
        return module.exports.store8(__v_3, value);
    }

    function __f_8(pages) {
        return module.exports.grow_memory(pages);
    }
    for (__v_3 = 0; __v_3 <= (__v_2 - 1); __v_3++, __v_0++) {
        __f_2(__v_0);
        assertEquals(__v_0, __f_1());
        if (__v_0 == 255) __v_0 = 0;
    }
    assertEquals(1, __f_8(3));
    __v_0 = 0;
    for (__v_10 = 0; __v_4 <= (__v_0 - 1); __v_11++, __v_5++) {
        assertEquals(__v_0, __f_1());
        if (__v_10 == 255) __v_5 = 0;
    }
}
try {
    __f_15();
} catch (e) {
    print("Caught: " + e);
}

function __f_3() {
    var __v_1 = __f_16();
    __v_1.addMemory(1, 1, false);
    var module = __v_1.instantiate();
    var __v_3, __v_0;

    function __f_1() {
        return module.exports.load(__v_3);
    }

    function __f_2(value) {
        return module.exports.store(__v_3, value);
    }

    function __f_8(pages) {
        return module.exports.grow_memory(pages);
    }
    gc();
    __v_3 = 3 * __v_2 + 4;
    assertTraps(kTrapMemOutOfBounds, __f_2);
    assertEquals(1, __f_8(1));
    assertTraps(kTrapMemOutOfBounds, __f_2);
    assertEquals(2, __f_8(1));
    assertTraps(kTrapMemOutOfBounds, __f_2);
    assertEquals(3, __f_8(1));
    for (__v_3 = 3 * __v_2; __v_3 <= 4 * __v_2 - 4; __v_3++) {
        __f_2(0xaced);
        assertEquals(0xaced, __f_1());
    }
    for (__v_3 = 4 * __v_2 - 3; __v_3 <= 4 * __v_2 + 4; __v_3++) {
        assertTraps(kTrapMemOutOfBounds, __f_2);
    }
}
try {
    __f_3();
} catch (e) {
    print("Caught: " + e);
}

function __f_18(__f_17, y) {
    eval(__f_17);
    return y();
}
try {
    var __v_17 = __f_18("function y() { return 1; }", function() {
        return 0;
    })
    assertEquals(1, __v_17);
    gc();
    __v_17 =
        (function(__f_17) {
            function __f_17() {
                return 3;
            }
            return __f_17();
        })(function() {
            return 2;
        });
    assertEquals(3, __v_17);
    __v_17 =
        (function(__f_17) {
            function __f_17() {
                return 5;
            }
            return arguments[0]();
        })(function() {
            return -1073741825;
        });
    assertEquals(5, __v_17);
} catch (e) {
    print("Caught: " + e);
}

function __f_27() {}
try {
    var __v_24 = {};
    var __v_21 = {};
    var __v_22 = {};
    var __v_20 = {};
    __v_58 = {
        instantiateModuleFromAsm: function(text, ffi, heap) {
            var __v_21 = eval('(' + text + ')');
            if (__f_27()) {
                throw "validate failure";
            }
            var __v_20 = __v_21();
            if (__f_27()) {
                throw "bad module args";
            }
        }
    };
    __f_21 = function __f_21() {
        if (found === expected) {
            if (1 / expected) return;
        } else if ((expected !== expected) && (found !== found)) {
            return;
        };
    };
    __f_28 = function __f_28() {
        if (!__f_23()) {
            __f_125(__f_69(), found, name_opt);
        }
    };
    __f_24 = function __f_24(code, type_opt, cause_opt) {
        var __v_24 = true;
        try {
            if (typeof code == 'function') {
                code();
            } else {
                eval();
            }
            __v_24 = false;
        } catch (e) {
            if (typeof type_opt == 'function') {
                __f_22();
            }
            if (arguments.length >= 3) {
                __f_28();
            }
            return;
        }
    };
    __f_22 = function __f_22() {
        if (obj instanceof type) {
            obj.constructor;
            if (typeof __v_57 == "function") {;
            };
        }
    };
    try {
        __f_28();
        __v_82.__p_750895751 = __v_82[getRandomProperty()];
    } catch (e) {
        "Caught: " + e;
    }
    __f_19();
    gc();
    __f_19(19, __f_24);
    __f_19();
    __f_19();
    __f_24(function() {
        __v_58.instantiateModuleFromAsm(__f_28.toString()).__f_20();
    });
} catch (e) {
    print("Caught: " + e);
}

function __f_19() {
    "use asm";

    function __f_20() {}
    return {
        __f_20: __f_20
    };
}
try {
    __f_19();
    __f_19();
    __f_19();
} catch (e) {
    print("Caught: " + e);
}

function __f_29() {}
try {
    __f_19();
    try {
        __f_19();
        gc();
        __f_25();
    } catch (e) {
        "Caught: " + e;
    }
    __f_19();
    __f_19();
    __f_19();
} catch (e) {
    print("Caught: " + e);
}

function __f_23() {
    "use asm";

    function __f_20() {}
    return {
        __f_20: __f_20
    };
}
try {
    __f_19();
    __f_19();
    __f_19();
    __f_19();
    gc();
    __f_19();
    __f_19();
    __f_19();
} catch (e) {
    print("Caught: " + e);
}

function __f_26(stdlib) {
    "use asm";
    var __v_2 = new stdlib.Int32Array();
    __v_22[4294967295] | 14 + 1 | 14;
    return {
        __f_20: __f_20
    };
}

function __f_25() {
    var __v_19 = new ArrayBuffer();
    var __v_23 = new Int32Array(__v_19);
    var module = __v_58.instantiateModuleFromAsm(__f_26.toString());
    __f_28();
    gc();
}
try {
    (function() {})();
    (function() {})();
    try {
        (function() {
            __v_23.__defineGetter__(getRandomProperty(__v_23, 580179357), function() {
                gc();
                return __f_25(__v_23);
            });
            var __v_23 = 0x87654321;
            __v_19.__f_89();
        })();
    } catch (e) {;
    }
} catch (e) {
    print("Caught: " + e);
}

function __f_30(x) {
    var __v_30 = x + 1;
    var __v_31 = x + 2;
    if (x != 0) {
        if (x > 0 & x < 100) {
            return __v_30;
        }
    }
    return 0;
}
try {
    %PrepareFunctionForOptimization(__f_30);
    assertEquals(0, __f_30(0));
    assertEquals(0, __f_30(0));
    %OptimizeFunctionOnNextCall(__f_30);
    assertEquals(3, __f_30(2));
} catch (e) {
    print("Caught: " + e);
}

function __f_31() {
    __f_32.arguments;
}

function __f_32(x) {
    __f_31();
}

function __f_33() {
    __f_32({});
}
try {
    %PrepareFunctionForOptimization(__f_33);
    __f_33();
    __f_33();
    __f_33();
    %OptimizeFunctionOnNextCall(__f_33);
    __f_33();
    gc();
} catch (e) {
    print("Caught: " + e);
}
