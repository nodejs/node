//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

if (this.WScript && this.WScript.LoadScriptFile) { // Check for running in ch
    this.WScript.LoadScriptFile("..\\UnitTestFramework\\UnitTestFramework.js");
    this.WScript.LoadScriptFile("util.js");
}

var tests = [
    {
        name: "Uint8ClampedArray constructor exists and has expected properties",
        body: function () {
            assert.areNotEqual(undefined, Uint8ClampedArray, "Uint8ClampedArray constructor exists");
            
            assert.isTrue(Uint8ClampedArray.hasOwnProperty("BYTES_PER_ELEMENT"), "Uint8ClampedArray constructor should have a BYTES_PER_ELEMENT property");
            assert.areEqual(1, Uint8ClampedArray.BYTES_PER_ELEMENT, "Uint8ClampedArray.BYTES_PER_ELEMENT === 8");
            
            var descriptor = Object.getOwnPropertyDescriptor(Uint8ClampedArray, 'BYTES_PER_ELEMENT');
            assert.isFalse(descriptor.writable, "Uint8ClampedArray.BYTES_PER_ELEMENT.descriptor.writable === false");
            assert.isFalse(descriptor.enumerable, "Uint8ClampedArray.BYTES_PER_ELEMENT.descriptor.enumerable === false");
            assert.isFalse(descriptor.configurable, "Uint8ClampedArray.BYTES_PER_ELEMENT.descriptor.configurable === false");
        }
    },
    {
        name: "Uint8ClampedArray prototype has expected fields",
        body: function() {
            var descriptor = Object.getOwnPropertyDescriptor(Uint8ClampedArray, 'prototype');
            assert.isFalse(descriptor.writable, "Uint8ClampedArray.prototype.descriptor.writable === false");
            assert.isFalse(descriptor.enumerable, "Uint8ClampedArray.prototype.descriptor.enumerable === false");
            assert.isFalse(descriptor.configurable, "Uint8ClampedArray.prototype.descriptor.configurable === false");
            
            assert.isTrue(Uint8ClampedArray.prototype.hasOwnProperty("set"), "Uint8ClampedArray prototype should have a set method");
            assert.isTrue(Uint8ClampedArray.prototype.hasOwnProperty("subarray"), "Uint8ClampedArray prototype should have a subarray method");
            
            assert.isTrue(Uint8ClampedArray.prototype.set.length === 2, "set method has length of 2");
            assert.isTrue(Uint8ClampedArray.prototype.subarray.length === 2, "subarray method has length of 2");
        }
    },
    {
        name: "Uint8ClampedArray clamping functionality",
        body: function() {
            var arr = new Uint8ClampedArray(100);
            
            arr[0] = 0;
            arr[1] = 0.0;
            arr[2] = -1;
            arr[3] = -1.0;
            arr[4] = 254;
            arr[5] = 255;
            arr[6] = 256;
            arr[7] = 257;
            arr[8] = 254.0;
            arr[9] = 254.4;
            arr[10] = 254.5;
            arr[11] = 254.6;
            arr[12] = 254.9999;
            arr[13] = 255.0;
            arr[14] = 255.5;
            arr[15] = 256.0;
            arr[16] = 2000;
            arr[17] = 2000.0;
            arr[18] = 99999999999999999999999;
            arr[19] = -99999999999999999999999;
            arr[20] = 1.5;
            arr[21] = 253.5;
            arr[22] = 1.4999;
            arr[23] = 0.33333 * 3;
            arr[24] = 0.33333 * 3 + 0.5;
            arr[25] = -20000.0;
            arr[26] = -20000;
            arr[27] = false;
            arr[28] = true;
            arr[29] = "256.0";
            arr[30] = "-256.0";
            arr[31] = undefined;
            arr[32] = null;
            arr[33] = NaN;
            arr[34] = Number.NEGATIVE_INFINITY;
            arr[35] = (-Number.MAX_VALUE)*2;
            arr[36] = Number.POSITIVE_INFINITY;
            arr[37] = Number.MAX_VALUE*2;
            arr[38] = 0.5;
            arr[39] = 1.5;
            arr[40] = 2.5;
            arr[41] = 3.5;
            arr[42] = 4.5;
            arr[43] = 10.5;
            arr[44] = 11.5;
            
            assert.areEqual(0, arr[0], "clamped(0) === 0");
            assert.areEqual(0, arr[1], "clamped(0.0) === 0");
            assert.areEqual(0, arr[2], "clamped(-1) === 0");
            assert.areEqual(0, arr[3], "clamped(-1.0) === 0");
            assert.areEqual(254, arr[4], "clamped(254) === 254");
            assert.areEqual(255, arr[5], "clamped(255) === 255");
            assert.areEqual(255, arr[6], "clamped(256) === 255");
            assert.areEqual(255, arr[7], "clamped(257) === 255");
            assert.areEqual(254, arr[8], "clamped(254.0) === 254");
            assert.areEqual(254, arr[9], "clamped(254.4) === 254");
            assert.areEqual(254, arr[10], "clamped(254.5) === 254");
            assert.areEqual(255, arr[11], "clamped(254.6) === 255");
            assert.areEqual(255, arr[12], "clamped(254.9999) === 255");
            assert.areEqual(255, arr[13], "clamped(255.0) === 255");
            assert.areEqual(255, arr[14], "clamped(255.5) === 255");
            assert.areEqual(255, arr[15], "clamped(256.0) === 255");
            assert.areEqual(255, arr[16], "clamped(2000) === 255");
            assert.areEqual(255, arr[17], "clamped(2000.0) === 255");
            assert.areEqual(255, arr[18], "clamped(99999999999999999999999) === 255");
            assert.areEqual(0, arr[19], "clamped(-99999999999999999999999) === 0");
            assert.areEqual(2, arr[20], "clamped(1.5) === 2");
            assert.areEqual(254, arr[21], "clamped(253.5) === 254");
            assert.areEqual(1, arr[22], "clamped(1.4999) === 1");
            assert.areEqual(1, arr[23], "clamped(0.33333 * 3) === 1");
            assert.areEqual(1, arr[24], "clamped(0.33333 * 3 + 0.5) === 1");
            assert.areEqual(0, arr[25], "clamped(-20000.0) === 0");
            assert.areEqual(0, arr[26], "clamped(-20000) === 0");
            assert.areEqual(0, arr[27], "clamped(false) === 0");
            assert.areEqual(1, arr[28], "clamped(true) === 1");
            assert.areEqual(255, arr[29], "clamped('256.0') === 255");
            assert.areEqual(0, arr[30], "clamped('-256.0') === 0");
            assert.areEqual(0, arr[31], "clamped(undefined) === 0");
            assert.areEqual(0, arr[32], "clamped(null) === 0");
            assert.areEqual(0, arr[33], "clamped(NaN) === 0");
            assert.areEqual(0, arr[34], "clamped(Number.NEGATIVE_INFINITY) === 0");
            assert.areEqual(0, arr[35], "clamped((-Number.MAX_VALUE)*2) === 0");
            assert.areEqual(255, arr[36], "clamped(Number.POSITIVE_INFINITY) === 255");
            assert.areEqual(255, arr[37], "clamped(Number.MAX_VALUE*2) === 255");
            assert.areEqual(0, arr[38], "clamped(0.5) === 0");
            assert.areEqual(2, arr[39], "clamped(1.5) === 2");
            assert.areEqual(2, arr[40], "clamped(2.5) === 2");
            assert.areEqual(4, arr[41], "clamped(3.5) === 4");
            assert.areEqual(4, arr[42], "clamped(4.5) === 4");
            assert.areEqual(10, arr[43], "clamped(10.5) === 10");
            assert.areEqual(12, arr[44], "clamped(11.5) === 12");
        }
    },
    {
        name: "ToString works correctly for Uint8Clamped array",
        body: function() {
            var arr = new Uint8ClampedArray(100);
            
            assert.areEqual("[object Uint8ClampedArray]", arr.toString(), 'arr.toString == "[object Uint8ClampedArray]"');
        }
    },
    {
        name: "Uint8ClampedArray.subarray functional test",
        body: function() {
            var arr = new Uint8ClampedArray([0,1,2,3,4,5]);
            
            assert.areEqual(arr, arr.subarray(0,arr.length), 'arr.subarray(0,arr.length) == arr');
            assert.areEqual([], arr.subarray(0,0), 'arr.subarray(0,0) == []');
            assert.areEqual([1], arr.subarray(1,2), 'arr.subarray(1,2) == [1]');
            assert.areEqual([], arr.subarray(3,2), 'arr.subarray(3,2) == []');
            assert.areEqual([1,2,3], arr.subarray(1,4), 'arr.subarray(1,4) == [1,2,3]');
            
            assert.throws(function () { arr.subarray(); }, RangeError, "Invalid begin/end value in typed array subarray method", "Invalid begin/end value in typed array subarray method");
        }
    },
    {
        name: "Uint8ClampedArray.set functional test",
        body: function() {
            var arr = new Uint8ClampedArray([0,1,2,3,4,5]);
            
            var dst = new Uint8ClampedArray(10);
            dst.set(arr);
            assert.areEqual([0,1,2,3,4,5,0,0,0,0], dst, 'dst.set(arr) == [0,1,2,3,4,5,0,0,0,0]');
            var dst = new Uint8ClampedArray(10);
            dst.set(arr,0);
            assert.areEqual([0,1,2,3,4,5,0,0,0,0], dst, 'dst.set(arr,0) == [0,1,2,3,4,5,0,0,0,0]');
            var dst = new Uint8ClampedArray(10);
            dst.set(arr,4);
            assert.areEqual([0,0,0,0,0,1,2,3,4,5], dst, 'dst.set(arr,4) == [0,0,0,0,0,1,2,3,4,5]');
            
            assert.throws(function () { arr.set(); }, TypeError, "Invalid source in typed array set", "Invalid source in typed array set");
            assert.throws(function () { new Uint8ClampedArray(3).set(arr); }, TypeError, "Invalid offset/length when creating typed array", "Invalid offset/length when creating typed array");
            assert.throws(function () { new Uint8ClampedArray(3).set(arr,0); }, TypeError, "Invalid offset/length when creating typed array", "Invalid offset/length when creating typed array");
            assert.throws(function () { new Uint8ClampedArray(3).set(arr,10); }, TypeError, "Invalid offset/length when creating typed array", "Invalid offset/length when creating typed array");
            assert.throws(function () { new Uint8ClampedArray(3).set(arr,10); }, TypeError, "Invalid offset/length when creating typed array", "Invalid offset/length when creating typed array");
        }
    }
];

testRunner.runTests(tests);


WScript.Echo("test2");
testSetWithInt(-1, 2, new Uint8ClampedArray(3), new Uint8ClampedArray(3), new Uint8ClampedArray(3));
testSetWithFloat(-1, 2, new Uint8ClampedArray(3), new Uint8ClampedArray(3), new Uint8ClampedArray(3));
testSetWithObj(-1, 2, new Uint8ClampedArray(3), new Uint8ClampedArray(3), new Uint8ClampedArray(3));

WScript.Echo("test2 JIT");
testSetWithInt(-1, 2, new Uint8ClampedArray(3), new Uint8ClampedArray(3), new Uint8ClampedArray(3));
testSetWithFloat(-1, 2, new Uint8ClampedArray(3), new Uint8ClampedArray(3), new Uint8ClampedArray(3));
testSetWithObj(-1, 2, new Uint8ClampedArray(3), new Uint8ClampedArray(3), new Uint8ClampedArray(3));

WScript.Echo("test3");
testIndexValueForSet(new Uint8ClampedArray(5));
WScript.Echo("test3 JIT");
testIndexValueForSet(new Uint8ClampedArray(5));
