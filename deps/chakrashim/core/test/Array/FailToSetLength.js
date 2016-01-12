//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// Some of the Array.prototype built-in functions set the length property of the array object and
// should throw a TypeError if setting the length property fails. Tests in this file verify that
// we throw TypeError when we're supposed to.
// See BLUE: 559834 for more details

if (this.WScript && this.WScript.LoadScriptFile) { // Check for running in ch
    this.WScript.LoadScriptFile("..\\UnitTestFramework\\UnitTestFramework.js");
}

var tests = [
    {
        name: "Array.prototype built-in functions called with object that has length property with no setter",
        body: function () {
            var obj = { 0: 0, 1: 1, get length() { return 2; }};

            assert.throws(function() { Array.prototype.pop.call(obj); }, TypeError, "Array.prototype.pop throws when obj has a getter for length but is missing a setter", "Cannot define property: object is not extensible");
            assert.throws(function() { Array.prototype.push.call(obj, 2); }, TypeError, "Array.prototype.push throws when obj has a getter for length but is missing a setter", "Cannot define property: object is not extensible");
            assert.throws(function() { Array.prototype.shift.call(obj); }, TypeError, "Array.prototype.shift throws when obj has a getter for length but is missing a setter", "Cannot define property: object is not extensible");
            assert.throws(function() { Array.prototype.unshift.call(obj, 2); }, TypeError, "Array.prototype.unshift throws when obj has a getter for length but is missing a setter", "Cannot define property: object is not extensible");
            assert.throws(function() { Array.prototype.splice.call(obj, 0, 1); }, TypeError, "Array.prototype.splice throws when obj has a getter for length but is missing a setter", "Cannot define property: object is not extensible");
        }
    },
    {
        name: "Array.prototype built-in functions called with object that has length property with no setter and length property has zero value",
        body: function () {
            var obj = { 0: 0, 1: 1, get length() { return 0; }};

            assert.throws(function() { Array.prototype.pop.call(obj); }, TypeError, "Array.prototype.pop throws when obj has a getter for length which returns zero", "Cannot define property: object is not extensible");
            assert.throws(function() { Array.prototype.push.call(obj, 2); }, TypeError, "Array.prototype.push throws when obj has a getter for length which returns zero", "Cannot define property: object is not extensible");
            assert.throws(function() { Array.prototype.shift.call(obj); }, TypeError, "Array.prototype.shift throws when obj has a getter for length which returns zero", "Cannot define property: object is not extensible");
            assert.throws(function() { Array.prototype.unshift.call(obj, 2); }, TypeError, "Array.prototype.unshift throws when obj has a getter for length which returns zero", "Cannot define property: object is not extensible");
            assert.throws(function() { Array.prototype.splice.call(obj, 0, 1); }, TypeError, "Array.prototype.splice throws when obj has a getter for length which returns zero", "Cannot define property: object is not extensible");
        }
    },
    {
        name: "Array.prototype built-in functions called with object that has length property which is non-configurable and non-writable",
        body: function () {
            var obj = { 0: 0, 1: 1 };
            Object.defineProperty(obj, "length", { value: 2, writable: false, configurable: false });

            assert.throws(function() { Array.prototype.pop.call(obj); }, TypeError, "Array.prototype.pop throws when obj has a length property which is not writable and not configurable", "Object doesn't support this action");
            assert.throws(function() { Array.prototype.push.call(obj, 2); }, TypeError, "Array.prototype.push throws when obj has a length property which is not writable and not configurable", "Object doesn't support this action");
            assert.throws(function() { Array.prototype.shift.call(obj); }, TypeError, "Array.prototype.shift throws when obj has a length property which is not writable and not configurable", "Object doesn't support this action");
            assert.throws(function() { Array.prototype.unshift.call(obj, 2); }, TypeError, "Array.prototype.unshift throws when obj has a length property which is not writable and not configurable", "Object doesn't support this action");
            assert.throws(function() { Array.prototype.splice.call(obj, 0, 1); }, TypeError, "Array.prototype.splice throws when obj has a length property which is not writable and not configurable", "Object doesn't support this action");
        }
    },
    {
        name: "Array.prototype built-in functions called with object that has properties with index we need to set in prototype chain and property is an accessor with no setter",
        body: function () {
            var proto = {};
            var obj = {0:1, 1:1, 2:1, 3:-109, length:4};
            obj.__proto__ = proto;
            Object.defineProperty(proto, "4", {configurable: true, get: function() { return 31; }});

            assert.throws(function() { Array.prototype.unshift.call(obj, 200, 201, 202); }, TypeError, "Array.prototype.unshift throws when obj prototype-chain has a property named one of the indices we need to set which is an accessor with no setter", "Cannot define property: object is not extensible");
            assert.throws(function() { Array.prototype.push.call(obj, 200); }, TypeError, "Array.prototype.push throws when obj prototype-chain has a property named one of the indices we need to set which is an accessor with no setter", "Cannot define property: object is not extensible");
        }
    },
];

testRunner.runTests(tests, { verbose: WScript.Arguments[0] != "summary" });
