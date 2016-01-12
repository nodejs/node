//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

WScript.LoadScriptFile("..\\UnitTestFramework\\UnitTestFramework.js");

var tests = [
    {
        name: "Exponentiation operator (**) test cases",
        body: function () {
            assert.areEqual(8 , 2 ** 3, "2 to the power of 3 is 8");
            assert.areEqual(Math.pow(-2, -4), -2 ** -4, "Exponentiation operator ** works as intended");
            assert.areEqual(Math.pow(-0, -4), -0 ** -4, "Exponentiation operator ** works as intended");
            assert.areEqual(Math.pow(4, 0), 4 ** -0, "Exponentiation operator ** works as intended");
            assert.areEqual(Math.pow(0, -0), 0 ** -0, "Exponentiation operator ** works as intended");
            assert.areEqual(Math.pow(0, -0), 0 ** -0, "Exponentiation operator ** works as intended");
            assert.areEqual(Math.pow(Infinity, 0), Infinity ** 0,"Exponentiation operator ** works as intended");
            assert.areEqual(Math.pow(Infinity, -Infinity), Infinity ** -Infinity, "Exponentiation operator ** works as intended");
            assert.areEqual(Math.pow(NaN, 2), NaN ** 2, "Exponentiation operator ** works as intended");
            assert.areEqual(Math.pow(3.4, 2.2), 3.4 ** 2.2,"Exponentiation operator ** works as intended");
            assert.areEqual(Math.pow({}, 2.2), ({}) ** 2.2, "Exponentiation operator ** works as intended");
        }
    },
    {
        name: "Right associativity",
        body: function () {
            assert.areEqual( Math.pow(2, Math.pow(3, 2)), 2 ** 3 ** 2, "** is right associative");
        }
    },
    {
        name: "Exponentiation operator assignment",
        body: function () {
            var a = 2;
            a**= 4;
            assert.areEqual(Math.pow(2,4), a, "Exponentiation assignment works as intended");
            var b = -2;
            b **= -4;
            assert.areEqual(Math.pow(-2,-4), b,"Exponentiation assignment works as intended");
        }
    },
    {
        name: "Exponentiation operator must be evaluated before multiplication ",
        body: function () {
            assert.areEqual((2 * 3) ** 2, 2 * 3 ** 2, "Exponentiation operator has lower precedence than multiplicative operator *");
            assert.areEqual(2 + (3 ** 2), 2 + 3 ** 2, "Exponentiation operator has higher precedence than additive operator +");
        }
    },
];

testRunner.runTests(tests, { verbose: WScript.Arguments[0] != "summary" });
