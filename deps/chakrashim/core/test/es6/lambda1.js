//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// ES6 Lambda Arrow Function tests -- verifies basic functionality and parsing

WScript.LoadScriptFile("..\\UnitTestFramework\\UnitTestFramework.js");

var globalObject = this;
var globalVar = 123;

var globalLambdaThis = () => this;
var globalLambdaThisProperty = () => this.globalVar;

var globalLambdaEvalThis = () => eval('this');
var globalLambdaEvalThisProperty = () => eval('this.globalVar');

var globalEvalLambdaThis = eval('() => this');
var globalEvalLambdaThisProperty = eval('() => this.globalVar');

var globalLambdaEvalLambdaThis = () => eval('() => this');
var globalLambdaEvalLambdaThisCall = globalLambdaEvalLambdaThis();
var globalLambdaEvalLambdaThisProperty = () => eval('() => this.globalVar');
var globalLambdaEvalLambdaThisPropertyCall = globalLambdaEvalLambdaThisProperty();

var globalEvalLambdaEvalThis = eval('() => eval("this")');
var globalEvalLambdaEvalThisProperty = eval('() => eval("this.globalVar")');

var globalEvalLambdaThisWithNestedEval = eval('eval(""); () => this');
var globalEvalNestedEvalLambdaThis = eval('eval("() => this")');
var globalEvalNestedEvalLambdaThisWithNestedEval = eval('eval("eval(\'\'); () => this")');

