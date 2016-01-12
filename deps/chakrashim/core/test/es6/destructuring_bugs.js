//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

WScript.LoadScriptFile("..\\UnitTestFramework\\UnitTestFramework.js");

var tests = [
  {
    name: "Destructuring bug fixes validation",
    body: function () {
      assert.throws(function () { eval("function f1() { var a = 10; [a+2] = []; }; f1();"); }, SyntaxError, "var empty object declaration pattern without an initializer is not valid syntax", "Unexpected operator in destructuring expression");
      assert.throws(function () { eval("function f2() { var a = 10; ({x:a+2} = {x:2}); }; f2();"); }, SyntaxError, "var empty object declaration pattern without an initializer is not valid syntax", "Unexpected operator in destructuring expression");
      assert.throws(function () { eval("function f3() { var a = 10; for ([a+2] in []) { } }; f3();"); }, SyntaxError, "var empty object declaration pattern without an initializer is not valid syntax", "Unexpected operator in destructuring expression");
      assert.doesNotThrow(function () { eval("(function () { var x; for ({x:x}.x of [1,2]) {}; })();"); }, "for..of initializer start with left curly but not a pattern is valid syntax");
      assert.throws(function () { eval("(function () { 'use strict'; [x] = [1]; let x = 2; })();"); }, ReferenceError, "A variable, defined in pattern, is used before its declaration in not valid syntax", "Use before declaration");
      assert.throws(function () { eval("(function () { 'use strict'; let x1 = 1; ({x:x1, y:{y1:y1}} = {x:11, y:{y1:22}}); let y1 = 2; })();"); }, ReferenceError, "A variable, defined in nested pattern, is used before its declaration is not valid syntax", "Use before declaration");
      assert.throws(function () { eval("(function () { 'use strict'; [eval] = []; })();"); }, SyntaxError, "variable name 'eval' defined in array pattern is not valid in strict mode", "Invalid usage of 'eval' in strict mode");
      assert.throws(function () { eval("let a = [a] = [10]"); }, ReferenceError, "A let variable is used in the array pattern in the same statement where it is declared", "Use before declaration");
      assert.throws(function () { eval("let a = {a:a} = {}"); }, ReferenceError, "A let variable is used in object pattern in the same statement where it is declared", "Use before declaration");
      assert.throws(function () { eval("var a = 1; (delete [a] = [2]);"); }, ReferenceError, "Array literal in unary expression should not be converted to array pattern", "Invalid left-hand side in assignment");
    }
  },
  {
    name: "Destructuring bug fix - function expression under destructuring pattern does not crash",
    body: function () {
      var a = [];
      var b = 2;
      function f() {
        [ a [ function () { }, b ] ] = [2] ;
      }
      f();
    }
  }
];

testRunner.runTests(tests, { verbose: WScript.Arguments[0] != "summary" });
