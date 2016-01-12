//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

WScript.LoadScriptFile("..\\UnitTestFramework\\UnitTestFramework.js");

var tests = [
  {
    name: "Default argument parsing",
    body: function () {
      // Incomplete expressions
      assert.throws(function () { eval("function foo(a =) { return a; }"); },               SyntaxError, "Incomplete default expression throws in a function",                "Syntax error");
      assert.throws(function () { eval("var x = function(a =) { return a; }"); },           SyntaxError, "Incomplete default expression throws in a function expression",     "Syntax error");
      assert.throws(function () { eval("(a =) => a"); },                                    SyntaxError, "Incomplete default expression throws in a lambda",                  "Syntax error");
      assert.throws(function () { eval("var x = { foo(a =) { return a; } }"); },            SyntaxError, "Incomplete default expression throws in an object method",          "Syntax error");
      assert.throws(function () { eval("var x = class { foo(a =) { return a; } }"); },      SyntaxError, "Incomplete default expression throws in a class method",            "Syntax error");
      assert.throws(function () { eval("var x = { foo: function (a =) { return a; } }"); }, SyntaxError, "Incomplete default expression throws in an object member function", "Syntax error");
      assert.throws(function () { eval("function * foo(a =) { return a; }"); },             SyntaxError, "Incomplete default expression throws in a generator function",      "Syntax error");
      assert.throws(function () { eval("var x = function*(a =) { return a; }"); },          SyntaxError, "Incomplete default expression throws in a generator function",      "Syntax error");
      assert.throws(function () { eval("var x = class { * foo(a =) { return a; } }"); },    SyntaxError, "Incomplete default expression throws in a class generator method",  "Syntax error");

      assert.throws(function () { eval("function foo(a *= 5)"); }, SyntaxError, "Other assignment operators do not work");

      // Redeclaration errors - non-simple in this case means any parameter list with a default expression
      assert.doesNotThrow(function () { eval("function foo(a = 1) { var a; }"); },            "Var redeclaration with a non-simple parameter list");
      assert.doesNotThrow(function () { eval("function foo(a = 1, b) { var b; }"); },         "Var redeclaration does not throw with a non-simple parameter list on a non-default parameter");
      assert.throws(function () { function foo(a = 1) { eval('var a;'); }; foo() },     ReferenceError, "Var redeclaration throws with a non-simple parameter list inside an eval", "Let/Const redeclaration");
      assert.throws(function () { function foo(a = 1, b) { eval('var b;'); }; foo(); }, ReferenceError, "Var redeclaration throws with a non-simple parameter list on a non-default parameter inside eval", "Let/Const redeclaration");

      assert.doesNotThrow(function () { function foo(a = 1) { eval('let a;'); }; foo() },           "Let redeclaration inside an eval does not throw with a non-simple parameter list");
      assert.doesNotThrow(function () { function foo(a = 1) { eval('const a = "str";'); }; foo() }, "Const redeclaration inside an eval does not throw with a non-simple parameter list");
      assert.doesNotThrow(function () { function foo(a = 1) { eval('let a;'); }; foo() },           "Let redeclaration of a non-default parameter inside an eval does not throw with a non-simple parameter list");
      assert.doesNotThrow(function () { function foo(a = 1) { eval('const a = 0;'); }; foo() },     "Const redeclaration of a non-default parameter inside an eval does not throw with a non-simple parameter list");

      assert.throws(function () { eval("x = 3 => x"); },                                    SyntaxError, "Lambda formals without parentheses cannot have default expressions", "Expected \'(\'");
      assert.throws(function () { eval("var a = 0, b = 0; (x = ++a,++b) => x"); },          SyntaxError, "Default expressions cannot have comma separated expressions",        "Expected identifier");

      // Bug 263626: Checking strict formal parameters with defaults should not throw
      function foostrict1(a = 1, b) { "use strict"; }
      function foostrict2(a, b = 1) { "use strict"; }
    }
  },
  {
    name: "Default argument sanity checks and expressions",
    body: function () {
      function foo(a, b = 2, c = 1, d = a + b + c, e) { return d; }
      assert.areEqual(foo(),     foo(undefined, undefined, undefined, undefined, undefined), "Passing undefined has the same behavior as a default expression");
      assert.areEqual(foo(1),    foo(1, 2),    "Passing some arguments leaves default values for the rest");
      assert.areEqual(foo(1, 2), foo(1, 2, 1), "Passing some arguments leaves default values for the rest");
      assert.areEqual(foo(3, 5), foo(3, 5, 1), "Overriding default values leaves other default values in tact");

      function sideEffects(a = 1, b = ++a, c = ++b, d = ++c) { return [a,b,c,d]; }
      assert.areEqual([2,3,4,4], sideEffects(),              "Side effects chain properly");
      assert.areEqual([1,1,1,1], sideEffects(0,undefined,0), "Side effects chain properly with some arguments passed");

      function argsObj1(a = 15, b = arguments[1], c = arguments[0]) { return [a,b,c]; }
      assert.areEqual([15,undefined,undefined], argsObj1(), "Arguments object uses original parameters passed");

      function argsObj2(a, b = 5 * arguments[0]) { return arguments[1]; }
      assert.areEqual(undefined, argsObj2(5), "Arguments object does not return default expression");

      this.val = { test : "test" };
      function thisTest(a = this.val, b = this.val = 1.1, c = this.val, d = this.val2 = 2, e = this.val2) { return [a,b,c,d,e]; }
      assert.areEqual([{test:"test"}, 1.1, 1.1, 2, 2], thisTest(), "'this' is able to be referenced and modified");

      function lambdaCapture() {
        this.propA = 1;
        var lambda = (a = this.propA++) => {
          assert.areEqual(a,          1, "Lambda default parameters use 'this' correctly");
          assert.areEqual(this.propA, 2, "Lambda default parameters using 'this' support side effects");
        };
        lambda();
      }
      lambdaCapture();

      // Function length with and without default
      function length1(a, b, c) {}
      function length2(a, b, c = 1) {}
      function length3(a, b = 1, c = 1) {}
      function length4(a, b = 1, c) {}
      function length5(a = 2, b, c) {}
      function length6(a = 2, b = 5, c = "str") {}

      assert.areEqual(3, length1.length, "No default parameters gives correct length");
      assert.areEqual(2, length2.length, "One trailing default parameter gives correct length");
      assert.areEqual(1, length3.length, "Two trailing default parameters gives correct length");
      assert.areEqual(1, length4.length, "One default parameter with following non-default gives correct length");
      assert.areEqual(0, length5.length, "One default parameter with two following non-defaults gives correct length");
      assert.areEqual(0, length6.length, "All default parameters gives correct length");
    }
  },
  {
    name: "Use before declaration in parameter lists",
    body: function () {
      assert.throws(function () { eval("function foo(a = b, b = a) {}; foo();"); },   ReferenceError, "Unevaluated parameters cannot be referenced in a default initializer", "Use before declaration");
      assert.throws(function () { eval("function foo(a = a, b = b) {}; foo();"); },   ReferenceError, "Unevaluated parameters cannot be referenced in a default initializer", "Use before declaration");
      assert.throws(function () { eval("function foo(a = (b = 5), b) {}; foo();"); }, ReferenceError, "Unevaluated parameters cannot be modified in a default initializer", "Use before declaration");
      assert.throws(function () { eval("function foo(a = eval('b'), b) {}; foo();"); }, ReferenceError, "Future default references using eval are not allowed", "Use before declaration");

      function argsFoo(a = (arguments[1] = 5), b) { return b };
      assert.areEqual(undefined, argsFoo(),     "Unevaluated paramaters are referencable using the arguments object");
      assert.areEqual(undefined, argsFoo(1),    "Side effects on the arguments object are allowed but has no effect on default parameter initialization");
      assert.areEqual(2,         argsFoo(1, 2), "Side effects on the arguments object are allowed but has no effect on default parameter initialization");
    }
  },
  {
    name: "Parameter scope does not have visibility of body declarations",
    body: function () {
      function foo1(a = x) { var x = 1; return a; }
      assert.throws(function() { foo1(); },
                    ReferenceError,
                    "Shadowed var in parameter scope is not affected by body initialization when setting the default value",
                    "'x' is undefined");

      function foo2(a = () => x) { var x = 1; return a(); }
      assert.throws(function () { foo2(); },
                    ReferenceError,
                    "Arrow function capturing var at parameter scope is not affected by body declaration",
                    "'x' is undefined");

      function foo3(a = () => x) { var x = 1; return a; } // a() undefined
      assert.throws(function () { foo3()(); },
                    ReferenceError,
                    "Attempted closure capture of body scoped var throws in an arrow function default expression",
                    "'x' is undefined");

      function foo4(a = function() { return x; }) { var x = 1; return a(); }
      assert.throws(function () { foo4(); },
                    ReferenceError,
                    "Attempted closure capture of body scoped var throws in an anonymous function default expression",
                    "'x' is undefined");

      function foo5(a = function bar() { return 1; }, b = bar()) { return [a(), b]; }
      assert.throws(function () { foo5(); },
                    ReferenceError,
                    "Named function expression does not leak name into subsequent default expressions",
                    "'bar' is undefined");
    }
  },
  {
    name: "Parameter scope shadowing tests",
    body: function () {
      // These tests exercise logic in FindOrAddPidRef for when we need to look at parameter scope

      // Original sym in parameter scope
      var test0 = function(arg1 = _strvar0) {
        for (var _strvar0 in f32) {
            for (var _strvar0 in obj1) {
            }
        }
      }

      // False positive PidRef (no decl) at parameter scope
      function test1() {
        for (var _strvar0 in a) {
            var f = function(b = _strvar0) {
                for (var _strvar0 in c) {}
            };
        }
      }

      function test2() {
        let l = (z) => {
            let  w = { z };
            assert.areEqual(10, w.z, "Identifier reference in object literal should get the correct reference from the arguments");
            var z;
        }
        l(10);
      };
      test2();

    }
  },
  {
    name: "Basic eval in parameter scope",
    body: function () {
      assert.areEqual(1,
                      function (a = eval("1")) { return a; }(),
                      "Eval with static constant works in parameter scope");

      {
        let b = 2;
        assert.areEqual(2,
                        function (a = eval("b")) { return a; }(),
                        "Eval with parent var reference works in parameter scope");
      }

      assert.areEqual(1,
                      function (a, b = eval("arguments[0]")) { return b; }(1),
                      "Eval with arguments reference works in parameter scope");

      function testSelf(a = eval("testSelf(1)")) {
        return a;
      }
      assert.areEqual(1, testSelf(1), "Eval with reference to the current function works in parameter scope");

      var testSelfExpr = function (a = eval("testSelfExpr(1)")) {
        return a;
      }
      assert.areEqual(1, testSelfExpr(), "Eval with reference to the current function expression works in parameter scope");

      {
        let a = 1, b = 2, c = 3;
        function testEvalRef(a = eval("a"), b = eval("b"), c = eval("c")) {
          return [a, b, c];
        }
        assert.throws(function () { testEvalRef(); },
                      ReferenceError,
                      "Eval with reference to the current formal throws",
                      "Use before declaration");

        function testEvalRef2(x = eval("a"), y = eval("b"), z = eval("c")) {
          return [x, y, z];
        }
        assert.areEqual([1, 2, 3], testEvalRef2(), "Eval with references works in parameter scope");
      }
    }
  },
  {
    name: "Eval declarations in parameter scope",
    body: function() {
      // Redeclarations of formals - var
      assert.throws(function () { return function (a = eval("var a = 2"), b = a) { return [a, b]; }() },
                    ReferenceError,
                    "Redeclaring the current formal using var inside an eval throws",
                    "Let/Const redeclaration");
      assert.doesNotThrow(function () { "use strict"; return function (a = eval("var a = 2"), b = a) { return [a, b]; }() },
                          "Redeclaring the current formal using var inside a strict mode eval does not throw");
      assert.doesNotThrow(function () { "use strict"; return function (a = eval("var a = 2"), b = a) { return [a, b]; }() },
                          "Redeclaring the current formal using var inside a strict mode function eval does not throw");

      assert.throws(function () { function foo(a = eval("var b"), b, c = b) { return [a, b, c]; } foo(); },
                    ReferenceError,
                    "Redeclaring a future formal using var inside an eval throws",
                    "Let/Const redeclaration");

      assert.throws(function () { function foo(a, b = eval("var a"), c = a) { return [a, b, c]; } foo(); },
                    ReferenceError,
                    "Redeclaring a previous formal using var inside an eval throws",
                    "Let/Const redeclaration");

      // Let and const do not leak outside of an eval, so the test cases below should never throw.

      // Redeclarations of formals - let
      assert.doesNotThrow(function (a = eval("let a")) { return a; },
                          "Attempting to redeclare the current formal using let inside an eval does not leak");

      assert.doesNotThrow(function (a = eval("let b"), b) { return [a, b]; },
                          "Attempting to redeclare a future formal using let inside an eval does not leak");

      assert.doesNotThrow(function (a, b = eval("let a")) { return [a, b]; },
                          "Attempting to redeclare a previous formal using let inside an eval does not leak");

      // Redeclarations of formals - const
      assert.doesNotThrow(function (a = eval("const a = 1")) { return a; },
                          "Attempting to redeclare the current formal using const inside an eval does not leak");

      assert.doesNotThrow(function (a = eval("const b = 1"), b) { return [a, b]; },
                          "Attempting to redeclare a future formal using const inside an eval does not leak");

      assert.doesNotThrow(function (a, b = eval("const a = 1")) { return [a, b]; },
                          "Attempting to redeclare a previous formal using const inside an eval does not leak");

      // Conditional declarations
      function test(x = eval("var a = 1; let b = 2; const c = 3;")) {
        if (x === undefined) {
          // only a should be visible
          assert.areEqual(1, a, "Var declarations leak out of eval into parameter scope");
        } else {
          // none should be visible
          assert.throws(function () { a }, ReferenceError, "Ignoring the default value does not result in an eval declaration leaking", "'a' is undefined");
        }

        assert.throws(function () { b }, ReferenceError, "Let declarations do not leak out of eval to parameter scope",   "'b' is undefined");
        assert.throws(function () { c }, ReferenceError, "Const declarations do not leak out of eval to parameter scope", "'c' is undefined");
      }
      test();
      test(123);

      // Redefining locals
      function foo(a = eval("var x = 1;")) {
        assert.areEqual(1, x, "Shadowed parameter scope var declaration retains its original value before the body declaration");
        var x = 10;
        assert.areEqual(10, x, "Shadowed parameter scope var declaration uses its new value in the body declaration");
      }

      assert.doesNotThrow(function() { foo(); }, "Redefining a local var with an eval var does not throw");
      assert.throws(function() { return function(a = eval("var x = 1;")) { let x = 2; }(); },   ReferenceError, "Redefining a local let declaration with a parameter scope eval var declaration leak throws",   "Let/Const redeclaration");
      assert.throws(function() { return function(a = eval("var x = 1;")) { const x = 2; }(); }, ReferenceError, "Redefining a local const declaration with a parameter scope eval var declaration leak throws", "Let/Const redeclaration");

      // Function bodies defined in eval
      function funcArrow(a = eval("() => 1"), b = a()) { return [a(), b]; }
      assert.areEqual([1,1], funcArrow(), "Defining an arrow function body inside an eval works at default parameter scope");

      function funcDecl(a = eval("function foo() { return 1; }"), b = foo()) { return [a, b, foo()]; }
      assert.areEqual([undefined, 1, 1], funcDecl(), "Declaring a function inside an eval works at default parameter scope");

      function genFuncDecl(a = eval("function * foo() { return 1; }"), b = foo().next().value) { return [a, b, foo().next().value]; }
      assert.areEqual([undefined, 1, 1], genFuncDecl(), "Declaring a generator function inside an eval works at default parameter scope");

      function funcExpr(a = eval("var f = function () { return 1; }"), b = f()) { return [a, b, f()]; }
      assert.areEqual([undefined, 1, 1], funcExpr(), "Declaring a function inside an eval works at default parameter scope");
    }
  },
  {
    name: "Arrow function bodies in parameter scope",
    body: function () {
      // Nested parameter scopes
      function arrow(a = ((x = 1) => x)()) { return a; }
      assert.areEqual(1, arrow(), "Arrow function with default value works at parameter scope");

      function nestedArrow(a = (b = (x = () => 1)) => 1) { return a; }
      assert.areEqual(1, nestedArrow()(), "Nested arrow function with default value works at parameter scope");
    }
  },
  {
    name: "Split parameter scope",
    body: function () {
      function g() {
        return 3 * 3;
      }

      function f(h = () => eval("g()")) { // cannot combine scopes
        function g() {
          return 2 * 3;
          }
        return h(); // 9
      }
    }
    // TODO(tcare): Re-enable when split scope support is implemented
    //assert.areEqual(9, f(), "Paramater scope remains split");
  },
  {
    name: "OS 1583694: Arguments sym is not set correctly on undo defer",
    body: function () {
      eval();
      var arguments;
    }
  },
  {
    name: "Unmapped arguments - Non simple parameter list",
    body: function () {
        function f1 (x = 10, y = 20, z) {
            x += 2;
            y += 2;
            z += 2;

            assert.areEqual(arguments[0], undefined,  "arguments[0] is not mapped with first formal and did not change when the first formal changed");
            assert.areEqual(arguments[1], undefined,  "arguments[1] is not mapped with second formal and did not change when the second formal changed");
            assert.areEqual(arguments[2], 30,  "arguments[2] is not mapped with third formal and did not change when the third formal changed");

            arguments[0] = 1;
            arguments[1] = 2;
            arguments[2] = 3;

            assert.areEqual(x, 12,  "Changing arguments[0], did not change the first formal");
            assert.areEqual(y, 22,  "Changing arguments[1], did not change the second formal");
            assert.areEqual(z, 32,  "Changing arguments[2], did not change the third formal");
        }
        f1(undefined, undefined, 30);

        function f2 (x = 10, y = 20, z) {
            eval('');
            x += 2;
            y += 2;
            z += 2;

            assert.areEqual(arguments[0], undefined,  "Function has eval - arguments[0] is not mapped with first formal and did not change when the first formal changed");
            assert.areEqual(arguments[1], undefined,  "Function has eval - arguments[1] is not mapped with second formal and did not change when the second formal changed");
            assert.areEqual(arguments[2], 30,  "Function has eval - arguments[2] is not mapped with third formal and did not change when the third formal changed");

            arguments[0] = 1;
            arguments[1] = 2;
            arguments[2] = 3;

            assert.areEqual(x, 12,  "Function has eval - Changing arguments[0], did not change the first formal");
            assert.areEqual(y, 22,  "Function has eval - Changing arguments[1], did not change the second formal");
            assert.areEqual(z, 32,  "Function has eval - Changing arguments[2], did not change the third formal");
        }
        f2(undefined, undefined, 30);

        function f3 (x = 10, y = 20, z) {
            (function () {
                eval('');
            })();
            x += 2;
            y += 2;
            z += 2;

            assert.areEqual(arguments[0], undefined,  "Function's inner function has eval - arguments[0] is not mapped with first formal and did not change when the first formal changed");
            assert.areEqual(arguments[1], undefined,  "Function's inner function has eval - arguments[1] is not mapped with second formal and did not change when the second formal changed");
            assert.areEqual(arguments[2], 30,  "Function's inner function has eval - arguments[2] is not mapped with third formal and did not change when the third formal changed");

            arguments[0] = 1;
            arguments[1] = 2;
            arguments[2] = 3;

            assert.areEqual(x, 12,  "Function's inner function has eval - Changing arguments[0], did not change the first formal");
            assert.areEqual(y, 22,  "Function's inner function has eval - Changing arguments[1], did not change the second formal");
            assert.areEqual(z, 32,  "Function's inner function has eval - Changing arguments[2], did not change the third formal");
        }
        f3(undefined, undefined, 30);

    }
  }
];

testRunner.runTests(tests, { verbose: WScript.Arguments[0] != "summary" });
