//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// ES6 Species Built-In APIs tests -- verifies the shape and basic behavior of the built-in [@@species] property

WScript.LoadScriptFile("..\\UnitTestFramework\\UnitTestFramework.js");

var tests = [
    {
        name: "AV when calling slice on an array when using es6all flag",
        body: function () {
            var c = [];
            c[0] = 1;
            c[4294967294] = 2;
            Object.defineProperty(Array, Symbol.species, {enumerable: false, configurable: true, writable: true});
            assert.areEqual(c, c.slice(0));
        }
    },
    {
        name: "Flag 'isNotPathTypeHandlerOrHasUserDefinedCtor' should propagate in PathTypeHandler chain",
        body: function () {
            var arr = [1,2,3,4,5,6];
            arr.constructor = null;
            arr.x = 1;
            assert.throws(function() { Array.prototype.splice.call(arr, 0, 3); }, TypeError, "TypeError when constructor[Symbol.species] is not constructor", "Function 'constructor[Symbol.species]' is not a constructor");
        }
    },
];

testRunner.runTests(tests, { verbose: WScript.Arguments[0] != "summary" });
