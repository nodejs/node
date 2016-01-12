//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// ES6 Number API extensions tests -- verifies the API shape and basic functionality

WScript.LoadScriptFile("..\\UnitTestFramework\\UnitTestFramework.js");

var tests = [
    {
        name: "Number constructor and prototype should have spec defined built-ins with correct lengths",
        body: function () {
            assert.isTrue(Number.hasOwnProperty('EPSILON'), "Number should have a EPSILON value property");
            assert.isTrue(Number.hasOwnProperty('MAX_SAFE_INTEGER'), "Number should have a MAX_SAFE_INTEGER value property");
            assert.isTrue(Number.hasOwnProperty('MIN_SAFE_INTEGER'), "Number should have a MIN_SAFE_INTEGER value property");

            assert.isTrue(Number.hasOwnProperty('parseInt'), "Number should have a parseInt method");
            assert.isTrue(Number.hasOwnProperty('parseFloat'), "Number should have a parseFloat method");
            assert.isTrue(Number.hasOwnProperty('isNaN'), "Number should have a isNaN method");
            assert.isTrue(Number.hasOwnProperty('isFinite'), "Number should have a isFinite method");
            assert.isTrue(Number.hasOwnProperty('isInteger'), "Number should have a isInteger method");
            assert.isTrue(Number.hasOwnProperty('isSafeInteger'), "Number should have a isSafeInteger method");

            assert.isTrue(Number.parseInt.length === 2, "parseInt method takes two arguments");
            assert.isTrue(Number.parseFloat.length === 1, "parseFloat method takes one argument");
            assert.isTrue(Number.isNaN.length === 1, "isNaN method takes one argument");
            assert.isTrue(Number.isFinite.length === 1, "isFinite method takes one argument");
            assert.isTrue(Number.isInteger.length === 1, "isInteger method takes one argument");
            assert.isTrue(Number.isSafeInteger.length === 1, "isSafeInteger method takes one argument");
        }
    },
    {
        name: "EPSILON is approximately 2.220446049250313e-16",
        body: function () {
            var computedEpsilon = (function () {
                var next, result;
                for (next = 1; 1 + next !== 1; next = next / 2) {
                    result = next;
                }
                return result;
            }());

            // Use areEqual because we want this value to be exact. That is, even though it is
            // floating point use a strict equality comparison instead of an epsilon check.
            assert.areEqual(Number.EPSILON, computedEpsilon, "Number.EPSILON matches computed value");
        }
    },
    {
        name: "MAX_SAFE_INTEGER and MIN_SAFE_INTEGER are exactly +/-9007199254740991",
        body: function () {
            assert.areEqual(Number.MAX_SAFE_INTEGER, 9007199254740991, "Number.MAX_SAFE_INTEGER is the largest integer value representable by Number without losing precision, i.e. 9007199254740991");
            assert.areEqual(Number.MIN_SAFE_INTEGER, -9007199254740991, "Number.MIN_SAFE_INTEGER is the loweset integer value representable by Number without losing precision, i.e. -9007199254740991");
        }
    },
    {
        name: "parseInt parses integers in a given string in the given radix -- same as the global object's parseInt",
        body: function () {
            // Just do a small sample of tests since we know the implementation of parseInt is the exact same as the global parseInt function
            assert.areEqual(0, Number.parseInt("0"), "Testing sample: 0");
            assert.areEqual(-1, Number.parseInt("-1"), "Testing sample: -1");
            assert.areEqual(128, Number.parseInt("10000000", 2), "Testing sample: 10000000 base 2");
            assert.areEqual(16, Number.parseInt("g", new String("17")), "Testing sample: g base 17");

            assert.areEqual(parseInt, Number.parseInt, "global parseInt and Number.parseInt are the same function object");
        }
    },
    {
        name: "parseFloat parses floats in a given string -- same as the global object's parseFloat",
        body: function () {
            // Just do a small sample of tests since we know the implementation of parseFloat is the exact same as the global parseFloat function
            assert.areEqual(0, Number.parseFloat("0"), "Testing sample: 0");
            assert.areEqual(-1, Number.parseFloat("-1"), "Testing sample: -1");
            assert.areEqual(3.14159, Number.parseFloat("3.14159"), "Testing sample: 3.14159");

            assert.areEqual(parseFloat, Number.parseFloat, "global parseFloat and Number.parseFloat are the same function object");
        }
    },
    {
        name: "isNaN behaves similar to the global object's isNaN except it does not coerce its argument to Number",
        body: function () {
            assert.isTrue(Number.isNaN(NaN), "Number.isNaN returns true for NaN");
            assert.isTrue(Number.isNaN(0/0), "Number.isNaN returns true for 0/0 which is NaN");
            assert.isFalse(Number.isNaN(123), "Number.isNaN returns false for a finite number, say 123");
            assert.isFalse(Number.isNaN(-3.14159), "Number.isNaN returns false for a finite number, say -3.14159");
            assert.isFalse(Number.isNaN(Infinity), "Number.isNaN returns false for +Infinity");
            assert.isFalse(Number.isNaN(-Infinity), "Number.isNaN returns false for -Infinity");
            assert.isFalse(Number.isNaN(undefined), "Number.isNaN returns false for undefined");
            assert.isFalse(Number.isNaN(null), "Number.isNaN returns false for null");
            assert.isFalse(Number.isNaN(new Number(0)), "Number.isNaN returns false for Number object");
            assert.isFalse(Number.isNaN(new Number(NaN)), "Number.isNaN returns false for Number object whose value is NaN");
            assert.isFalse(Number.isNaN("1234"), "Number.isNaN returns false for a string with value '1234'");
            assert.isFalse(Number.isNaN("NaN"), "Number.isNaN returns false for a string with value 'NaN'");
            assert.isFalse(Number.isNaN("Apple"), "Number.isNaN returns false for a string with value 'Apple'");
        }
    },
    {
        name: "isFinite behaves similar to the global object's isFinite except it does not coerce its argument to Number",
        body: function () {
            assert.isTrue(Number.isFinite(0), "Number.isFinite returns true for 0");
            assert.isTrue(Number.isFinite(123), "Number.isFinite returns true for 123");
            assert.isTrue(Number.isFinite(-3.14159), "Number.isFinite returns true for -3.14159");
            assert.isTrue(Number.isFinite(Number.MAX_SAFE_INTEGER), "Number.isFinite returns true for Number.MAX_SAFE_INTEGER");
            assert.isTrue(Number.isFinite(Number.MIN_SAFE_INTEGER), "Number.isFinite returns true for Number.MIN_SAFE_INTEGER");
            assert.isFalse(Number.isFinite(Infinity), "Number.isFinite returns false for Infinity");
            assert.isFalse(Number.isFinite(-Infinity), "Number.isFinite returns false for -Infinity");
            assert.isFalse(Number.isFinite(NaN), "Number.isFinite returns false for NaN");
            assert.isFalse(Number.isFinite(undefined), "Number.isFinite returns false for undefined");
            assert.isFalse(Number.isFinite(null), "Number.isFinite returns false for null");
            assert.isFalse(Number.isFinite(new Number(0)), "Number.isFinite returns false for Number object with finite value");
            assert.isFalse(Number.isFinite(new Number(Infinity)), "Number.isFinite returns false for Number object with infinite value");
            assert.isFalse(Number.isFinite("1234"), "Number.isFinite returns false for a string with value '1234'");
            assert.isFalse(Number.isFinite("Infinity"), "Number.isFinite returns false for a string with value 'Infinity'");
            assert.isFalse(Number.isFinite("NaN"), "Number.isFinite returns false for a string with value 'NaN'");
            assert.isFalse(Number.isFinite("Apple"), "Number.isFinite returns false for a string with value 'Apple'");
        }
    },
    {
        name: "isInteger returns true if its argument is a number and, after coercion via ToInteger abstract operation, is the same value, false otherwise",
        body: function () {
            assert.isTrue(Number.isInteger(0), "Number.isInteger returns true for 0");
            assert.isTrue(Number.isInteger(-0), "Number.isInteger returns true for -0");
            assert.isTrue(Number.isInteger(1), "Number.isInteger returns true for 1");
            assert.isTrue(Number.isInteger(-1), "Number.isInteger returns true for -1");
            assert.isTrue(Number.isInteger(12345), "Number.isInteger returns true for 12345");
            assert.isTrue(Number.isInteger(Number.MAX_SAFE_INTEGER), "Number.isInteger returns true for Number.MAX_SAFE_INTEGER");
            assert.isTrue(Number.isInteger(Number.MIN_SAFE_INTEGER), "Number.isInteger returns true for Number.MIN_SAFE_INTEGER");
            assert.isTrue(Number.isInteger(Number.MAX_SAFE_INTEGER - 1000), "Number.isInteger returns true for Number.MAX_SAFE_INTEGER - 1000");
            assert.isTrue(Number.isInteger(Number.MAX_SAFE_INTEGER + 1000), "Number.isInteger returns true for Number.MAX_SAFE_INTEGER + 1000");
            assert.isFalse(Number.isInteger(Infinity), "Number.isInteger returns false for Infinity");
            assert.isFalse(Number.isInteger(-Infinity), "Number.isInteger returns false for -Infinity");
            assert.isFalse(Number.isInteger(0.5), "Number.isInteger returns false for 0.5");
            assert.isFalse(Number.isInteger(-0.5), "Number.isInteger returns false for -0.5");
            assert.isFalse(Number.isInteger(3.14159), "Number.isInteger returns false for 3.14159");
            assert.isFalse(Number.isInteger(Number.MAX_SAFE_INTEGER / 2), "Number.isInteger returns false for Number.MAX_SAFE_INTEGER / 2");
            assert.isFalse(Number.isInteger(NaN), "Number.isInteger returns false for NaN");
            assert.isFalse(Number.isInteger(undefined), "Number.isInteger returns false for undefined");
            assert.isFalse(Number.isInteger(null), "Number.isInteger returns false for null");
            assert.isFalse(Number.isInteger("12345"), "Number.isInteger returns false for a string with value '12345'");
            assert.isFalse(Number.isInteger("3.14159"), "Number.isInteger returns false for a string with value '3.14159'");
            assert.isFalse(Number.isInteger("NaN"), "Number.isInteger returns false for a string with value 'NaN'");
            assert.isFalse(Number.isInteger(new Number(125)), "Number.isInteger returns false for a Number object with value 125");
            assert.isFalse(Number.isInteger(new Number(65.536)), "Number.isInteger returns false for Number object with value 65.536");
            assert.isFalse(Number.isInteger(new Number(Infinity)), "Number.isInteger returns false for Number object with value Infinity");
        }
    },
    {
        name: "isSafeInteger returns true if its argument is a number and, after coercion via ToInteger abstract operation, is the same value, false otherwise",
        body: function () {
            assert.isTrue(Number.isSafeInteger(0), "Number.isSafeInteger returns true for 0");
            assert.isTrue(Number.isSafeInteger(-0), "Number.isSafeInteger returns true for -0");
            assert.isTrue(Number.isSafeInteger(1), "Number.isSafeInteger returns true for 1");
            assert.isTrue(Number.isSafeInteger(-1), "Number.isSafeInteger returns true for -1");
            assert.isTrue(Number.isSafeInteger(12345), "Number.isSafeInteger returns true for 12345");
            assert.isTrue(Number.isSafeInteger(Number.MAX_SAFE_INTEGER), "Number.isSafeInteger returns true for Number.MAX_SAFE_INTEGER");
            assert.isTrue(Number.isSafeInteger(Number.MIN_SAFE_INTEGER), "Number.isSafeInteger returns true for Number.MIN_SAFE_INTEGER");
            assert.isTrue(Number.isSafeInteger(Number.MAX_SAFE_INTEGER - 1000), "Number.isSafeInteger returns true for Number.MAX_SAFE_INTEGER - 1000");
            assert.isTrue(Number.isSafeInteger(Number.MIN_SAFE_INTEGER + 1000), "Number.isSafeInteger returns true for Number.MIN_SAFE_INTEGER + 1000");
            assert.isFalse(Number.isSafeInteger(Number.MAX_SAFE_INTEGER + 1000), "Number.isSafeInteger returns false for Number.MAX_SAFE_INTEGER + 1000");
            assert.isFalse(Number.isSafeInteger(Number.MIN_SAFE_INTEGER - 1000), "Number.isSafeInteger returns false for Number.MIN_SAFE_INTEGER - 1000");
            assert.isFalse(Number.isSafeInteger(Infinity), "Number.isSafeInteger returns false for Infinity");
            assert.isFalse(Number.isSafeInteger(-Infinity), "Number.isSafeInteger returns false for -Infinity");
            assert.isFalse(Number.isSafeInteger(0.5), "Number.isSafeInteger returns false for 0.5");
            assert.isFalse(Number.isSafeInteger(-0.5), "Number.isSafeInteger returns false for -0.5");
            assert.isFalse(Number.isSafeInteger(3.14159), "Number.isSafeInteger returns false for 3.14159");
            assert.isFalse(Number.isSafeInteger(Number.MAX_SAFE_INTEGER / 2), "Number.isSafeInteger returns false for Number.MAX_SAFE_INTEGER / 2");
            assert.isFalse(Number.isSafeInteger(NaN), "Number.isSafeInteger returns false for NaN");
            assert.isFalse(Number.isSafeInteger(undefined), "Number.isSafeInteger returns false for undefined");
            assert.isFalse(Number.isSafeInteger(null), "Number.isSafeInteger returns false for null");
            assert.isFalse(Number.isSafeInteger("12345"), "Number.isSafeInteger returns false for a string with value '12345'");
            assert.isFalse(Number.isSafeInteger("3.14159"), "Number.isSafeInteger returns false for a string with value '3.14159'");
            assert.isFalse(Number.isSafeInteger("NaN"), "Number.isSafeInteger returns false for a string with value 'NaN'");
            assert.isFalse(Number.isSafeInteger(new Number(125)), "Number.isSafeInteger returns false for a Number object with value 125");
            assert.isFalse(Number.isSafeInteger(new Number(65.536)), "Number.isSafeInteger returns false for Number object with value 65.536");
            assert.isFalse(Number.isSafeInteger(new Number(Infinity)), "Number.isSafeInteger returns false for Number object with value Infinity");
        }
    },
];

testRunner.runTests(tests, { verbose: WScript.Arguments[0] != "summary" });