var tests = [
    {
        name: "Some simple basic arrow functions testing variation of the different syntaxes",
        body: function () {
            var empty = () => {};
            var product = (x, y) => { return x * y; };
            var nil = x => { var x; };
            var twox = x => x + x;
            var manyformals = (a, b, c, d, e, f) => { return '' + a + b + c + d + e + f; };
            // These nested forms add coverage for bug BLUE 513592
            var nested = x => () => x;
            var nestedoverride = x => x => x;
            var nestedblock = x => { return () => x; }
            var nestedblockoverride = x => { return x => x; }

            assert.areEqual(undefined, empty(), "empty lambda body returns undefined");
            assert.areEqual(6, product(2, 3), "simple lambda with two formals returns product of the arguments");
            assert.areEqual(undefined, nil(0), "simple block lambda without return statement returns undefined");
            assert.areEqual(2, twox(1), "simple expression lambda with single formal returns double the argument");
            assert.areEqual('123456', manyformals(1, 2, 3, 4, 5, 6), "lambda with more than two formal parameters");
            assert.areEqual('abc', nested('abc')('123'), "expression lambda returning nested lambda that captures outer formal and returns it");
            assert.areEqual('123', nestedoverride('abc')('123'), "expression lambda returning nested lambda that shadows outer formal and returns its own");
            assert.areEqual('abc', nestedblock('abc')('123'), "block lambda returning nested lambda that captures outer formal and returns it");
            assert.areEqual('123', nestedblockoverride('abc')('123'), "block lambda returning nested lambda that shadows outer formal and returns its own");
        }
    },
    {
        name: "Lambdas cannot be used as constructors for new expressions and do not have a .prototype property",
        body: function () {
            var empty = () => {};
            var simple = x => x;
            var multi = (x, y, z) => x + y + z;
            var nested = x => () => x;
            var block = (x, y) => { return x + y; };
            var blocknested = () => { return () => { return this; } };

            assert.throws(function () { new empty(); }, TypeError, "Lambdas cannot be used as constructor: empty", "Function is not a constructor");
            assert.throws(function () { new simple(0); }, TypeError, "Lambdas cannot be used as constructor: simple", "Function is not a constructor");
            assert.throws(function () { new multi(2, 3, 4); }, TypeError, "Lambdas cannot be used as constructor: multi", "Function is not a constructor");
            assert.throws(function () { new nested(''); }, TypeError, "Lambdas cannot be used as constructor: nested", "Function is not a constructor");
            assert.throws(function () { new block(null, 0); }, TypeError, "Lambdas cannot be used as constructor: block", "Function is not a constructor");
            assert.throws(function () { new blocknested(); }, TypeError, "Lambdas cannot be used as constructor: blocknested", "Function is not a constructor");

            assert.isFalse(empty.hasOwnProperty('prototype'), "Lambda function objects do not have a 'prototype' property: empty");
            assert.isFalse(simple.hasOwnProperty('prototype'), "Lambda function objects do not have a 'prototype' property: simple");
            assert.isFalse(multi.hasOwnProperty('prototype'), "Lambda function objects do not have a 'prototype' property: multi");
            assert.isFalse(nested.hasOwnProperty('prototype'), "Lambda function objects do not have a 'prototype' property: nested");
            assert.isFalse(block.hasOwnProperty('prototype'), "Lambda function objects do not have a 'prototype' property: block");
            assert.isFalse(blocknested.hasOwnProperty('prototype'), "Lambda function objects do not have a 'prototype' property: blocknested");
        }
    },
    {
        name: "Quick checks on global lambda this binding",
        body: function () {
            assert.areEqual(globalObject, globalLambdaThis(), "lambda at global scope captures global object this");
            assert.areEqual(globalVar, globalLambdaThisProperty(), "lambda at global scope captures global object for this and accesses global properties");

            var fake = { steal: globalLambdaThis };
            var fake2 = { globalVar: 'abc', steal: globalLambdaThisProperty };

            assert.areNotEqual(fake, fake.steal(), "lambda at global scope doesn't bind this dynamically");
            assert.areNotEqual(fake2.globalVar, fake2.steal(), "lambda at global scope doesn't bind this dynamically, does not access target object's property");
            assert.areEqual(globalObject, fake.steal(), "lambda at global scope is not confused, still has lexical this binding");
            assert.areEqual(globalVar, fake2.steal(), "lambda at global scope is not confused, still has lexical this binding and returns global property");
        }
    },
    {
        name: "Arrow function has lexical 'this'; normal function still has dynamic 'this'",
        body: function () {
            // ''=>'' has only lexical ''this'', no dynamic ''this''
            var obj = {
              method: function () {
                return () => this;
              }
            };
            assert.areEqual(obj, obj.method()(), "''=>'' has only lexical ''this'', no dynamic ''this''");

            var fake = {steal: obj.method()};
            assert.areEqual(obj, fake.steal(), "lambda on different object still has original lexical ''this''");

            // But ''function'' still has dynamic ''this'' of course
            var real = {borrow: obj.method};
            assert.areEqual(real, real.borrow()(), "''function'' still has dynamic ''this''");

            // type of scope (e.g. ActivationObject) should not affect binding of ''this''
            obj = {
                method: function () {
                    eval('');
                    return () => this;
                }
            };

            assert.areEqual(obj, obj.method()(), "[ActivationObject] ''=>'' has only lexical ''this'', no dynamic ''this''");

            fake = {steal: obj.method()};
            assert.areEqual(obj, fake.steal(), "[ActivationObject] lambda on different object still has original lexical ''this''");

            // But ''function'' still has dynamic ''this'' of course
            real = {borrow: obj.method};
            assert.areEqual(real, real.borrow()(), "[ActivationObject] ''function'' still has dynamic ''this''");
        }
    },
    {
        name: "Arrow function that refers to this more than once (Blue bug 538880)",
        body: function () {
            var obj = {
                a: 5,
                b: 10,
                method: function () {
                    return () => this.a + this.b;
                }
            };

            assert.areEqual(15, obj.method()(), "lambda can refer to this more than once");
        }
    },
    {
        name: "Lambdas and closures",
        body: function () {
            var f = (function() {
                var x = 'outer';
                var lambda = () => {
                    assert.areEqual(undefined, x);
                    var x = 'lambda';
                    return function() {
                        assert.areEqual('lambda', x);
                    }
                }
                assert.areEqual('outer', x);
                return lambda;
            })();

            f()();
        }
    },
    {
        name: "Interesting valid and invalid syntax",
        body: function () {
            assert.throws(() => { eval('(var x) => {};'); }, SyntaxError, "var not used in formals declaration", "Syntax error");
            assert.throws(() => { eval('(x, ) => {};'); }, SyntaxError, "missing formal identifier in parameter list", "Syntax error");
            assert.throws(() => { eval('a.x => {};'); }, SyntaxError, "valid expression syntax that is invalid parameter list syntax on lhs of =>", "Syntax error");
            assert.throws(() => { eval('(x, y)[7] => {};'); }, SyntaxError, "valid expression syntax that is invalid parameter list syntax on lhs of =>", "Expected '=>'");
            assert.throws(() => { eval('x() => {};'); }, SyntaxError, "valid expression syntax that is invalid parameter list syntax on lhs of =>", "Syntax error");

            assert.throws(() => { eval('0 || () => 0'); }, SyntaxError, "Arrow function no params is not valid on RHS of ||", "Expected '('");
            assert.throws(() => { eval('0 || x => 0'); }, SyntaxError, "Arrow function naked param is not valid on RHS of ||", "Expected '('");
            assert.throws(() => { eval('0 || (x) => 0'); }, SyntaxError, "Arrow function single param is not valid on RHS of ||", "Expected '('");
            assert.throws(() => { eval('0 || (x,y) => 0'); }, SyntaxError, "Arrow function more than single param is not valid on RHS of ||", "Expected '('");
            assert.throws(() => { eval('!()=>{}'); }, SyntaxError, "Arrow function preceded by UnaryOperator is a SyntaxError", "Expected '('");

            assert.doesNotThrow(function () { eval('function foo() { }; foo(x => x);'); }, "lambda with one formal and nor parentheses parses as argument to function call");
            assert.doesNotThrow(function () { eval('function foo() { }; foo(() => "abc"); foo(() => "abc", 123);'); }, "lambda with no formals parses as argument to function call");
            assert.doesNotThrow(function () { eval('function foo() { }; foo((x, y) => "abc"); foo(b => "abc", 123);'); }, "lambda with multiple formals parses as argument to function call");
            assert.doesNotThrow(function () { eval('({})[x => x]'); }, "lambda parses as argument to element access operator");
            assert.doesNotThrow(function () { eval('() => () => 0'); }, "lambda nested in lambda");
            assert.doesNotThrow(function () { eval('() => x => (a, b, c) => 0'); }, "lambda nested in lambda nested in lambda with parameters");
            assert.doesNotThrow(function () { eval('y => () => (a) => 0'); }, "lambda nested in lambda nested in lambda with other parameters");
            assert.doesNotThrow(() => { eval('function * foo() { yield ()=>{}; }'); }, "Arrow function preceded by yield is not an Error");
        }
    },
    {
        name: "toString on a lambda",
        body: function () {
            assert.areEqual("() => { }",                    (() => { }).toString(),                     "toString() on no formals empty body lambda");
            assert.areEqual("x => { }",                     (x => { }).toString(),                      "toString() on non-parenthesized single formal empty body lambda");
            assert.areEqual("x => x",                       (x => x).toString(),                        "toString() on non-parenthesized single formal trivial expression lambda");
            assert.areEqual("(a, b) => { return a * b; }",  ((a, b) => { return a * b; }).toString(),   "toString() on parenthesized formals non-empty body lambda");
            assert.areEqual("(a, b) => a - 2*b",            ((a, b) => a - 2*b).toString(),             "toString() on parenthesized formals non-trivial expression lambda");
        }
    },
    {
        name: "lambdas do not have an arguments object",
        body: function () {
            var arguments = 'not arguments object';
            assert.areEqual(arguments, (() => arguments)(), "arguments in lambda should bind to outside arguments since it is not given its own arguments binding");

            function f (x, y, z) {
                return () => [this, x, y, z, arguments];
            }

            function g (x, y, z) {
                eval('');
                return () => [this, x, y, z, arguments];
            }

            function verify(f, n) {
                let e = ['_this_', '_x_', '_y_', '_z_', '[object Arguments]'];
                let r = f.call('_this_', '_x_', '_y_', '_z_')();

                assert.areEqual(e.length, r.length, n + ": result array length matches expected array length");
                for (let i = 0; i < e.length; i += 1) {
                    assert.areEqual(e[i], r[i] + '', n + ": this, formals, and arguments object are all captured correctly: " + e[i]);
                }
            }

            // Bug, f doesn't work because lambda capturing arguments doesn't cause parser to realize that
            // enclosing non-lambda function has reference to arguments object, so it doesn't load it.
            verify(f, 'f');
            verify(g, 'g');

            function h () {
                return () => {
                    assert.areEqual(5, arguments.length, "captured arguments length is respected");
                    assert.areEqual(h, arguments.callee, "arguments caller is respected");
                    assert.areEqual(1, arguments[0], "first argument is 1");
                    assert.areEqual(2, arguments[1], "second argument is 2");
                    assert.areEqual(3, arguments[2], "third argument is 3");
                    assert.areEqual('abc', arguments[3], "fourth argument is 'abc'");
                    assert.areEqual(null, arguments[4], "fifth argument is null");
                };
            }
            h(1, 2, 3, 'abc', null)();

            function i () {
                return () => () => { return () => arguments; };
            }
            var args = i('a', 'b', 'c', 123, undefined)()()();

            assert.areEqual(5, args.length, "captured arguments (through multiple lambdas) length is respected");
            assert.areEqual(i, args.callee, "arguments (through multiple lambdas) caller is respected");
            assert.areEqual('a', args[0], "first argument (through multiple lambdas) is 'a'");
            assert.areEqual('b', args[1], "second argument (through multiple lambdas) is 'b'");
            assert.areEqual('c', args[2], "third argument (through multiple lambdas) is 'c'");
            assert.areEqual(123, args[3], "fourth argument (through multiple lambdas) is 123");
            assert.areEqual(undefined, args[4], "fifth argument (through multiple lambdas) is undefined");

            function j () {
                var arguments = 'not an arguments object';
                return () => arguments;
            }

            assert.areEqual('not an arguments object', j()(), "lambda captures local variables named arguments");

            function k (arguments) {
                return () => arguments;
            }

            assert.areEqual('foo', k('foo')(), "lambda captures formal parameters named arguments");
        }
    },
    {
        name: "Capturing dynamically scoped variables",
        body: function () {
            eval("var a = 10;");
            var lambda1 = () => a;

            assert.areEqual(10, lambda1(), "lambda can capture dynamically scoped variables introduced by eval");

            var lambdaobj1 = {
                method: function () {
                    return () => {
                        a;
                        return this;
                    };
                }
            };

            assert.areEqual(lambdaobj1, lambdaobj1.method()(), "lambda's lexical ''this'' binding is unaffected by capturing a dyanamically scoped variable introduced by eval");


            var obj = { b: 20 };

            with (obj) {
                let lambda2 = () => b;
                assert.areEqual(20, lambda2(), "lambda can capture properties introduced by with blocks");

                var lambdaobj2 = {
                    method: function () {
                        return () => {
                            b;
                            return this;
                        };
                    }
                };

                assert.areEqual(lambdaobj2, lambdaobj2.method()(), "lambda's lexical ''this'' binding is unaffected by capturing a property introduced by a with block");
            }
        }
    },
    {
        name: "Formal parameter binding and strictness",
        body: function () {
            assert.throws(function () { eval('(eval) => { }'); }, SyntaxError, "'eval' is not allowed as a lambda formal parameter name", "Invalid usage of 'eval' in strict mode");
            assert.throws(function () { eval('(arguments) => { }'); }, SyntaxError, "'arguments' is not allowed as a lambda formal parameter name", "Invalid usage of 'arguments' in strict mode");
            assert.throws(function () { eval('(x, x) => { }'); }, SyntaxError, "duplicate names are not allowed in a lambda formal parameter list", "Duplicate formal parameter names not allowed in strict mode");
            assert.throws(function () { eval('x => { let x; }'); }, SyntaxError, "lambda formal parameters cause redeclaration error with function scoped let bindings of the same name", "Let/Const redeclaration");

            assert.areEqual(10, (function (x) { return x => x; })(20)(10), "lambda formal parameters should shadow outside local variables of the same name [parameter]");
            assert.areEqual(10, (function () { var x = 20; return x => x; })()(10), "lambda formal parameters should shadow outside local variables of the same name [var]");
            assert.areEqual(10, (function () { let x = 20; return x => x; })()(10), "lambda formal parameters should shadow outside local variables of the same name [let]");
            assert.areEqual(10, (function () { const x = 20; return x => x; })()(10), "lambda formal parameters should shadow outside local variables of the same name [const]");
        }
    },
    {
        name: "Lexical this and eval",
        body: function () {
            var thisVal = { localVar: 456 };

            function test () {
                var localLambdaEvalThis = () => eval('this');
                var localLambdaEvalThisProperty = () => eval('this.localVar');

                var localEvalLambdaThis = eval('() => this');
                var localEvalLambdaThisProperty = eval('() => this.localVar');

                var localLambdaEvalLambdaThis = () => eval('() => this');
                var localLambdaEvalLambdaThisCall = localLambdaEvalLambdaThis();
                var localLambdaEvalLambdaThisProperty = () => eval('() => this.localVar');
                var localLambdaEvalLambdaThisPropertyCall = localLambdaEvalLambdaThisProperty();

                var localEvalLambdaEvalThis = eval('() => eval("this")');
                var localEvalLambdaEvalThisProperty = eval('() => eval("this.localVar")');

                var localEvalLambdaNested3This = eval('() => () => () => this');
                var localLambdaNested3EvalThis = () => () => () => eval('this');

                var localEvalLambdaThisWithNestedEval = eval('eval(""); () => this');
                var localEvalNestedEvalLambdaThis = eval('eval("() => this")');
                var localEvalNestedEvalLambdaThisWithNestedEval = eval('eval("eval(\'\'); () => this")');

                assert.areEqual(this, localLambdaEvalThis(), "localLambdaEvalThis() should equal test()'s this value");
                assert.areEqual(this.localVar, localLambdaEvalThisProperty(), "localLambdaEvalThisProperty() should equal the localVar property");

                assert.areEqual(this, localEvalLambdaThis(), "localEvalLambdaThis() should equal test()'s this value");
                assert.areEqual(this.localVar, localEvalLambdaThisProperty(), "localEvalLambdaThisProperty() should equal the localVar property");

                assert.areEqual(this, localLambdaEvalLambdaThis()(), "localLambdaEvalLambdaThis()() should equal test()'s this value");
                assert.areEqual(this, localLambdaEvalLambdaThisCall(), "localLambdaEvalLambdaThisCall() should equal test()'s this value");
                assert.areEqual(this.localVar, localLambdaEvalLambdaThisProperty()(), "localLambdaEvalLambdaThisProperty()() should equal the localVar property");
                assert.areEqual(this.localVar, localLambdaEvalLambdaThisPropertyCall(), "localLambdaEvalLambdaThisPropertyCall() should equal the localVar property");

                assert.areEqual(this, localEvalLambdaEvalThis(), "localEvalLambdaEvalThis() should equal test()'s this value");
                assert.areEqual(this.localVar, localEvalLambdaEvalThisProperty(), "localEvalLambdaEvalThisProperty() should equal the localVar property");

                assert.areEqual(this, localEvalLambdaNested3This()()(), "localEvalLambdaNested3This()()() should equal test()'s this value");
                assert.areEqual(this, localLambdaNested3EvalThis()()(), "localLambdaNested3EvalThis()()() should equal test()'s this value");

                assert.areEqual(this, localEvalLambdaThisWithNestedEval(), "localEvalLambdaThisWithNestedEval() should equal test()'s this value");
                assert.areEqual(this, localEvalNestedEvalLambdaThis(), "localEvalNestedEvalLambdaThis() should equal test()'s this value");
                assert.areEqual(this, localEvalNestedEvalLambdaThisWithNestedEval(), "localEvalNestedEvalLambdaThisWithNestedEval() should equal test()'s this value");


                // Global equivalents
                assert.areEqual(globalObject, globalLambdaEvalThis(), "globalLambdaEvalThis() should equal the global object");
                assert.areEqual(globalVar, globalLambdaEvalThisProperty(), "globalLambdaEvalThisProperty() should equal the global object's globalVar property");

                assert.areEqual(globalObject, globalEvalLambdaThis(), "globalEvalLambdaThis() should equal the global object");
                assert.areEqual(globalVar, globalEvalLambdaThisProperty(), "globalEvalLambdaThisProperty() should equal the global object's globalVar property");

                assert.areEqual(globalObject, globalLambdaEvalLambdaThis()(), "globalLambdaEvalLambdaThis()() should equal the global object");
                assert.areEqual(globalObject, globalLambdaEvalLambdaThisCall(), "globalLambdaEvalLambdaThisCall() should equal the global object");
                assert.areEqual(globalVar, globalLambdaEvalLambdaThisProperty()(), "globalLambdaEvalLambdaThisProperty()() should equal the global object's globalVar property");
                assert.areEqual(globalVar, globalLambdaEvalLambdaThisPropertyCall(), "globalLambdaEvalLambdaThisPropertyCall() should equal the global object's globalVar property");

                assert.areEqual(globalObject, globalEvalLambdaEvalThis(), "globalEvalLambdaEvalThis() should equal the global object");
                assert.areEqual(globalVar, globalEvalLambdaEvalThisProperty(), "globalEvalLambdaEvalThisProperty() should equal the global object");

                assert.areEqual(globalObject, globalEvalLambdaThisWithNestedEval(), "globalEvalLambdaThisWithNestedEval() should equal the global object");
                assert.areEqual(globalObject, globalEvalNestedEvalLambdaThis(), "globalEvalNestedEvalLambdaThis() should equal the global object");
                assert.areEqual(globalObject, globalEvalNestedEvalLambdaThisWithNestedEval(), "globalEvalNestedEvalLambdaThisWithNestedEval() should equal the global object");
            }

            test.call(thisVal);
        }
    },
    {
        name: "New line characters are not allowed between arrow parameters and =>",
        body: function () {
            assert.throws(function () { eval('x \n => d;'); }, SyntaxError, "Arrow with simple expression body and simple parameter", "Syntax error");
            assert.throws(function () { eval('var a = x \n => d;'); }, SyntaxError, "Arrow with simple expression body and simple parameter assigned to a var", "Syntax error");
            assert.throws(function () { eval('(x) \n => d;'); }, SyntaxError, "Arrow with simple expression body and single parameter", "Syntax error");
            assert.throws(function () { eval('var a = (x) \n => d;'); }, SyntaxError, "Arrow with simple expression body and single parameter assigned to a var", "Syntax error");
            assert.throws(function () { eval('() \n => d;'); }, SyntaxError, "Arrow with simple expression body and empty parameter list", "Syntax error");
            assert.throws(function () { eval('var a = () \n => d;'); }, SyntaxError, "Arrow with simple expression body and empty parameter list assigned to a var", "Syntax error");

            assert.throws(function () { eval('x \n => { return d };'); }, SyntaxError, "Arrow with block body and simple parameter", "Syntax error");
            assert.throws(function () { eval('var a = x \n => { return d };'); }, SyntaxError, "Arrow with block body and simple parameter assigned to a var", "Syntax error");
            assert.throws(function () { eval('(x) \n => { return d };'); }, SyntaxError, "Arrow with block body and single parameter", "Syntax error");
            assert.throws(function () { eval('var a = (x) \n => { return d };'); }, SyntaxError, "Arrow with block body and single parameter assigned to a var", "Syntax error");
            assert.throws(function () { eval('() \n => { return d };'); }, SyntaxError, "Arrow with block body and empty parameter list", "Syntax error");
            assert.throws(function () { eval('var a = () \n => { return d };'); }, SyntaxError, "Arrow with block body and empty parameter list assigned to a var", "Syntax error");

            assert.throws(function () { eval('var a = {}; a.x \n => d;'); }, SyntaxError, "Verify that badly formed arrow functions return correct error even if a newline is before the => token", "Syntax error");
            assert.throws(function () { eval('var a = {}; a\n.x => d;'); }, SyntaxError, "Verify that badly formed arrow functions return correct error even if a newline is before the => token", "Syntax error");
        }
    }
];

testRunner.runTests(tests, { verbose: WScript.Arguments[0] != "summary" });
