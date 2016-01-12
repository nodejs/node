//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

WScript.LoadScriptFile("..\\UnitTestFramework\\UnitTestFramework.js");

var tests = [
    {
        name: "Trailing comma in function",
        body: function () {
            assert.doesNotThrow(function () { eval("function foo(a,) {}"); }, "Trailing comma in function declaration is a valid syntax");
            assert.doesNotThrow(function () { eval("function foo(a,) {'use strict'; }"); }, "Trailing comma in function declaration under strict mode is a valid syntax");
            assert.throws(function () { eval("function foo(a, ,) {}"); }, SyntaxError, "More than one trailing comma is not valid syntax", "Expected identifier");
            assert.throws(function () { eval("function foo(...a,) {}"); }, SyntaxError, "Trailing comma after rest is not valid syntax", "The rest parameter must be the last parameter in a formals list.");
            assert.throws(function () { eval("function foo(,) {}"); }, SyntaxError, "Trailing comma after missing name is not valid syntax", "Expected identifier");
            function f1(a,b,) {}
            assert.areEqual(f1.length, 2, "Function's length is not affected by a trailing comma");
        }
    },
    {
        name: "Trailing comma in call expression",
        body: function () {
            assert.doesNotThrow(function () { eval("function foo() {}; foo(1, 2,);"); }, "Trailing comma in a user function call is a valid syntax");
            assert.doesNotThrow(function () { eval("Math.min(1, 2, );"); }, "Trailing comma in built-in function call is a valid syntax");
        }
    }
];

testRunner.runTests(tests, { verbose: WScript.Arguments[0] != "summary" });
