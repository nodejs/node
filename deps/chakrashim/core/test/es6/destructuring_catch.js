//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

WScript.LoadScriptFile("..\\UnitTestFramework\\UnitTestFramework.js");

var tests = [
  {
    name: "Basic destructuring syntax as catch param",
    body: function () {
      assert.doesNotThrow(function () { eval("try {} catch({}) {}"); }, "Object destructuring pattern (empty) as a catch param is valid syntax");
      assert.doesNotThrow(function () { eval("try {} catch([]) {}"); }, "Array destructuring pattern (empty) as a catch param  is valid syntax");
      assert.doesNotThrow(function () { eval("try {} catch({x:x}) {}"); }, "Object destructuring pattern as a catch param  is valid syntax");
      assert.doesNotThrow(function () { eval("try {} catch([e]) {}"); }, "Object destructuring pattern as a catch param  is valid syntax");
      assert.doesNotThrow(function () { eval("try {} catch({x}) {}"); }, "Object destructuring pattern (as short-hand) as a catch param  is valid syntax");
      assert.doesNotThrow(function () { eval("function foo() {try {} catch({x, y:[y]}) {} }"); }, "Object destructuring pattern as a catch param inside a function is valid syntax");
      assert.doesNotThrow(function () { eval("function foo() {try {} catch([x, {y:[y]}]) {} }"); }, "Object destructuring pattern as a catch param inside a function is valid syntax");
    }
  },
  {
    name: "Destructuring syntax as catch param - invalid syntax",
    body: function () {
      assert.throws(function () { eval("function foo() {try {} catch({,}) {} }"); }, SyntaxError,  "Object destructuring pattern as a catch param with empty names is not valid syntax", "Expected identifier, string or number");
      assert.throws(function () { eval("function foo() {try {} catch(([])) {} }"); }, SyntaxError,  "Object destructuring pattern as a catch param with empty names is not valid syntax", "Expected identifier");
      assert.throws(function () { eval("function foo() {try {} catch({x:abc+1}) {} }"); }, SyntaxError,  "Object destructuring pattern as a catch param with operator is not valid syntax", "Unexpected operator in destructuring expression");
      assert.throws(function () { eval("function foo() {try {} catch([abc.d]) {} }"); }, SyntaxError,  "Array destructuring pattern as a catch param with property reference is not valid syntax", "Syntax error");
      assert.throws(function () { eval("function foo() {try {} catch([x], [y]) {} }"); }, SyntaxError,  "More than one patterns/identifiers as catch params is not valid syntax", "Expected ')'");
      assert.throws(function () { eval("function foo() {'use strict'; try {} catch([arguments]) {} }"); }, SyntaxError,  "StrictMode - identifier under pattern named as 'arguments' is not valid syntax", "Invalid usage of 'arguments' in strict mode");
      assert.throws(function () { eval("function foo() {'use strict'; try {} catch([eval]) {} }"); }, SyntaxError,  "StrictMode - identifier under pattern named as 'eval' is not valid syntax", "Invalid usage of 'eval' in strict mode");
    }
  },
  {
    name: "Destructuring syntax as params - Initializer",
    body: function () {
      assert.doesNotThrow(function () { eval("function foo() {try {} catch({x:x = 20}) {} }"); },   "Catch param as object destructuring pattern with initializer is valid syntax");
      assert.doesNotThrow(function () { eval("function foo() {try {} catch([x = 20]) {} }"); },   "Catch param as array destructuring pattern with initializer is valid syntax");

      assert.doesNotThrow(function () { eval("function foo() {try {} catch({x1:x1 = 1, x2:x2 = 2, x3:x3 = 3}) {} }"); },   "Catch param as object destructuring pattern has three names with initializer is valid syntax");
      assert.doesNotThrow(function () { eval("function foo() {try {} catch([x1 = 1, x2 = 2, x3 = 3]) {} }"); },   "Catch param as array destructuring pattern has three names with initializer is valid syntax");

      assert.throws(function () { eval("function foo() {try {} catch({x:x} = {x:1}) {} }"); }, SyntaxError,  "Catch param as pattern with default is not valid syntax", "Destructuring declarations cannot have an initializer");
    }
  },
  {
    name: "Destructuring syntax as params - redeclarations",
    body: function () {
      assert.throws(function () { eval("function foo() {try {} catch({x:x, x:x}) {} }"); },  SyntaxError,  "Catch param as object pattern has duplicate binding identifiers is not valid syntax", "Let/Const redeclaration");
      assert.throws(function () { eval("function foo() {try {} catch([x, x]) {} }"); }, SyntaxError,   "Catch param as array pattern has duplicate binding identifiers is not valid syntax", "Let/Const redeclaration");
      assert.throws(function () { eval("function foo() {try {} catch({z1, x:{z:[z1]}}) {} }"); }, SyntaxError,  "Catch param has nesting pattern has has matching is not valid syntax", "Let/Const redeclaration");
      assert.throws(function () { eval("function foo() {try {} catch([x]) { let x = 10;} }"); }, SyntaxError,  "Catch param as a pattern and matching name with let/const variable in body is not valid syntax", "Let/Const redeclaration");
      assert.throws(function () { eval("function foo() {try {} catch([x]) { function x() {} } }"); }, SyntaxError,  "Catch param as a pattern and matching name with function name in body is not valid syntax", "Let/Const redeclaration");
      assert.doesNotThrow(function () { eval("function foo() {try {} catch([x]) { var x = 10;} }"); },  "Catch param as a pattern and matching name with var declared name in body is valid syntax");
    }
  },
  {
    name: "Destructuring on catch param - basic functionality",
    body : function () {
        try {
            throw [1];
        }
        catch ([e1]) {
            assert.areEqual(e1, 1, "Array pattern as a catch param matches with actual exception and initializes the identifier correctly");
        }

        try {
            throw {e2:2};
        }
        catch({e2}) {
            assert.areEqual(e2, 2, "Object pattern as a catch param matches with actual exception and initializes the identifier correctly");
        }

        try {
            throw [3, {e4:[4]}];
        }
        catch([e3, {e4:[e4]}]) {
            assert.areEqual(e3, 3, "First identifier in catch param as pattern is matched and initialized correctly");
            assert.areEqual(e4, 4, "Second identifier in catch param as pattern is matched and initialized correctly");
        }
    }
   },
  {
    name: "Destructuring on catch param - initializer",
    body : function () {
        try {
            throw [];
        }
        catch ([e1 = 11]) {
            assert.areEqual(e1, 11, "Array pattern as a catach param has initializer and initializes with initializer value");
        }

        try {
            throw {};
        }
        catch({e2:e2 = 22}) {
            assert.areEqual(e2, 22, "Object pattern as a catach param has initializer and initializes with initializer value");
        }

        try {
            throw [, {e4:[]}];
        }
        catch([e3 = 11, {e4:[e4 = 22]} = {e4:[]}]) {
            assert.areEqual(e3, 11, "First identifier in catch params as a pattern is initialized with initializer value");
            assert.areEqual(e4, 22, "Second identifier in catch params as a pattern is initialized with initializer value");
        }
    }
   },
  {
    name: "Destructuring on catch param - captures",
    body : function () {
        (function () {
            try {
                throw {x1:'x1', x2:'x2', x3:'x3'};
            }
            catch ({x1, x2, x3}) {
                (function () {
                    x1;x2;x3;
                })();
                let m = x1+x2+x3;
                assert.areEqual(m, 'x1x2x3',  "Inner Function - capturing all identifiers from object pattern in inner function is working correctly");
            }
        })();

        (function () {
            try {
                throw ['y1', 'y2', 'y3'];
            }
            catch ([x1, x2, x3]) {
                (function () {
                    x1;x2;x3;
                })();
                let m = x1+x2+x3;
                assert.areEqual(m, 'y1y2y3',  "Inner Function - capturing all identifiers from array pattern in inner function is working correctly");
            }

        })();

        (function () {
            try {
                throw ['y1', 'y2', 'y3'];
            }
            catch ([x1, x2, x3]) {
                (function () {
                    x2;
                })();
                let m = x1+x2+x3;
                assert.areEqual(m, 'y1y2y3',  "Inner Function - capturing only one identifier from pattern in inner function is working correctly");
            }

        })();

        (function () {
            try {
                throw ['y1', 'y2', 'y3'];
            }
            catch ([x1, x2, x3]) {
                eval('');
                let m = x1+x2+x3;
                assert.areEqual(m, 'y1y2y3',  "Has eval - identifiers from catch params are initialized correctly");
            }

        })();

        (function () {
            try {
                throw ['y1', 'y2', 'y3'];
            }
            catch ([x1, x2, x3]) {
                (function () {
                    x1;x2;x3;
                })();
                eval('');
                let m = x1+x2+x3;
                assert.areEqual(m, 'y1y2y3',  "Has eval and inner function - identifiers from catch params are initialized correctly");
            }
        })();

        (function () {
            try {
                throw ['y1', 'y2', 'y3'];
            }
            catch ([x1, x2, x3]) {
                (function () {
                    eval('');
                    x1;x2;x3;
                })();
                let m = x1+x2+x3;
                assert.areEqual(m, 'y1y2y3',  "Inner function has eval - identifiers from catch params are initialized correctly");
            }
        })();
     }
   }
];

testRunner.runTests(tests, { verbose: WScript.Arguments[0] != "summary" });
