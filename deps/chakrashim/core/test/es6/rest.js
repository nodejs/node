//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

WScript.LoadScriptFile("..\\UnitTestFramework\\UnitTestFramework.js");

var tests = [
  {
    name: "Rest parsing and errors",
    body: function () {
      assert.throws(function () { eval("function foo(...a, ...b) {}")},          SyntaxError,    "More than one rest parameter throws", "The rest parameter must be the last parameter in a formals list.");
      assert.throws(function () { eval("function foo(a, ...b, c) => {}")},       SyntaxError,    "Rest parameter not in the last position throws", "The rest parameter must be the last parameter in a formals list.");
      assert.throws(function () { eval("var obj = class { method(a, b = 1, ...c = [2,3]) {} };")}, SyntaxError, "Rest parameter cannot have a default value");
      assert.throws(function () { eval("function foo(a = b, ...b) {}; foo();")}, ReferenceError, "Rest parameters cannot be referenced in default expressions (use before declaration)", "Use before declaration");

      // Redeclaration errors - non-simple in this case means any parameter list with a rest parameter
      assert.doesNotThrow(function () { eval("function foo(...a) { var a; }"); },
                    "Var redeclaration does not throw with a non-simple parameter list");
      assert.doesNotThrow(function () { eval("function foo(a, ...b) { var a; }"); },
                    "Var redeclaration does not throw with a non-simple parameter list on a non-rest parameter");
      assert.throws(function () { function foo(...a) { eval('var a;'); }; foo(); },
          ReferenceError,
          "Var redeclaration throws with a non-simple parameter list inside an eval",
          "Let/Const redeclaration");
      assert.throws(function () { function foo(a, ...b) { eval('var b;'); }; foo(); },
          ReferenceError,
          "Var redeclaration throws with a non-simple parameter list inside an eval",
          "Let/Const redeclaration");
      assert.throws(function () { function foo(a = 1, ...b) { eval('var b;'); }; foo(); },
          ReferenceError,
          "Var redeclaration throws with a non-simple parameter list inside an eval",
          "Let/Const redeclaration");
      assert.throws(function () { function foo(a, b = 1, ...c) { eval('var c;'); }; foo(); },
          ReferenceError,
          "Var redeclaration throws with a non-simple parameter list inside an eval",
          "Let/Const redeclaration");

      assert.doesNotThrow(function () { function foo(...a) { eval('let a;'); }; foo(); }, "Let redeclaration inside an eval does not throw with a non-simple parameter list");
      assert.doesNotThrow(function () { function foo(...a) { eval('const a = "str";'); }; foo() }, "Const redeclaration inside an eval does not throw with a non-simple parameter list");
      assert.throws(function () { function foo(a, ...b) { eval('var a;'); }; foo(); },
                    ReferenceError,
                    "Var redeclaration throws with a non-simple parameter list on a non-rest parameter inside eval",
                    "Let/Const redeclaration");
      assert.doesNotThrow(function () { function foo(...a) { eval('let a;'); }; foo(); }, "Let redeclaration of a non-default parameter inside an eval does not throw with a non-simple parameter list");
      assert.doesNotThrow(function () { function foo(...a) { eval('const a = 0;'); }; foo(); }, "Const redeclaration of a non-default parameter inside an eval does not throw with a non-simple parameter list");

      assert.doesNotThrow(function () { eval("function foo(a, ...args) { function args() { } }"); }, "Nested function redeclaration of a rest parameter does not throw");

      // Deferred spread/rest errors in lambda formals
      assert.doesNotThrow(function () { (a, b = [...[1,2,3]], ...rest) => {}; },     "Correct spread and rest usage");
      assert.doesNotThrow(function () { (a, b = ([...[1,2,3]]), ...rest) => {}; },   "Correct spread and rest usage with added parens");
      assert.doesNotThrow(function () { (a, b = (([...[1,2,3]])), ...rest) => {}; }, "Correct spread and rest usage with added parens");
      assert.throws(function () { eval("(a = ...NaN, b = [...[1,2,3]], ...rest) => {};"); },
                    SyntaxError,
                    "Invalid spread with valid rest throws on the first invalid spread",
                    "Unexpected ... operator");
      assert.throws(function () { eval("(a = (...NaN), ...b = [...[1,2,3]], ...rest) => {};"); },
                    SyntaxError,
                    "Invalid spread in parens with invalid and valid rest throws on the first invalid spread",
                    "Invalid use of the ... operator. Spread can only be used in call arguments or an array literal.");
      assert.throws(function () { eval("(a = (...NaN), ...b = [...[1,2,3]], rest) => {};"); },
                    SyntaxError,
                    "Invalid spread in parens with invalid rest throws on the first invalid spread",
                    "Invalid use of the ... operator. Spread can only be used in call arguments or an array literal.");
      assert.throws(function () { eval("(a = [...NaN], ...b = [...[1,2,3]], rest) => {};"); },
                    SyntaxError,
                    "Invalid spread (runtime error) with invalid rest throws on the first invalid rest",
                    "Unexpected ... operator");
      assert.throws(function () { eval("(a, ...b, ...rest) => {};"); },
                    SyntaxError,
                    "Invalid rest with valid rest throws on the first invalid rest",
                    "Unexpected ... operator");
      assert.throws(function () { eval("(...rest = ...NaN) => {};"); },
                    SyntaxError,
                    "Invalid rest with invalid spread initializer throws on the invalid rest",
                    "The rest parameter cannot have a default intializer.");

      assert.throws(function () { eval("var x = { set setter(...x) {} }"); },
                    SyntaxError,
                    "Setter methods cannot have a rest parameter",
                    "Unexpected ... operator");
      assert.throws(function () { eval("var x = class { set setter(...x) {} }"); },
                    SyntaxError,
                    "Class setter methods cannot have a rest parameter",
                    "Unexpected ... operator");

      // Default evaluation of 'this' should happen after the rest formal is assigned a register
      assert.doesNotThrow(function () { eval("function foo(a = this, ...b) {}"); }, "'this' referenced in formal defaults should not affect rest parameter");

    }
  },
  {
    name: "Rest basic uses and sanity checks",
    body: function () {
      function foo(a, b, c, ...rest) { return [a, b, c, ...rest]; }

      var bar = (a, b, c, ...rest) => [a, b, c, ...rest];

      class restClass {
        method(a, b, c, ...rest) { return [a, b, c, ...rest]; }
      };
      var baz = new restClass();

      var obj = {
        method(a, b, c, ...rest) { return [a, b, c, ...rest]; },
        evalMethod(a, b, c, ...rest) { return eval("[a, b, c, ...rest]"); }
      };

      var funcObj = new Function("a, b, c, ...rest", "return [a, b, c, ...rest]");

      function singleRest(...rest) { return rest; }

      assert.areEqual([1,2,undefined], foo(1,2),                    "Rest is an empty array with too few parameters to a function");
      assert.areEqual([1,2,3],         foo(1,2,3),                  "Rest is an empty array with the exact number of parameters to a function");
      assert.areEqual([1,2,3,4,5,6],   foo(1,2,3,4,5,6),            "Rest is a non-empty array with too many parameters to a function");

      assert.areEqual([1,2,undefined], bar(1,2),                    "Rest is an empty array with too few parameters to a lambda");
      assert.areEqual([1,2,3],         bar(1,2,3),                  "Rest is an empty array with the exact number of parameters to a lambda");
      assert.areEqual([1,2,3,4,5,6],   bar(1,2,3,4,5,6),            "Rest is a non-empty array with too many parameters to a lambda");

      assert.areEqual([1,2,undefined], baz.method(1,2),             "Rest is an empty array with too few parameters to a class method");
      assert.areEqual([1,2,3],         baz.method(1,2,3),           "Rest is an empty array with the exact number of parameters to a class method");
      assert.areEqual([1,2,3,4,5,6],   baz.method(1,2,3,4,5,6),     "Rest is a non-empty array with too many parameters to a class method");

      assert.areEqual([1,2,undefined], obj.method(1,2),             "Rest is an empty array with too few parameters to a method");
      assert.areEqual([1,2,3],         obj.method(1,2,3),           "Rest is an empty array with the exact number of parameters to a method");
      assert.areEqual([1,2,3,4,5,6],   obj.method(1,2,3,4,5,6),     "Rest is a non-empty array with too many parameters to a method");

      assert.areEqual([1,2,undefined], obj.method(1,2),             "Rest is an empty array with too few parameters to a method with an eval");
      assert.areEqual([1,2,3],         obj.method(1,2,3),           "Rest is an empty array with the exact number of parameters to a method with an eval");
      assert.areEqual([1,2,3,4,5,6],   obj.method(1,2,3,4,5,6),     "Rest is a non-empty array with too many parameters to a method with an eval");

      assert.areEqual([1,2,undefined], funcObj(1,2),                "Rest is an empty array with too few parameters to a function object");
      assert.areEqual([1,2,3],         funcObj(1,2,3),              "Rest is an empty array with the exact number of parameters to a function object");
      assert.areEqual([1,2,3,4,5,6],   funcObj(1,2,3,4,5,6),        "Rest is a non-empty array with too many parameters to a method with a function object");

      // The following takes a different path in the JIT
      assert.areEqual([1,2,3,4,5,6],   singleRest(1,2,3,4,5,6),     "Rest is a non-empty array with any parameters to a function with only a rest parameter");
    }
  },
  {
    name: "Rest basic uses and sanity checks with an arguments reference",
    body: function () {
      function fooArgs(a, b, c, ...rest) { arguments; return [a, b, c, ...rest]; }

      var barArgs = (a, b, c, ...rest) => { arguments; return [a, b, c, ...rest]; }

      class restClass {
        methodArgs(a, b, c, ...rest) { arguments; return [a, b, c, ...rest]; }
      };
      var baz = new restClass();

      var obj = {
        methodArgs(a, b, c, ...rest) { arguments; return [a, b, c, ...rest]; },
        evalMethodArgs(a, b, c, ...rest) { arguments; return eval("[a, b, c, ...rest]"); }
      };

      function testScopeSlots(a, b, c, ...rest) {
        function sub() {
          return [a, b, c, ...rest];
        }
        arguments;
        return sub();
      }

      assert.areEqual([1,2,undefined], fooArgs(1,2),                "Rest is an empty array with too few parameters to a function with a reference to arguments");
      assert.areEqual([1,2,3],         fooArgs(1,2,3),              "Rest is an empty array with the exact number of parameters to a function with a reference to arguments");
      assert.areEqual([1,2,3,4,5,6],   fooArgs(1,2,3,4,5,6),        "Rest is a non-empty array with too many parameters to a function with a reference to arguments");

      assert.areEqual([1,2,undefined], barArgs(1,2),                "Rest is an empty array with too few parameters to a lambda with a reference to arguments");
      assert.areEqual([1,2,3],         barArgs(1,2,3),              "Rest is an empty array with the exact number of parameters to a lambda with a reference to arguments");
      assert.areEqual([1,2,3,4,5,6],   barArgs(1,2,3,4,5,6),        "Rest is a non-empty array with too many parameters to a lambda with a reference to arguments");

      assert.areEqual([1,2,undefined], baz.methodArgs(1,2),         "Rest is an empty array with too few parameters to a class method with a reference to arguments");
      assert.areEqual([1,2,3],         baz.methodArgs(1,2,3),       "Rest is an empty array with the exact number of parameters to a class method with a reference to arguments");
      assert.areEqual([1,2,3,4,5,6],   baz.methodArgs(1,2,3,4,5,6), "Rest is a non-empty array with too many parameters to a class method with a reference to arguments");

      assert.areEqual([1,2,undefined], obj.methodArgs(1,2),         "Rest is an empty array with too few parameters to a method with a reference to arguments");
      assert.areEqual([1,2,3],         obj.methodArgs(1,2,3),       "Rest is an empty array with the exact number of parameters to a method with a reference to arguments");
      assert.areEqual([1,2,3,4,5,6],   obj.methodArgs(1,2,3,4,5,6), "Rest is a non-empty array with too many parameters to a method with a reference to arguments");

      assert.areEqual([1,2,undefined], obj.methodArgs(1,2),         "Rest is an empty array with too few parameters to a method with eval and a reference to arguments");
      assert.areEqual([1,2,3],         obj.methodArgs(1,2,3),       "Rest is an empty array with the exact number of parameters to a method with eval and  a reference to arguments");
      assert.areEqual([1,2,3,4,5,6],   obj.methodArgs(1,2,3,4,5,6), "Rest is a non-empty array with too many parameters to a method with eval and a reference to arguments");

      assert.areEqual([1,2,undefined], testScopeSlots(1,2),         "Rest is an empty array with too few parameters to a function with a reference to arguments using a sub function");
      assert.areEqual([1,2,3],         testScopeSlots(1,2,3),       "Rest is an empty array with the exact number of parameters to a function with a reference to arguments using a sub function");
      assert.areEqual([1,2,3,4,5,6],   testScopeSlots(1,2,3,4,5,6), "Rest is a non-empty array with too many parameters to a function with a reference to arguments using a sub function");
    }
  },
  {
    name: "Rest basic uses and sanity checks with a this reference",
    body: function () {
      function fooArgs(a, b, c, ...rest) { this; return [a, b, c, ...rest]; }

      var barArgs = (a, b, c, ...rest) => { this; return [a, b, c, ...rest]; }

      class restClass {
        methodArgs(a, b, c, ...rest) { this; return [a, b, c, ...rest]; }
      };
      var baz = new restClass();

      var obj = {
        methodArgs(a, b, c, ...rest) { this; return [a, b, c, ...rest]; },
        evalMethodArgs(a, b, c, ...rest) { this; return eval("[a, b, c, ...rest]"); }
      };

      assert.areEqual([1,2,undefined], fooArgs(1,2),                "Rest is an empty array with too few parameters to a function with a reference to this");
      assert.areEqual([1,2,3],         fooArgs(1,2,3),              "Rest is an empty array with the exact number of parameters to a function with a reference to this");
      assert.areEqual([1,2,3,4,5,6],   fooArgs(1,2,3,4,5,6),        "Rest is a non-empty array with too many parameters to a function with a reference to this");

      assert.areEqual([1,2,undefined], barArgs(1,2),                "Rest is an empty array with too few parameters to a lambda with a reference to this");
      assert.areEqual([1,2,3],         barArgs(1,2,3),              "Rest is an empty array with the exact number of parameters to a lambda with a reference to this");
      assert.areEqual([1,2,3,4,5,6],   barArgs(1,2,3,4,5,6),        "Rest is a non-empty array with too many parameters to a lambda with a reference to this");

      assert.areEqual([1,2,undefined], baz.methodArgs(1,2),         "Rest is an empty array with too few parameters to a class method with a reference to this");
      assert.areEqual([1,2,3],         baz.methodArgs(1,2,3),       "Rest is an empty array with the exact number of parameters to a class method with a reference to this");
      assert.areEqual([1,2,3,4,5,6],   baz.methodArgs(1,2,3,4,5,6), "Rest is a non-empty array with too many parameters to a class method with a reference to this");

      assert.areEqual([1,2,undefined], obj.methodArgs(1,2),         "Rest is an empty array with too few parameters to a method with a reference to this");
      assert.areEqual([1,2,3],         obj.methodArgs(1,2,3),       "Rest is an empty array with the exact number of parameters to a method with a reference to this");
      assert.areEqual([1,2,3,4,5,6],   obj.methodArgs(1,2,3,4,5,6), "Rest is a non-empty array with too many parameters to a method with a reference to this");

      assert.areEqual([1,2,undefined], obj.methodArgs(1,2),         "Rest is an empty array with too few parameters to a method with eval and a reference to this");
      assert.areEqual([1,2,3],         obj.methodArgs(1,2,3),       "Rest is an empty array with the exact number of parameters to a method with eval and a reference to this");
      assert.areEqual([1,2,3,4,5,6],   obj.methodArgs(1,2,3,4,5,6), "Rest is a non-empty array with too many parameters to a method with eval and a reference to this");
    }
  },
  {
    name: "Rest basic uses and sanity checks with eval",
    body: function () {
      function fooArgs(a, b, c, ...rest) { eval(""); return [a, b, c, ...rest]; }

      var barArgs = (a, b, c, ...rest) => { eval(""); return [a, b, c, ...rest]; }

      class restClass {
        methodArgs(a, b, c, ...rest) { eval(""); return [a, b, c, ...rest]; }
      };
      var baz = new restClass();

      var obj = {
        methodArgs(a, b, c, ...rest) { eval(""); return [a, b, c, ...rest]; },
        evalMethodArgs(a, b, c, ...rest) { eval(""); return eval("[a, b, c, ...rest]"); }
      };

      assert.areEqual([1,2,undefined], fooArgs(1,2),                "Rest is an empty array with too few parameters to a function with an eval");
      assert.areEqual([1,2,3],         fooArgs(1,2,3),              "Rest is an empty array with the exact number of parameters to a function with an eval");
      assert.areEqual([1,2,3,4,5,6],   fooArgs(1,2,3,4,5,6),        "Rest is a non-empty array with too many parameters to a function with an eval");

      assert.areEqual([1,2,undefined], barArgs(1,2),                "Rest is an empty array with too few parameters to a lambda with an eval");
      assert.areEqual([1,2,3],         barArgs(1,2,3),              "Rest is an empty array with the exact number of parameters to a lambda with an eval");
      assert.areEqual([1,2,3,4,5,6],   barArgs(1,2,3,4,5,6),        "Rest is a non-empty array with too many parameters to a lambda with an eval");

      assert.areEqual([1,2,undefined], baz.methodArgs(1,2),         "Rest is an empty array with too few parameters to a class method with an eval");
      assert.areEqual([1,2,3],         baz.methodArgs(1,2,3),       "Rest is an empty array with the exact number of parameters to a class method with an eval");
      assert.areEqual([1,2,3,4,5,6],   baz.methodArgs(1,2,3,4,5,6), "Rest is a non-empty array with too many parameters to a class method with an eval");

      assert.areEqual([1,2,undefined], obj.methodArgs(1,2),         "Rest is an empty array with too few parameters to a method with an eval");
      assert.areEqual([1,2,3],         obj.methodArgs(1,2,3),       "Rest is an empty array with the exact number of parameters to a method with an eval");
      assert.areEqual([1,2,3,4,5,6],   obj.methodArgs(1,2,3,4,5,6), "Rest is a non-empty array with too many parameters to a method with an eval");

      assert.areEqual([1,2,undefined], obj.methodArgs(1,2),         "Rest is an empty array with too few parameters to a method with eval and an eval");
      assert.areEqual([1,2,3],         obj.methodArgs(1,2,3),       "Rest is an empty array with the exact number of parameters to a method with eval and an eval");
      assert.areEqual([1,2,3,4,5,6],   obj.methodArgs(1,2,3,4,5,6), "Rest is a non-empty array with too many parameters to a method with eval and an eval");
    }
  },
  {
    name: "Rest inlining",
    body: function () {
      function inlineTest() {
        function fooInline(a, b, c, ...rest) { arguments; this; return [a, b, c, ...rest]; }

        fooInline(1,2);
        assert.areEqual([1,2,undefined], fooInline(1,2), "Inlined rest handles less actuals than formals correctly");
        assert.areEqual([1,2,undefined], fooInline(...[1,2]), "Inlined rest handles less spread actuals than formals correctly");

        fooInline(1,2,3);
        assert.areEqual([1,2,3], fooInline(1,2,3), "Inlined rest handles the same amount of actuals and formals correctly");
        assert.areEqual([1,2,3], fooInline(...[1,2,3]), "Inlined rest handles the same amount of spread actuals and formals correctly");

        fooInline(1,2,3,4,5,6);
        assert.areEqual([1,2,3,4,5,6], fooInline(1,2,3,4,5,6), "Inlined rest handles the more actuals than formals correctly");
        assert.areEqual([1,2,3,4,5,6], fooInline(...[1,2,3,4,5,6]), "Inlined rest handles the more actuals than formals correctly");
      }
      inlineTest();
      inlineTest();
      inlineTest();
    }
  },
  {
    name: "OS 264962: Deferred nested function causes an assert",
    body: function () {
      var func4 = function (...argArr13) {
        function foo() {
            eval();
        }
      };
    }
  },
  {
    name: "OS 265363: ArgIn_Rest is emitted in loop bodies",
    body: function () {
      var func4 = function (argArrObj9, ...argArr11) {
        while (false) {
        }
      };
      func4();
    }
  },
  {
    name: "OS 266421: Rest does not create a frame object properly",
    body: function () {
      var func4 = function (...argArr6) {
        for (var _i in arguments) {
        }
      };
    }
  },
  {
    name: "OS 645508: Nested function reference to parent rest parameter fails",
    body: function () {
      function foo(...bar) {
        function child() {
          bar;
        }
        child();
      }
      foo();
    }
  },
  {
    name: "Rest parameter is incorrectly assumed to be in a scope slot",
    body: function () {
      function test0() {
        var func1 = function (...argArr5) {
          arguments[1];
        };
        do {
          func1();
          _oo2obj2.func1();
        } while (false);
      }
    }
  },
  {
    name: "OSG 5737917: Create arguments object when the only formal is a rest argument",
    body: function () {
      var func1 = function (...argArr0) {
        eval('');
        return (Object({
          get: function () {
          }
        }));
      }
    }
  }
];

testRunner.runTests(tests, { verbose: WScript.Arguments[0] != "summary" });

// OS: Bug 269660: [ES6][Rest] ASSERTION 14444: (inetcore\jscript\lib\backend\irbuilder.cpp, line 792) Tried to use an undefined stacksym?
// Serialization bug that needs to be at global scope.
function test0() {
  var func1 = function (...argArr2) {
      if (false) {
          var strvar9 = argArr2;
      }
  };
  func1();
}
test0();
test0();
