//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// ES6 Genertors functionality tests -- verifies behavior of generator functions

WScript.LoadScriptFile("..\\UnitTestFramework\\UnitTestFramework.js");

function ExpectedException() {
    this.message = "Expected Exception";
}

var x = 0;
function CreateIterable(nextFunc, returnFunc, throwFunc) {
    return {
        [Symbol.iterator]: function () {
            var obj = { i : 1 };
            if (nextFunc) {
                obj.next = nextFunc;
            }
            if (returnFunc) {
                obj.return = returnFunc;
            }
            if (throwFunc) {
                obj.throw = throwFunc;
            }
            return obj;
        }
    }
}

function simpleNextFunc() {
    return {
        done: this.i == 3,
        value: this.i++
    };
}

function simpleReturnFunc() {
    x++;
    return {
        done: true,
        value: undefined
    };
}

function simpleThrowFunc() {
    x++;
    return {
        done: true,
        value: undefined
    };
}

var global = (function() { return this; }());

var tests = [
    {
        name: "Simple generator functions with no parameters or locals or captures",
        body: function () {
            function* gf1() { }
            assert.areEqual({ value: undefined, done: true }, gf1().next(), "Empty generator is complete on first call to next and returns undefined");

            function* gf2() { return 123; }
            assert.areEqual({ value: 123, done: true }, gf2().next(), "Generator that returns value but does not yield is complete on first call to next and returns that value");

            function* gf3() { yield 1; }
            var g3 = gf3();
            assert.areEqual({ value: 1, done: false }, g3.next(), "Generator that yields once is not complete on the first call but returns the yielded value");
            assert.areEqual({ value: undefined, done: true }, g3.next(), "Generator that yields once is complete on the second call and returns undefined");

            function* gf4() { yield 1; return 123; }
            var g4 = gf4();
            assert.areEqual({ value: 1, done: false }, g4.next(), "Generator that yields once is not complete on the first call but returns the yielded value");
            assert.areEqual({ value: 123, done: true }, g4.next(), "Generator that yields once and has explicit return is complete on the second call and returns the specified value");

            function* gf5() { yield 1; yield 2; yield 3; return 10; }
            var g5 = gf5();
            assert.areEqual({ value: 1, done: false }, g5.next(), "Generator with three yields, first call to next gives first yielded value");
            assert.areEqual({ value: 2, done: false }, g5.next(), "Generator with three yields, second call to next gives second yielded value");
            assert.areEqual({ value: 3, done: false }, g5.next(), "Generator with three yields, third call to next gives third yielded value");
            assert.areEqual({ value: 10, done: true }, g5.next(), "Generator with three yields, fourth call to next gives return value");
        }
    },
    {
        name: "Calling a completed generator returns { value: undefined, done: true }",
        body: function () {
            function* gf() { yield 1; return 0; }
            var g = gf();

            assert.areEqual({ value: 1, done: false }, g.next(), "Just make sure we get the yield value");
            assert.areEqual({ value: 0, done: true }, g.next(), "Just make sure we get the return value");
            assert.areEqual({ value: undefined, done: true }, g.next(), "Calling completed generator gives back undefined and iteration complete");
            assert.areEqual({ value: undefined, done: true }, g.next(), "Calling completed generator gives back undefined and iteration complete second time");
            assert.areEqual({ value: undefined, done: true }, g.next(), "Calling completed generator gives back undefined and iteration complete every time");
        }
    },
    {
        name: "A single generator function creates distinct generator objects each time it is called",
        body: function () {
            function* gf() { yield 1; yield 2; yield 3; return 0; }

            var g1 = gf();
            var g2 = gf();

            assert.areEqual({ value: 1, done: false }, g1.next(), "next() on first generator returns first yielded value");
            assert.areEqual({ value: 1, done: false }, g2.next(), "interleaved call to next() on second generator returns first yielded value");
            assert.areEqual({ value: 2, done: false }, g1.next(), "interleaved next() on first generator returns second yielded value");
            assert.areEqual({ value: 2, done: false }, g2.next(), "interleaved call to next() on second generator returns second yielded value");
            assert.areEqual({ value: 3, done: false }, g1.next(), "interleaved next() on first generator returns third yielded value");
            assert.areEqual({ value: 3, done: false }, g2.next(), "interleaved call to next() on second generator returns third yielded value");
            assert.areEqual({ value: 0, done: true }, g1.next(), "interleaved next() on first generator finishes it with return value");
            assert.areEqual({ value: 0, done: true }, g2.next(), "interleaved call to next() on second generator finishes it with return value");

            var g3 = gf();

            assert.areEqual({ value: 1, done: false }, g3.next(), "independent next() on third generator returns first yielded value");
            assert.areEqual({ value: 2, done: false }, g3.next(), "independent next() on third generator returns second yielded value");
            assert.areEqual({ value: 3, done: false }, g3.next(), "independent next() on third generator returns third yielded value");
            assert.areEqual({ value: 0, done: true }, g3.next(), "independent next() on third generator finishes it with return value");
        }
    },
    {
        name: "Sanity check: generator used in for-of works as expected",
        body: function () {
            function* gf() { yield 1; yield 2; yield 3; return 0; }
            var a = [];

            for (let x of gf()) {
                a.push(x);
            }

            assert.areEqual(3, a.length, "for-of loop only had as many iterations as there were yields in the generator");
            assert.areEqual(1, a[0], "for-of loop first iteration value was first yielded value of generator");
            assert.areEqual(2, a[1], "for-of loop second iteration value was second yielded value of generator");
            assert.areEqual(3, a[2], "for-of loop third iteration value was third yielded value of generator");
        }
    },
    {
        name: "Yield expression result value is the argument value that was passed to next()",
        body: function () {
            function* gf() {
                assert.areEqual('a', yield 1, "Second call to next() passes argument as result of first yield");
                assert.areEqual('b', yield 2, "Third call to next() passes argument as result of first yield");
                assert.areEqual('c', yield 3, "Fourth call to next() passes argument as result of first yield");
                return 0;
            }

            var g = gf();
            assert.areEqual({ value: 1, done: false }, g.next('ignored'), "Sanity check that result value is first yield value and note that the argument to the first call to next is silently ignored");
            assert.areEqual({ value: 2, done: false }, g.next('a'), "Sanity check that result value is second yield value");
            assert.areEqual({ value: 3, done: false }, g.next('b'), "Sanity check that result value is third yield value");
            assert.areEqual({ value: 0, done: true }, g.next('c'), "Sanity check that result value is return value");
        }
    },
    {
        name: "Generator function with locals that span yields",
        body: function () {
            function* gf() {
                var a = 0, b = 1;

                yield a; a += b++;
                yield a; a += b++;
                yield a; a += b++;
            }

            var g = gf();
            assert.areEqual(0, g.next().value, "Initial value of a from the generator");
            assert.areEqual(1, g.next().value, "Second value of a, incremented by initial value of b, from the generator");
            assert.areEqual(3, g.next().value, "Third value of a, incremented by second value of b, from the generator");
        }
    },
    {
        name: "Generator function with parameters",
        body: function () {
            function* gf(a, b, c) {
                yield a;
                yield b;
                yield c;
            }

            var g = gf(0, 1, 2);
            assert.areEqual(0, g.next().value, "Initial value is first argument");
            assert.areEqual(1, g.next().value, "Second value is second argument");
            assert.areEqual(2, g.next().value, "Third value is third argument");

            g = gf(0);
            assert.areEqual(0, g.next().value, "Initial value is first and only given argument");
            assert.areEqual(undefined, g.next().value, "Second value is undefined for unspecified argument");
            assert.areEqual(undefined, g.next().value, "Third value is undefined for unspecified argument");

            g = gf(3, 4, 5, 6, 7);
            assert.areEqual(3, g.next().value, "Initial value is first argument (more arguments than formal parameters)");
            assert.areEqual(4, g.next().value, "Second value is second argument (more arguments than formal parameters)");
            assert.areEqual(5, g.next().value, "Third value is third argument (more arguments than formal parameters)");
            assert.areEqual(undefined, g.next().value, "Last value is undefined, extra arguments ignored");
        }
    },
    {
        name: "Generator functions with rest parameter",
        body: function () {
            function* gf1(...r) {
                for (var i = 0; i < r.length; i += 1) {
                    yield r[i];
                }
            }

            var g = gf1();
            assert.areEqual({ value: undefined, done: true }, g.next(), "gf1 yields no values when given no arguments");

            g = gf1(0);
            assert.areEqual(0, g.next().value, "Initial value is first argument passed in");
            assert.areEqual({ value: undefined, done: true }, g.next(), "gf1 is complete after first argument yielded");

            g = gf1(0, 1, 2, 3);
            assert.areEqual(0, g.next().value, "Initial value is first argument passed in");
            assert.areEqual(1, g.next().value, "Second value is second argument passed in");
            assert.areEqual(2, g.next().value, "Third value is third argument passed in");
            assert.areEqual(3, g.next().value, "Fourth value is fourth argument passed in");
            assert.areEqual(undefined, g.next().value, "gf1 is complete after last argument yielded");

            function* gf2(a, b, ...r) {
                for (var i = r.length - 1; i >= 0; i -= 1) {
                    yield r[i];
                }
                yield b;
                yield a;
            }

            g = gf2();
            assert.areEqual({ value: undefined, done: false }, g.next(), "Initial value is undefined for unspecified second argument");
            assert.areEqual({ value: undefined, done: false }, g.next(), "Second value is undefined for unspecified first argument");
            assert.areEqual({ value: undefined, done: true }, g.next(), "gf2 is complete after yielding first two formal parameters despite them being unspecified");

            g = gf2(0, 1);
            assert.areEqual({ value: 1, done: false }, g.next(), "Initial value is second argument");
            assert.areEqual({ value: 0, done: false }, g.next(), "Second value is first argument");
            assert.areEqual({ value: undefined, done: true }, g.next(), "gf2 is complete after yielding first two formal parameters since rest parameter is empty");

            g = gf2(0, 1, 2, 3, 4);
            assert.areEqual({ value: 4, done: false }, g.next(), "Initial value is third rest parameter argument");
            assert.areEqual({ value: 3, done: false }, g.next(), "Second value is second rest parameter argument");
            assert.areEqual({ value: 2, done: false }, g.next(), "Third value is first rest parameter argument");
            assert.areEqual({ value: 1, done: false }, g.next(), "Fourth value is second argument");
            assert.areEqual({ value: 0, done: false }, g.next(), "Fifth value is first argument");
            assert.areEqual({ value: undefined, done: true }, g.next(), "gf2 is complete after yielding first two formal parameters since rest parameter is empty");
        }
    },
    {
        name: "Geneartor functions with this reference",
        body: function () {
            function* gf(a) {
                yield 1 + a + this.a;
            }
            g = gf.call({a : 100}, 10);
            assert.areEqual({value : 111, done: false}, g.next(), "Returns the sum of 1, argument and the this's property's");
            assert.areEqual({value: undefined, done: true}, g.next(), "Generator is in completed state");
        }
    },
    {
        name: "Generator declarations and methods cannot be used as a constructor",
        body: function () {
            function* gf(a) { yield 1 + a + this.a; }
            assert.throws( function () { new gf(); }, TypeError, "Generator declarations used as constructor throws TypeError", "Function is not a constructor");
            assert.throws( function () { new gf(10); }, TypeError, "Generator declarations used as constructor with parameters throws TypeError", "Function is not a constructor");

            var obj1 = { *gf() {} };
            assert.throws( function () { new obj1.gf(); }, TypeError, "Generator methods used as constructor throws TypeError", "Function is not a constructor");

            class c { *gf() {} };
            var obj2 = new c();
            assert.throws( function () { new obj2.gf(); }, TypeError, "Generator methods in class used as constructor throws TypeError", "Function is not a constructor");
        }
    },
    {
        name: "Generator function using arguments.length where arguments does not escape",
        body: function () {
            function* gf() {
                return arguments.length;
            }

            assert.areEqual(0, gf().next().value, "Passing zero arguments should result in length of 0");
            assert.areEqual(1, gf(0).next().value, "Passing one arguments should result in length of 1");
            assert.areEqual(3, gf(0, 1, 2).next().value, "Passing three arguments should result in length of 3");
        }
    },
    {
        name: "Generator function using arguments elements where arguments does not escape",
        body: function () {
            function* gf() {
                for (var i = 0; i < arguments.length; i += 1) {
                    yield arguments[i];
                }
            }

            var g = gf();
            assert.areEqual({ value: undefined, done: true }, g.next(), "No results yielded if arguments is empty");

            g = gf(0);
            assert.areEqual({ value: 0, done: false }, g.next(), "Passing one argument should yield back that argument");
            assert.areEqual({ value: undefined, done: true }, g.next(), "Generator is in completed state after yielding the one argument");

            g = gf(0, 1, 2);
            assert.areEqual({ value: 0, done: false }, g.next(), "First value is the first argument");
            assert.areEqual({ value: 1, done: false }, g.next(), "Second value is the second argument");
            assert.areEqual({ value: 2, done: false }, g.next(), "Third value is the third argument");
            assert.areEqual({ value: undefined, done: true }, g.next(), "Generator is in completed state after yielding the three arguments");
        }
    },
    {
        name: "Generator function that captures outer scope locals",
        body: function () {
            function f(a) {
                let b = 0;
                function* gf() {
                    yield a++;
                    yield b++;
                    yield a++;
                    yield b++;
                }
                return gf;
            }

            var gf = f(10);
            var g = gf();

            assert.areEqual(10, g.next().value, "First value for a is initial 10 from the argument given in the call to f()");
            assert.areEqual( 0, g.next().value, "First value for b is 0 from initialization in f()");
            assert.areEqual(11, g.next().value, "Second value for a is the next integer value");
            assert.areEqual( 1, g.next().value, "Second value for b is the next integer value");
            assert.areEqual(undefined, g.next().value, "Only four yields, return value is undefined");

            g = gf();

            assert.areEqual(12, g.next().value, "New generator uses the same a so this is the third value which is the next integer value");
            assert.areEqual( 2, g.next().value, "New generator uses the same b so this is the third value which is the next integer value");
            assert.areEqual(13, g.next().value, "Fourth value for a is the next integer value");
            assert.areEqual( 3, g.next().value, "Fourth value for b is the next integer value");
            assert.areEqual(undefined, g.next().value, "Only four yields, so again, return value is undefined");

            gf = f(5);
            g = gf();

            assert.areEqual(5, g.next().value, "New instance of the generator function from a second call to f where a starts at the argument given in the second call to f()");
            assert.areEqual(0, g.next().value, "New instance of the generator function from a second call to f where b is 0 from initialization in f()");
            assert.areEqual(6, g.next().value, "Second value for a in new environment is the next integer value");
            assert.areEqual(1, g.next().value, "Second value for b in new environment is the next integer value");
        }
    },
    {
        name: "Yield expressions inside control flow constructs: if/else-if/else statement",
        body: function () {
            function* gf(a) {
                if (a === 'if') {
                    yield 1;
                } else if (a === 'else if') {
                    yield 2;
                } else {
                    yield 3;
                }
                return 0;
            }

            var g = gf('if');
            assert.areEqual(1, g.next().value, "This generator yields from the if branch");
            assert.areEqual(0, g.next().value, "It does not yield from the else-if or else branches and hits the return statement");

            g = gf('else if');
            assert.areEqual(2, g.next().value, "This generator yields from the else-if branch");
            assert.areEqual(0, g.next().value, "It does not yield from the else branch and hits the return statement");

            g = gf();
            assert.areEqual(3, g.next().value, "This generator yields from the else branch");
            assert.areEqual(0, g.next().value, "And hits the return statement");
        }
    },
    {
        name: "Yield expressions inside control flow constructs: switch statement",
        body: function () {
            function* gf(a) {
                switch (a) {
                    case 1:
                        yield 1;
                        break;
                    case 2:
                        yield 2;
                        // fallthrough
                    case 3:
                        yield 3;
                        break;
                    default:
                        yield -1;
                        break;
                }
                return 0;
            }

            var g = gf(1);
            assert.areEqual(1, g.next().value, "This generator yields from case 1");
            assert.areEqual(0, g.next().value, "It then breaks and hits the return statement");

            g = gf(2);
            assert.areEqual(2, g.next().value, "This generator yields from case 2");
            assert.areEqual(3, g.next().value, "It then falls through and yields from case 3");
            assert.areEqual(0, g.next().value, "And then it breaks and hits the return statement");

            var g = gf(3);
            assert.areEqual(3, g.next().value, "This generator yields from case 3");
            assert.areEqual(0, g.next().value, "It then breaks and hits the return statement");

            var g = gf(4);
            assert.areEqual(-1, g.next().value, "This generator yields from the default case");
            assert.areEqual(0, g.next().value, "It then breaks and hits the return statement");
        }
    },
    {
        name: "Yield expressions inside control flow constructs: while loop statement",
        body: function () {
            function* gf() {
                var a = 1;
                while (a < 3) {
                    yield a;
                    a += 1;
                }
                return 0;
            }

            var g = gf();
            assert.areEqual(1, g.next().value, "This generator yields 1 on the first iteration of the while loop");
            assert.areEqual(2, g.next().value, "This generator yields 2 on the second iteration of the while loop");
            assert.areEqual(0, g.next().value, "And completes the loop there and hits the return statement");
        }
    },
    {
        name: "Yield expressions inside control flow constructs: for loop statement",
        body: function () {
            function* gf() {
                for (var a = 1; a < 3; a++) {
                    yield a;
                }
                return 0;
            }

            var g = gf();
            assert.areEqual(1, g.next().value, "This generator yields 1 on the first iteration of the for loop");
            assert.areEqual(2, g.next().value, "This generator yields 2 on the second iteration of the for loop");
            assert.areEqual(0, g.next().value, "And completes the loop there and hits the return statement");
        }
    },
    {
        name: "Yield expressions inside control flow constructs: for-in loop statement",
        body: function () {
            function* gf() {
                var o = { a: '', b: '' };
                for (var a in o) {
                    yield a;
                }
                return 0;
            }

            var g = gf();
            assert.areEqual('a', g.next().value, "This generator yields property name 'a' on the first iteration of the for-in loop");
            assert.areEqual('b', g.next().value, "This generator yields property name 'b' on the second iteration of the for-in loop");
            assert.areEqual(0, g.next().value, "And completes the loop there and hits the return statement");
        }
    },
    {
        name: "Yield expressions inside control flow constructs: for-of loop statement",
        body: function () {
            function* gf() {
                for (var a of [1, 2]) {
                    yield a;
                }
                return 0;
            }

            var g = gf();
            assert.areEqual(1, g.next().value, "This generator yields 1 on the first iteration of the for-of loop");
            assert.areEqual(2, g.next().value, "This generator yields 2 on the second iteration of the for-of loop");
            assert.areEqual(0, g.next().value, "And completes the loop there and hits the return statement");
        }
    },
    {
        name: "Yield expression in a return statement doesn't clobber R0 register used by return statement",
        body: function () {
            function* gf1() {
                return {
                    [yield 1]: 2
                };
            }

            var g = gf1();
            assert.areEqual({ value: 1, done: false }, g.next(), "The yield's result value is not somehow corrupted by being part of a return statement (in computed property)");
            assert.areEqual({ value: { foo: 2 }, done: true }, g.next("foo"), "The generator's return value is not corrupted by the yield");

            function* gf2() {
                return yield 1;
            }

            g = gf2();
            assert.areEqual({ value: 1, done: false }, g.next(), "The yield's result value is not somehow corrupted by being part of a return statement");
            assert.areEqual({ value: 2, done: true }, g.next(2), "The generator's return value is the yield's result value");
        }
    },
    {
        name: "Generator that throws is put in completed state",
        body: function () {
            function* gf() {
                throw new Error();
                yield 10;
                return 20;
            }

            var g = gf();

            assert.throws(function () { g.next(); }, Error, "Generator immediately throws a new Error object");
            assert.areEqual({ value: undefined, done: true }, g.next(), "The generator is now completed so successive calls to next() return done: true; it does not yield 10");
            assert.areEqual({ value: undefined, done: true }, g.next(), "The generator is now completed so successive calls to next() return done: true; it does not return 20");
        }
    },
    {
        name: "Cannot reenter an active generator function",
        body: function () {
            function* gf() {
                assert.throws(function () { g.next() }, TypeError, "Calling next() on an active generator throws TypeError", "Generator.prototype.next: Cannot execute generator function because it is currently executing");
                assert.throws(function () { g.return(1) }, TypeError, "Calling return() on an active generator throws TypeError", "Generator.prototype.return: Cannot execute generator function because it is currently executing");
                assert.throws(function () { g.throw(1) }, TypeError, "Calling throw() on an active generator throws TypeError", "Generator.prototype.throw: Cannot execute generator function because it is currently executing");
                return 123;
            }
            var g = gf();

            assert.areEqual({ value: 123, done: true }, g.next(), "Ensure that gf() executed");
        }
    },
    {
        name: "toString() of a generator function should match the script code same as normal functions",
        body: function () {
            function* gf1() { }
            function* gf2(a, b, c) {
                yield a + b + c;
            }
            var gf3 = function* () { };

            assert.areEqual("function* gf1() { }", gf1.toString(), "generator function declaration as a string");
            assert.areEqual("function* gf2(a, b, c) {\r\n                yield a + b + c;\r\n            }", gf2.toString(), "generator function declaration with new lines, parameters and body as a string");
            assert.areEqual("function* () { }", gf3.toString(), "anonymous generator function expression as a string");
        }
    },
    {
        name: "GeneratorFunction constructor creates generator functions named anonymous",
        body: function () {
            var GeneratorFunction = Object.getPrototypeOf(function* () { }).constructor;

            var gf = new GeneratorFunction('yield 1; return 0;');

            var g = gf();

            assert.areEqual("function* anonymous() {\nyield 1; return 0;\n}", gf.toString(), "toString of GeneratorFunction constructed function is named anonymous");

            assert.areEqual({ value: 1, done: false }, g.next(), "gf is a generator function whose body yield's 1 then returns 0");
            assert.areEqual({ value: 0, done: true }, g.next(), "gf is a generator function whose body yield's 1 then returns 0");

            gf = new GeneratorFunction('a', 'b', 'c', 'yield a; yield b; yield c;');
            g = gf(1, 2, 3);

            assert.areEqual("function* anonymous(a,b,c) {\nyield a; yield b; yield c;\n}", gf.toString(), "toString of GeneratorFunction constructed function is named anonymous with specified parameters");

            assert.areEqual({ value: 1, done: false }, g.next(), "gf is a generator function that takes three parameters and yields each of them in turn");
            assert.areEqual({ value: 2, done: false }, g.next(), "gf is a generator function that takes three parameters and yields each of them in turn");
            assert.areEqual({ value: 3, done: false }, g.next(), "gf is a generator function that takes three parameters and yields each of them in turn");
            assert.areEqual({ value: undefined, done: true }, g.next(), "gf is a generator function that takes three parameters and yields each of them in turn");
        }
    },
    {
        name: "Generator function expression with name should map name to the generator function object",
        body: function () {
            var gf = function* gfrec() {
                return gfrec;
            };
            var x = gf().next().value;

            assert.areEqual(gf, x, "gfrec should refer to the same object returned by the function expression");
        }
    },
    {
        name: "Yield from try and catch blocks",
        body: function () {
            var gf = function* () { try { yield 1; } catch (ex) { }}
            var g = gf();
            assert.areEqual({ value: 1, done: false }, g.next(), "First next call should return 1 from try block");
            assert.areEqual({ value: undefined, done: true }, g.next(), "Second next call should have done as true");

            gf = function* () { try { throw ""; } catch (ex) { yield 1; }}
            g = gf();
            assert.areEqual({ value: 1, done: false }, g.next(), "First next call should return 1 from catch block");
            assert.areEqual({ value: undefined, done: true }, g.next(), "Return from catch block");

            gf = function* () { yield 1; try { yield 2; throw 3; } catch (ex) { yield ex; } yield 4; return 5;}
            g = gf();
            assert.areEqual({ value: 1, done: false }, g.next(), "First next call should return 1 from the function body");
            assert.areEqual({ value: 2, done: false }, g.next(), "Second next call should return 2 from the try block");
            assert.areEqual({ value: 3, done: false }, g.next(), "Third next call should return 3 from the catch block");
            assert.areEqual({ value: 4, done: false }, g.next(), "Forth next call should return 4 from the function body");
            assert.areEqual({ value: 5, done: true }, g.next(), "Return statement from function body");
            assert.areEqual({ value: undefined, done: true }, g.next(), "Return from catch block");

            gf = function* () { try { throw 2; } catch (ex) { yield 1; throw new ExpectedException(); } assert.fail("Control shouldn't reach here"); }
            g = gf();
            assert.areEqual({ value: 1, done: false }, g.next(), "First next call should return 1 from catch block with throw");
            assert.throws(function() { g.next() }, ExpectedException, "Second next call is expected to throw an exception");
            assert.areEqual({ value: undefined, done: true }, g.next(), "Return from catch block");
        }
    },
    {
        name: "Yield from try-finally blocks",
        body: function () {
            var gf = function* gf() { try { yield 1; } finally { yield 2; return 3; }}
            var g = gf();
            assert.areEqual({ value: 1, done: false }, g.next(), "First next call should return 1 from try block");
            assert.areEqual({ value: 2, done: false }, g.next(), "Second next call should return 2 from finally block");
            assert.areEqual({ value: 3, done: true }, g.next(), "Third next call should return 3 from finally block");
            assert.areEqual({ value: undefined, done: true }, g.next(), "Forth next call should have done as true");

            gf = function* () { try { yield 1; throw new ExpectedException(); } finally { yield 2; }}
            g = gf();
            assert.areEqual({ value: 1, done: false }, g.next(), "First next call should return 1 from try block");
            assert.areEqual({ value: 2, done: false }, g.next(), "Second next call should return 2 from finally block");
            assert.throws(function() { g.next() }, ExpectedException, "Third next call is expected to throw an exception from try block");
            assert.areEqual({ value: undefined, done: true }, g.next(), "Forth next call should have done as true");

            gf = function* () { try { yield 1; } finally { yield 2; throw new ExpectedException(); }}
            g = gf();
            assert.areEqual({ value: 1, done: false }, g.next(), "First next call should return 1 from try block");
            assert.areEqual({ value: 2, done: false }, g.next(), "Second next call should return 2 from finally block");
            assert.throws(function() { g.next(); }, ExpectedException, "Third next call is expected to throw an exception from finally block");
            assert.areEqual({ value: undefined, done: true }, g.next(), "Forth next call should have done as true");

            gf = function* () { try { return 2; } finally { yield 1; } }
            g = gf();
            assert.areEqual({ value: 1, done: false }, g.next(), "First next call should return 1 from try block");
            assert.areEqual({ value: 2, done: true }, g.next(), "Second next call should return 2 from finally block");
            assert.areEqual({ value: undefined, done: true }, g.next(), "Forth next call should have done as true");
        }
    },
    {
        name: "Nested blocks of try-catch-finally blocks",
        body: function () {
            var gf = function* () { yield 1; try { yield 2; throw 3; } catch (e) { yield e; } finally { yield 4; } yield 5; }
            var g = gf();
            assert.areEqual({ value: 1, done: false }, g.next(), "First next call should return 1 from the function body");
            assert.areEqual({ value: 2, done: false }, g.next(), "Second next call should return 2 from the try block");
            assert.areEqual({ value: 3, done: false }, g.next(), "Third next call should return 3 from the catch block");
            assert.areEqual({ value: 4, done: false }, g.next(), "Forth next call should return 4 from the finally block");
            assert.areEqual({ value: 5, done: false }, g.next(), "Fifth next call should return 5 from the function body");
            assert.areEqual({ value: undefined, done: true }, g.next(), "Return from catch block");

            gf = function* () { yield 1; try { try { yield 2; throw 4; } finally { yield 3; } yield 100; } catch (e) { yield e; } }
            g = gf();
            assert.areEqual({ value: 1, done: false }, g.next(), "First next call should return 1 from the function body");
            assert.areEqual({ value: 2, done: false }, g.next(), "Second next call should return 2 from the inner try block");
            assert.areEqual({ value: 3, done: false }, g.next(), "Third next call should return 3 from the inner finally block");
            assert.areEqual({ value: 4, done: false }, g.next(), "Forth next call should return 4 from the outer catch block");
            assert.areEqual({ value: undefined, done: true }, g.next(), "Return from catch block");

            gf = function* () { yield 1; try { try { yield 2; throw new ExpectedException(); } finally { yield 3; } yield 100; } finally { yield 4; } }
            g = gf();
            assert.areEqual({ value: 1, done: false }, g.next(), "First next call should return 1 from the function body");
            assert.areEqual({ value: 2, done: false }, g.next(), "Second next call should return 2 from the inner try block");
            assert.areEqual({ value: 3, done: false }, g.next(), "Third next call should return 3 from the inner finally block");
            assert.areEqual({ value: 4, done: false }, g.next(), "Forth next call should return 4 from the outer finally block");
            assert.throws(function() { g.next(); }, ExpectedException, "Fifth next call is expected to throw an exception");
            assert.areEqual({ value: undefined, done: true }, g.next(), "Return from catch block");

            gf = function* () { yield 1; try { try { yield 2; throw 3; } catch(ex) { yield ex; } throw 4; } catch(ex) { yield ex; } }
            g = gf();
            assert.areEqual({ value: 1, done: false }, g.next(), "First next call should return 1 from the function body");
            assert.areEqual({ value: 2, done: false }, g.next(), "Second next call should return 2 from the inner try block");
            assert.areEqual({ value: 3, done: false }, g.next(), "Third next call should return 3 from the inner catch block");
            assert.areEqual({ value: 4, done: false }, g.next(), "Forth next call should return 4 from the outer catch block");
            assert.areEqual({ value: undefined, done: true }, g.next(), "Return from catch block");

            gf = function* () { yield 1; try { try { yield 2; throw 3; } catch(ex) { yield ex; } throw new ExpectedException(); } finally { yield 4; } }
            g = gf();
            assert.areEqual({ value: 1, done: false }, g.next(), "First next call should return 1 from the function body");
            assert.areEqual({ value: 2, done: false }, g.next(), "Second next call should return 2 from the inner try block");
            assert.areEqual({ value: 3, done: false }, g.next(), "Third next call should return 3 from the inner catch block");
            assert.areEqual({ value: 4, done: false }, g.next(), "Forth next call should return 4 from the outer finally block");
            assert.throws(function() { g.next(); }, ExpectedException, "Fifth next call is expected throw an exception");
            assert.areEqual({ value: undefined, done: true }, g.next(), "Return from catch block");

            gf = function* () { yield 1; try { yield 2; try { yield 3; throw 4; } catch (ex) { yield ex; } yield 5; throw 6; } catch (ex) { yield ex; } return 7;}
            g = gf();
            assert.areEqual({ value: 1, done: false }, g.next(), "First next call should return 1 from the function body");
            assert.areEqual({ value: 2, done: false }, g.next(), "Second next call should return 2 from the outer try block");
            assert.areEqual({ value: 3, done: false }, g.next(), "Third next call should return 3 from the inner try block");
            assert.areEqual({ value: 4, done: false }, g.next(), "Forth next call should return 4 from the inner catch body");
            assert.areEqual({ value: 5, done: false }, g.next(), "Fifth next call should return 5 after the end of inner catch body");
            assert.areEqual({ value: 6, done: false }, g.next(), "Sixth next call should return 6 from the outer catch body");
            assert.areEqual({ value: 7, done: true }, g.next(), "Return statement from function body");
            assert.areEqual({ value: undefined, done: true }, g.next(), "Return from catch block");

            gf = function* () { yield 1; try { yield 2; throw 6; } catch (ex) { try { yield 3; throw 4; } catch (ex) { yield ex; } yield 5; yield ex; } return 7;}
            g = gf();
            assert.areEqual({ value: 1, done: false }, g.next(), "First next call should return 1 from the function body");
            assert.areEqual({ value: 2, done: false }, g.next(), "Second next call should return 2 from the outer try block");
            assert.areEqual({ value: 3, done: false }, g.next(), "Third next call should return 3 from the inner try block from the outer catch block");
            assert.areEqual({ value: 4, done: false }, g.next(), "Forth next call should return 4 from the inner catch body from the outer catch block");
            assert.areEqual({ value: 5, done: false }, g.next(), "Fifth next call should return 5 after the end of inner catch body from the outer catch block");
            assert.areEqual({ value: 6, done: false }, g.next(), "Sixth next call should return 6 from the outer catch body");
            assert.areEqual({ value: 7, done: true }, g.next(), "Return statement from function body");
            assert.areEqual({ value: undefined, done: true }, g.next(), "Return from catch block");

            gf = function* () { var x = 1; yield x; try { x = 2; yield x; x = 3; try { yield x; x = 4; throw x; } catch (ex) { yield ex; x = 5; } yield x; throw (x = 6); } catch (ex) { yield x; } x = 7; return x;}
            g = gf();
            assert.areEqual({ value: 1, done: false }, g.next(), "First next call should return the value of x as 1 from the function body");
            assert.areEqual({ value: 2, done: false }, g.next(), "Second next call should return the value of x as 2 from the outer try block");
            assert.areEqual({ value: 3, done: false }, g.next(), "Third next call should return the value of x as 3 from the inner try block");
            assert.areEqual({ value: 4, done: false }, g.next(), "Forth next call should return the value of x as 4 from the inner catch body");
            assert.areEqual({ value: 5, done: false }, g.next(), "Fifth next call should return the value of x as 5 after the end of inner catch body");
            assert.areEqual({ value: 6, done: false }, g.next(), "Sixth next call should return the value of x as 6 from the outer catch body");
            assert.areEqual({ value: 7, done: true }, g.next(), "Return statement from function the value of x as body");
            assert.areEqual({ value: undefined, done: true }, g.next(), "Return from catch block");

            gf = function* () { try { try { return 100; } finally { throw 1; } } catch(e) { yield e; } yield 2; }
            g = gf();
            assert.areEqual({ value: 1, done: false }, g.next(), "First next call should return 1 from the outer catch block");
            assert.areEqual({ value: 2, done: false }, g.next(), "Second next call should return 2 from the outer function body cancelling the return statement");
            assert.areEqual({ value: undefined, done: true }, g.next(), "Return from the function body");
        }
    },
    {
        name: "Test cases with throw api invoked at different states of a generator function where the exception is thrown out of the method",
        body: function () {
            var gf = function* () { yield 1; yield 2; yield 3; return 5; }
            var g = gf();
            assert.areEqual({ value: 1, done: false }, g.next(), "First next call should return 1");
            assert.throws(function() { g.throw(new ExpectedException()); }, ExpectedException, "Throw call is expected to throw the excpetion out as the funtion is not handling it");
            assert.areEqual({ value: undefined, done: true }, g.next(), "Second next call should return undefined as the throw caused a completion of the generator");

            gf = function* () { yield 1; }
            g = gf();
            assert.throws(function() { g.throw(new ExpectedException()); }, ExpectedException, "Throw call is expected to throw the excpetion out as the funtion is not handling it");
            assert.areEqual({ value: undefined, done: true }, g.next(), "Next call after throw should return undefined as the throw caused a completion of the generator");

            gf = function* () { yield 1; }
            g = gf();
            assert.areEqual({ value: 1, done: false }, g.next(), "First next call should return 1");
            assert.throws(function() { g.throw(new ExpectedException()); }, ExpectedException, "Throw call is expected to throw the excpetion out as the funtion is not handling it");
            assert.areEqual({ value: undefined, done: true }, g.next(), "Next call after throw should return undefined as the throw caused a completion of the generator");

            gf = function* () { yield 1; yield 2; }
            g = gf();
            assert.areEqual({ value: 1, done: false }, g.next(), "First next call should return 1");
            assert.throws(function() { g.throw(new ExpectedException()); }, ExpectedException, "Throw call is expected to throw the excpetion out as the funtion is not handling it");
            assert.throws(function() { g.throw(new ExpectedException()); }, ExpectedException, "Second Throw call is also expected to throw the excpetion out as the funtion is not handling it");
            assert.areEqual({ value: undefined, done: true }, g.next(), "Next call after throw should return undefined as the throw caused a completion of the generator");

            gf = function* () { yield 1; }
            g = gf();
            assert.areEqual({ value: 1, done: false }, g.next(), "First next call should return 1");
            assert.throws(function() { g.throw(); }, undefined, "Throw call without any args should throw undefined");
            assert.areEqual({ value: undefined, done: true }, g.next(), "Next call after throw should return undefined as the throw caused a completion of the generator");
        }
    },
    {
        name: "test cases for throw api where the exception is handled by the generator through catch block",
        body: function() {
            var gf = function* () { try { yield 1; throw 100; } catch (ex) { yield ex; }}
            var g = gf();
            assert.areEqual({ value: 1, done: false }, g.next(), "First next call should return 1 from try block");
            assert.areEqual({value: 2, done: false }, g.throw(2), "After throwing the yield should happen from the catch block");
            assert.areEqual({ value: undefined, done: true }, g.next(), "Generator is in comleted state");

            gf = function* () { try { yield 1; throw 2; } catch (ex) { yield ex; yield 100; } assert.fail("Control should never reach here"); }
            g = gf();
            assert.areEqual({ value: 1, done: false }, g.next(), "First next call should return 1 from try block");
            assert.areEqual({value: 2, done: false }, g.next(), "Second next call should return 2 from the catch block");
            assert.throws(function() { g.throw(new ExpectedException()) }, ExpectedException, "The throw happens before the try block");
            assert.areEqual({ value: undefined, done: true }, g.next(), "Generator is in comleted state");
        }
    },
    {
        name: "Test cases to make sure when a throw occurs the finally blocks are executed as expected",
        body: function () {
            var gf = function* () { try { yield 1; } finally { yield 2; } assert.fail("Control should never reach here"); }
            var g = gf();
            assert.areEqual({ value: 1, done: false }, g.next(), "First next call should return 1 from try block");
            assert.areEqual({ value: 2, done: false }, g.throw(new ExpectedException()), "Throw will causes the execution of the finally block");
            assert.throws(function() { g.next() }, ExpectedException, "Second next call should cause the exception to occur after ");
            assert.areEqual({ value: undefined, done: true }, g.next(), "Forth next call should have done as true");

            gf = function* () { try { yield 1; throw 100; } finally { yield 2; }}
            g = gf();
            assert.areEqual({ value: 1, done: false }, g.next(), "First next call should return 1 from try block");
            assert.areEqual({ value: 2, done: false }, g.throw(new ExpectedException()), "Throw causes the execution of the finally block");
            assert.throws(function() { g.next() }, ExpectedException, "Second next call is expected to throw an exception from try block");
            assert.areEqual({ value: undefined, done: true }, g.next(), "Forth next call should have done as true");

            gf = function* () { try { yield 1; } finally { yield 2; throw new ExpectedException(); }}
            g = gf();
            assert.areEqual({ value: 1, done: false }, g.next(), "First next call should return 1 from try block");
            assert.areEqual({ value: 2, done: false }, g.throw(3), "Throw causes the execution of the finally block");
            assert.throws(function() { g.next(); }, ExpectedException, "Second next call is expected to throw an exception from finally block");
            assert.areEqual({ value: undefined, done: true }, g.next(), "Forth next call should have done as true");

            gf = function* () { try { return 2; } finally { yield 1; } }
            g = gf();
            assert.areEqual({ value: 1, done: false }, g.next(), "First next call should return 1 from try block");
            assert.throws(function() { g.throw(new ExpectedException()) }, ExpectedException, "Throw causes the return to be aborted and the exception to occur");
            assert.areEqual({ value: undefined, done: true }, g.next(), "Second next call should have done as true");
        }
    },
    {
        name: "Throw api called when the generator resumes from inside the nested blocks of try-catch-finally",
        body: function () {
            var gf = function* () { try { yield 1; } catch (e) { yield e; } finally { yield 3; } assert.fail("Control should not reach here"); }
            var g = gf();
            assert.areEqual({ value: 1, done: false }, g.next(), "First next call should return 1 from the try block");
            assert.areEqual({ value: 2, done: false }, g.throw(2), "Throw causes the catch block to be executed");
            assert.areEqual({ value: 3, done: false }, g.next(), "Second next call should return 3 from the finally block");
            assert.throws(function() { g.throw(new ExpectedException()); }, ExpectedException, "Second throw causes a throw from finally and ends the method");
            assert.areEqual({ value: undefined, done: true }, g.next(), "Method execution has finished");

            gf = function* () { try { try { yield 1; } finally { yield 2; } assert.fail("Control should never reach here"); } catch (e) { yield e; } }
            g = gf();
            assert.areEqual({ value: 1, done: false }, g.next(), "First next call should return 1 from the inner try block");
            assert.areEqual({ value: 2, done: false }, g.throw(3), "Throw causes the inner finally block to be executed");
            assert.areEqual({ value: 3, done: false }, g.next(), "Second next call should return 3 from the catch block");
            assert.throws(function() { g.throw(new ExpectedException()) }, ExpectedException, "Second throw causses an exception fromt he catch block");
            assert.areEqual({ value: undefined, done: true }, g.next(), "Method execution has finished");

            gf = function* () { try { try { yield 1; throw 100; } finally { yield 2; yield 100; } } finally { yield 3; } }
            g = gf();
            assert.areEqual({ value: 1, done: false }, g.next(), "First next call should return 1 from the inner try block");
            assert.areEqual({ value: 2, done: false }, g.next(), "Second next call should return 2 from the inner finally block");
            assert.areEqual({ value: 3, done: false }, g.throw(new ExpectedException()), "Exception on the inner finally block causes the execution of the second finally block");
            assert.throws(function() { g.next() }, ExpectedException, "Third next call causes the previous exception to be thrown");
            assert.areEqual({ value: undefined, done: true }, g.next(), "Method execution has finished");

            gf = function* () { try { try { yield 1; throw 2; } catch(ex) { yield ex; } throw 100; } catch(ex) { yield ex; } }
            g = gf();
            assert.areEqual({ value: 1, done: false }, g.next(), "First next call should return 1 from the inner try block");
            assert.areEqual({ value: 2, done: false }, g.next(), "Second next call should return 2 from the inner catch block");
            assert.areEqual({ value: 3, done: false }, g.throw(3), "Throw causes an exception from inner catch block and the outer catch block catches it");
            assert.areEqual({ value: undefined, done: true }, g.next(), "Method execution has finished");

            gf = function* () { try { try { yield 1; throw 2; } catch(ex) { yield ex; } yield 3; } finally { yield 4; } }
            g = gf();
            assert.areEqual({ value: 1, done: false }, g.next(), "First next call should return 1 from the inner try block");
            assert.areEqual({ value: 2, done: false }, g.next(), "Second next call should return 2 from the inner catch block");
            assert.areEqual({ value: 3, done: false }, g.next(), "Third next call should return 3 from the inner catch block");
            assert.areEqual({ value: 4, done: false }, g.throw(new ExpectedException()), "Throw causes the execution of finally block");
            assert.throws(function() { g.next(); }, ExpectedException, "Forth next call is expected throw an exception");
            assert.areEqual({ value: undefined, done: true }, g.next(), "Method execution has finished");

            var x = 1;
            gf = function* () { try { yield x; x = 2; try { yield x; } catch (ex) { yield ex; x = 4; } yield x; } catch (ex) { yield x; } x = 6; return x;}
            g = gf();
            assert.areEqual({ value: 1, done: false }, g.next(), "First next call should return the value of x as 1 from the outer try block");
            assert.areEqual({ value: 2, done: false }, g.next(), "Second next call should return the value of x as 2 from the inner try block");
            assert.areEqual({ value: 3, done: false }, g.throw(x = 3), "Throw causes the execution of inner catch block");
            assert.areEqual({ value: 4, done: false }, g.next(), "Third next call should return the value of x as 4 after the inner catch block");
            assert.areEqual({ value: 5, done: false }, g.throw(x = 5), "Second throw causes the outer catch block to be executed");
            assert.areEqual({ value: 6, done: true }, g.next(), "Forth next call should return the value of x as 6 from the function body");
            assert.areEqual({ value: undefined, done: true }, g.next(), "Method execution has finished");

            gf = function* () { try { try { return 100; } finally { yield 1; } } catch(e) { yield e; } yield 3; }
            g = gf();
            assert.areEqual({ value: 1, done: false }, g.next(), "First next call should return 1 from the finally block");
            assert.areEqual({ value: 2, done: false }, g.throw(2), "Throw causes the execution of the catch block");
            assert.areEqual({ value: 3, done: false }, g.next(), "Second next call should return 1 from the outer catch block");
            assert.areEqual({ value: undefined, done: true }, g.next(), "Method execution has finished");
        }
    },
    {
        name: "Test cases with return api invoked at different state of the generator method",
        body: function () {
            var gf = function* () { yield 1; assert.fail("Control should never reach here"); }
            var g = gf();
            assert.areEqual({ value: 1, done: false }, g.next(), "First yield call returns 1");
            assert.areEqual({ value: 2, done: true }, g.return(2), "Return statement returns the value passed in");
            assert.areEqual({ value: undefined, done: true }, g.next(), "Earlier return api invocation completes the method");

            gf = function* () { assert.fail("Control should never reach here"); }
            g = gf();
            assert.areEqual({ value: 1, done: true }, g.return(1), "Calling return when function is in the suspended start state will cause the value to be returned");
            assert.areEqual({ value: undefined, done: true }, g.next(), "Function is in complete state after the return call")

            gf = function* () { yield 1; }
            g = gf();
            assert.areEqual({ value: 1, done: false }, g.next(), "First yield call returns 1");
            assert.areEqual({ value: 2, done: true }, g.return(2), "Return statement returns the value passed in");
            assert.areEqual({ value: 3, done: true }, g.return(3), "Return statement returns the value passed in even if the method is in complete state");
            assert.areEqual({ value: undefined, done: true }, g.next(), "Earlier return api invocation completes the method");

            gf = function* () { for (i = 1; i < 3; i += 2) { yield i; } assert("Control should never reach here"); }
            g = gf();
            assert.areEqual({ value: 1, done: false }, g.next(), "First yield call returns 1 from the loop");
            assert.areEqual({ value: 2, done: true }, g.return(2), "Return statement returns the value passed in");
            assert.areEqual({ value: undefined, done: true }, g.next(), "Earlier return api invocation completes the method");

            gf = function* () { yield 1; return "Control should never reach here"; }
            g = gf();
            assert.areEqual({ value: 1, done: false }, g.next(), "First next call returns 1");
            assert.areEqual({ value: 2, done: true }, g.return(2), "Return statement returns the value passed in");
            assert.areEqual({ value: undefined, done: true }, g.next(), "Earlier return api invocation completes the method");

            gf = function* () { yield 1; }
            g = gf();
            assert.areEqual({ value: 1, done: false }, g.next(), "First next call should return 1");
            assert.areEqual({ value: undefined, done: true }, g.return(), "Return call without any args should return undefined");
            assert.areEqual({ value: undefined, done: true }, g.next(), "Next call after throw should return undefined as the throw caused a completion of the generator");
        }
    },
    {
        name: "Test cases with return api invoked while the generator resumes inside one of the try-catch-finally block",
        body: function () {
            var gf = function* () { try { yield 1; } catch (ex) { }}
            var g = gf();
            assert.areEqual({ value: 1, done: false }, g.next(), "First next call should return 1 from try block");
            assert.areEqual({ value: 2, done: true }, g.return(2), "Return statement completes the function from try block");
            assert.areEqual({ value: undefined, done: true }, g.next(), "Generator is already in completed state");

            gf = function* () { try { try { yield 1; } finally { yield 2; } } catch (e) { yield e; } }
            g = gf();
            assert.areEqual({ value: 1, done: false }, g.next(), "First next call should return 1 from the inner try block");
            assert.areEqual({ value: 2, done: false }, g.next(), "Second next call returns from the finally block");
            assert.areEqual({ value: 3, done: true }, g.return(3), "Return skips the catch block");
            assert.areEqual({ value: undefined, done: true }, g.next(), "Method execution has finished");

            gf = function* () { try { yield 1; assert.fail("Control should never reach here"); } finally { yield 2; return 3; }}
            g = gf();
            assert.areEqual({ value: 1, done: false }, g.next(), "First next call should return 1 from try block");
            assert.areEqual({ value: 2, done: false }, g.return(100), "Return next call should return 2 from finally block");
            assert.areEqual({ value: 3, done: true }, g.next(), "Return call causes return from the finally block");
            assert.areEqual({ value: undefined, done: true }, g.next(), "Generator is already in completed state");

            gf = function* () { try { throw 1; } catch (e) { yield e; } finally { yield 2; return 3; } }
            g = gf();
            assert.areEqual({ value: 1, done: false }, g.next(), "First next call should return 1 from the catch block");
            assert.areEqual({ value: 2, done: false }, g.return(100), "Return causes the finally block to execute");
            assert.areEqual({ value: 3, done: true }, g.next(), "Second next call should return 3 from the end of finally block");
            assert.areEqual({ value: undefined, done: true }, g.next(), "Method execution has finished");

            gf = function* () { try { throw 1; } catch (e) { yield e; } finally { yield 2; throw new ExpectedException(); } }
            g = gf();
            assert.areEqual({ value: 1, done: false }, g.next(), "First next call should return 1 from the catch block");
            assert.areEqual({ value: 2, done: false }, g.return(100), "Return causes the finally block to execute");
            assert.throws(function () { g.next(); }, ExpectedException, "Second next call should throw from finally block");
            assert.areEqual({ value: undefined, done: true }, g.next(), "Method execution has finished");
        }
    },
    {
        name: "Return api scenarios where the actual return statement is ignored",
        body: function () {
            var gf = function* () { try { yield 1; } finally { yield 2; return 100; }}
            var g = gf();
            assert.areEqual({ value: 1, done: false }, g.next(), "First next call should return 1 from try block");
            assert.areEqual({ value: 2, done: false }, g.next(), "Second next call should return 2 from finally block");
            assert.areEqual({ value: 3, done: true }, g.return(3), "Return call causes return from the finally block");
            assert.areEqual({ value: undefined, done: true }, g.next(), "Generator is already in completed state");

            gf = function* () { try { try { return 100; } finally { throw 1; } } catch(e) { yield e; } assert.fail("Control should never reach here"); }
            g = gf();
            assert.areEqual({ value: 1, done: false }, g.next(), "First next call should return 1 from the outer catch block");
            assert.areEqual({ value: 2, done: true }, g.return(2), "Return call completes the generator and the exception will not be visible");
            assert.areEqual({ value: undefined, done: true }, g.next(), "Return from the function body");

            gf = function* () { try { throw 1; } catch (e) { yield e; } finally { yield 2; } return 100; }
            g = gf();
            assert.areEqual({ value: 1, done: false }, g.next(), "First next call should return 1 from the catch block");
            assert.areEqual({ value: 2, done: false }, g.return(3), "Return causes the finally block to execute");
            assert.areEqual({ value: 3, done: true }, g.next(), "Second next call should return 3 from the function body");
            assert.areEqual({ value: undefined, done: true }, g.next(), "Method execution has finished");

            gf = function* () { var x = yield 1; if (x) { yield x; return "Control should never reach here"; } else { yield x; return 4; }}
            var g1 = gf();
            assert.areEqual({ value: 1, done: false }, g1.next(), "First next call returns 1");
            assert.areEqual({ value: 2, done: false }, g1.next(2), "Second next call returns e");
            assert.areEqual({ value: 3, done: true }, g1.return(3), "Return call returns from the if block");
            var g2 = gf();
            assert.areEqual({ value: 1, done: false }, g2.next(), "First next call returns 1");
            assert.areEqual({ value: 0, done: false }, g2.next(0), "Second next call returns e");
            assert.areEqual({ value: 2, done: true }, g2.return(2), "Return call returns from the if block");
        }
    },
    {
        name: "Return api scenarios where thrown exceptions are ignored",
        body: function () {
            var gf = function* () { try { try { yield 1; throw 100; } finally { yield 2; } } finally { yield 3; } }
            var g = gf();
            assert.areEqual({ value: 1, done: false }, g.next(), "First next call should return 1 from the inner try block");
            assert.areEqual({ value: 2, done: false }, g.next(), "Second next call should return 2 from the inner finally block");
            assert.areEqual({ value: 3, done: false }, g.return(4), "Return api causes the execution of second finally block");
            assert.areEqual({ value: 4, done: true }, g.next(), "Second next call should return 2 from the inner finally block");
            assert.areEqual({ value: undefined, done: true }, g.next(), "Method execution has finished");

            gf = function* () { try { yield 1; throw 100; } finally { yield 2; }}
            g = gf();
            assert.areEqual({ value: 1, done: false }, g.next(), "First next call should return 1 from try block");
            assert.areEqual({ value: 2, done: false }, g.next(), "Second next call should return 2 from finally block");
            assert.areEqual({ value: 3, done: true }, g.return(3), "Return causes the exception to be ignored and return normally from the generator");
            assert.areEqual({ value: undefined, done: true }, g.next(), "Forth next call should have done as true");

            gf = function* () { try { throw ""; } catch (ex) { yield 1; }}
            g = gf();
            assert.areEqual({ value: 1, done: false }, g.next(), "First next call should return 1 from catch block");
            assert.areEqual({ value: 2, done: true }, g.return(2), "Return statement completes the function from catch block");
            assert.areEqual({ value: undefined, done: true }, g.next(), "Generator is already in completed state");
        }
    },
    {
        name: "Working of next, return and throw apis together",
        body: function () {
            var gf = function* () { try { yield 1; } finally { yield 2; assert.fail("Control should not reach here"); } }
            var g = gf();
            assert.areEqual({ value: 1, done: false }, g.next(), "First next call should return 1");
            assert.areEqual({ value: 2, done: false }, g.throw(100), "Throw call causes the second yield to be executed");
            assert.areEqual({ value: 3, done: true }, g.return(3), "Return call overrides the throw work flow");
            assert.areEqual({ value: undefined, done: true }, g.next(), "Method execution has finished");

            gf = function* () { var x = yield 1; var y = yield x; yield y; }
            g = gf();
            assert.areEqual({ value: 1, done: false }, g.next(), "First next call should return 1");
            assert.areEqual({ value: 2, done: false }, g.next(2), "Second next call assigns a value to x");
            assert.areEqual({ value: 3, done: true }, g.return(3), "Return call overrides the throw work flow");
            assert.areEqual({ value: undefined, done: true }, g.next(), "Method execution has finished");

            gf = function* () { try { try { } finally { yield 1; } } finally { yield 2; } }
            g = gf();
            assert.areEqual({ value: 1, done: false }, g.next(), "First next call should return 1");
            assert.areEqual({ value: 2, done: false }, g.throw(100), "Throw call causes the second yield to be executed");
            assert.areEqual({ value: 3, done: true }, g.return(3), "Return call overrides the throw work flow");
        }
    },
    {
        name: "Return and throw apis arguments are propogated on yield*",
        body: function () {
            x = 0;
            var simpleIterator = CreateIterable(simpleNextFunc, simpleReturnFunc, null);
            var gf = function* () { yield* simpleIterator; }
            var g = gf();
            assert.areEqual({value: 1, done: false}, g.next(), "Get the first yield value from the inner iterator");
            assert.areEqual({value: undefined, done: true}, g.return(3), "Returns the value from inner iterator");
            assert.areEqual(1, x, "Make sure that iterator's return is executed");

            x = 0;
            var simpleIterator = CreateIterable(simpleNextFunc, undefined, simpleReturnFunc);
            var gf = function* () { yield* simpleIterator; }
            g = gf();
            assert.areEqual({value: 1, done: false}, g.next(), "Get the first yield value from the inner iterator");
            assert.throws(function () { g.throw(new ExpectedException()); }, ExpectedException, "Throw the exception out ignoring the result from the iterator's throw method");
            assert.areEqual(1, x, "Make sure that iterator's throw is executed");
        }
    },
    {
        name: "Return api with generators on yield*",
        body: function () {
            var gf1 = function* () { yield 1; yield 2; }
            var g1 = gf1();
            var gf2 = function* () { yield* g1; }
            var g2 = gf2();
            assert.areEqual({value: 1, done: false}, g2.next(), "Get the first yield value from the inner generator");
            assert.areEqual({value: 3, done: true}, g2.return(3), "Returns the passed in value");
            assert.areEqual({value: undefined, done: true}, g1.next(), "Inner geneartor is in complete state");
            assert.areEqual({value: undefined, done: true}, g2.next(), "Outer generator is in complete state");

            g1 = gf1();
            g2 = gf2();
            function* gf3() { yield* g2; }
            var g3 = gf3();
            assert.areEqual({value: 1, done: false}, g3.next(), "Get the first yield value from the inner generator");
            assert.areEqual({value: 3, done: true}, g3.return(3), "Returns the passed in value");
            assert.areEqual({value: undefined, done: true}, g1.next(), "Return gets propogated to the lowest level");
            assert.areEqual({value: undefined, done: true}, g2.next(), "Return gets propogated to the second level also");
            assert.areEqual({value: undefined, done: true}, g3.next(), "Return completes the first level of generator also");
        }
    },
    {
        name: "Throw api with generators on yield*",
        body: function () {
            function* gf1() { yield 1; yield 2; }
            var g1 = gf1();
            function* gf2() { yield* g1; }
            var g2 = gf2();
            assert.areEqual({value: 1, done: false}, g2.next(), "Get the first yield value from the inner generator");
            assert.throws(function () { g2.throw(new ExpectedException()); }, ExpectedException, "Throw comes out of the generator loop as nobody handles it");
            assert.areEqual({value: undefined, done: true}, g1.next(), "Throw gets propogated to the lowest level");
            assert.areEqual({value: undefined, done: true}, g2.next(), "Throw gets propogated to the top level also");

            g1 = gf1();
            g2 = gf2();
            function* gf3() { yield* g2; }
            var g3 = gf3();
            assert.areEqual({value: 1, done: false}, g3.next(), "Get the first yield value from the inner generator");
            assert.throws(function () { g3.throw(new ExpectedException()); }, ExpectedException, "Throw comes out of the generator loop as nobody handles it");
            assert.areEqual({value: undefined, done: true}, g1.next(), "Throw gets propogated to the lowest level");
            assert.areEqual({value: undefined, done: true}, g2.next(), "Throw gets propogated to the second level also");
            assert.areEqual({value: undefined, done: true}, g3.next(), "Throw completes the first level of generator also");
        }
    },
    {
        name: "Exceptions from outer yield* can be caught in the inner generators",
        body: function () {
            var gf1 = function* () {
                try {
                    yield 1;
                    assert.fail("Control should never reach here");
                } catch (e) {
                    assert.areEqual(2, e, "Catch the exception from the outer yield");
                    yield 100;
                }
                yield 3;
            }
            var g1 = gf1();
            var gf2 = function* () { yield* g1; }
            var g2 = gf2();
            var gf3 = function* () { yield* g2; }
            var g3 = gf3();
            assert.areEqual({value: 1, done: false}, g3.next(), "Yield 1 from the inner generator");
            assert.throws(function () { g3.throw(2); }, 2, "Even thogh the inner generator handles the throw the result from it is ignored");
            assert.areEqual({value: 3, done: false}, g1.next(), "First generator handled the exception so it is not in complete state yet");
            assert.areEqual({value: undefined, done: true}, g2.next(), "Second generator does not handle the exception so it is in complete state");
            assert.areEqual({value: undefined, done: true}, g3.next(), "Third generator also does not handle the exception so it is in complete state");

            gf1 = function* () { yield 1; assert.fail("Control should never reach here"); }
            g1 = gf1();
            gf2 = function* () {
                try {
                    yield *g1;
                } catch (e) {
                    assert.areEqual(2, e, "Catch the exception here");
                    yield 100;
                }
                yield 3;
            }
            g2 = gf2();
            g3 = gf3();
            assert.areEqual({value: 1, done: false}, g3.next(), "Yield 1 from the inner generator");
            assert.throws(function () { g3.throw(2); }, 2, "Even though the inner generator handles the throw the result from it is ignored");
            assert.areEqual({value: undefined, done: true}, g1.next(), "First generator does not handle the exception so it is in complete state");
            assert.areEqual({value: 3, done: false}, g2.next(), "Second generator handles the exception so it is not in complete state yet");
            assert.areEqual({value: undefined, done: true}, g3.next(), "Third generator also does not handle the exception so it is in complete state");
        }
    },
    {
        name: "Throw and return invokes all the finally blocks in the yield* stack",
        body: function () {
            x = 0;
            var gf1 = function* () {
                try {
                    yield 1;
                    assert.fail("Control should never reach here");
                } finally {
                    x += 1;
                }
            }
            var g1 = gf1();
            var gf2 = function* () {
                try {
                    yield* g1;
                    assert.fail("Control should never reach here");
                } finally {
                    x += 3;
                }
            }
            var g2 = gf2();
            var gf3 = function* () {
                try {
                    yield* g2;
                    assert.fail("Control should never reach here");
                } finally {
                    x += 5;
                }
            }
            var g3 = gf3();
            assert.areEqual({value: 1, done: false}, g3.next(), "Yield 1 from the first generator");
            assert.throws(function () { g3.throw(new ExpectedException()); }, ExpectedException, "The exception thrown comes out as the outer generator does not handle it");
            assert.areEqual(9, x, "All finally blocks must be executed");
            assert.areEqual({value: undefined, done: true}, g1.next(), "First generator should be in complete state");
            assert.areEqual({value: undefined, done: true}, g2.next(), "Second generator should be in complete state");
            assert.areEqual({value: undefined, done: true}, g3.next(), "Third generator should be in complete state");

            x = 0;
            g1 = gf1();
            g2 = gf2();
            g3 = gf3();
            assert.areEqual({value: 1, done: false}, g3.next(), "Yield 1 from the first generator");
            assert.areEqual({value: 2, done: true}, g3.return(2), "Gets the return value as it is passed in");
            assert.areEqual(9, x, "All finally blocks must be executed");
            assert.areEqual({value: undefined, done: true}, g1.next(), "First generator should be in complete state");
            assert.areEqual({value: undefined, done: true}, g2.next(), "Second generator should be in complete state");
            assert.areEqual({value: undefined, done: true}, g3.next(), "Third generator should be in complete state");

            gf1 = function* () {
                try {
                    yield 1;
                    assert.fail("Control should never reach here");
                } finally {
                    x += 1;
                    return 100;
                }
            }
            x = 0;
            g1 = gf1();
            g2 = gf2();
            g3 = gf3();
            assert.areEqual({value: 1, done: false}, g3.next(), "Yield 1 from the first generator");
            assert.throws(function () { g3.throw(new ExpectedException()); }, ExpectedException, "The exception thrown comes out as the outer generator ignoring the return value from the inner generator");
            assert.areEqual(9, x, "All finally blocks must be executed");
            assert.areEqual({value: undefined, done: true}, g1.next(), "First generator should be in complete state");
            assert.areEqual({value: undefined, done: true}, g2.next(), "Second generator should be in complete state");
            assert.areEqual({value: undefined, done: true}, g3.next(), "Third generator should be in complete state");

            gf1 = function* () {
                try {
                    try {
                        yield 1;
                        assert.fail("Control should never reach here");
                    } finally {
                        x += 1;
                    }
                } finally {
                    x += 2;
                }
            }
            g1= gf1();
            gf2 = function* () {
                try {
                    try {
                        try {
                            yield* g1;
                            assert.fail("Control should never reach here");
                        } finally {
                            x += 4;
                        }
                    } finally {
                        x += 8;
                    }
                } finally {
                    x += 16;
                }
            }
            g2 = gf2();
            gf3 = function* () {
                try {
                    yield* g2;
                    assert.fail("Control should never reach here");
                } finally {
                    x += 32;
                }
            }
            g3 = gf3();
            x = 0;
            assert.areEqual({value: 1, done: false}, g3.next(), "Yield 1 from the first generator");
            assert.throws(function () { g3.throw(new ExpectedException()); }, ExpectedException, "The exception thrown comes out as the outer generator does not handle it");
            assert.areEqual(63, x, "All finally blocks must be executed");
            assert.areEqual({value: undefined, done: true}, g1.next(), "First generator should be in complete state");
            assert.areEqual({value: undefined, done: true}, g2.next(), "Second generator should be in complete state");
            assert.areEqual({value: undefined, done: true}, g3.next(), "Third generator should be in complete state");

            x = 0;
            g1 = gf1();
            g2 = gf2();
            g3 = gf3();
            assert.areEqual({value: 1, done: false}, g3.next(), "Yield 1 from the first generator");
            assert.areEqual({value: 2, done: true}, g3.return(2), "Gets the return value as it is passed in");
            assert.areEqual(63, x, "All finally blocks must be executed");
            assert.areEqual({value: undefined, done: true}, g1.next(), "First generator should be in complete state");
            assert.areEqual({value: undefined, done: true}, g2.next(), "Second generator should be in complete state");
            assert.areEqual({value: undefined, done: true}, g3.next(), "Third generator should be in complete state");
        }
    },
    {
        name: "Exceptions from inner iterator are popogated",
        body: function () {
            var gf1 = function* () {
                try {
                    yield 1;
                    assert.fail("Control should never reach here");
                } catch (e) {
                    assert.areEqual(2, e, "Catch the outer exception here");
                    throw new ExpectedException();
                }
            }
            var g1 = gf1();
            var gf2 = function* () { yield* g1; }
            var g2 = gf2();
            assert.areEqual({value: 1, done: false}, g2.next(), "Yield 1 from the inner generator");
            assert.throws(function () { g2.throw(2); }, ExpectedException, "Exception from the inner generator overrides the outer exception");
            assert.areEqual({value: undefined, done: true}, g1.next(), "First generator should be in complete state");
            assert.areEqual({value: undefined, done: true}, g2.next(), "Second generator should be in complete state");

            gf1 = function* () {
                try {
                    yield 1;
                } finally {
                    throw new ExpectedException();
                }
            }
            g1 = gf1();
            g2 = gf2();
            assert.areEqual({value: 1, done: false}, g2.next(), "Yield 1 from the inner generator");
            assert.throws(function () { g2.return(100); }, ExpectedException, "Exception from the inner generator overrides the return");
            assert.areEqual({value: undefined, done: true}, g1.next(), "First generator should be in complete state");
            assert.areEqual({value: undefined, done: true}, g2.next(), "Second generator should be in complete state");

            g1 = gf1();
            g2 = gf2();
            assert.areEqual({value: 1, done: false}, g2.next(), "Yield 1 from the inner generator");
            assert.throws(function () { g2.throw(2); }, ExpectedException, "Exception from the inner generator overrides the outer exception");
            assert.areEqual({value: undefined, done: true}, g1.next(), "First generator should be in complete state");
            assert.areEqual({value: undefined, done: true}, g2.next(), "Second generator should be in complete state");
        }
    },
    {
        name: "Returning from the inner iterator will override the return call",
        body: function () {
            var gf1 = function* () {
                try {
                    yield 1;
                    assert.fail("Control should never reach here");
                } finally {
                    return 2;
                }
            }
            var g1 = gf1();
            var gf2 = function* () { yield* g1; }
            g1 = gf1();
            g2 = gf2();
            assert.areEqual({value: 1, done: false}, g2.next(), "Yield 1 from the inner generator");
            assert.areEqual({value: 2, done: true}, g2.return(100), "Return 2 from the inner generator overriding the value passed in");

            gf1 = function* ()  {
                try {
                    return 100;
                } finally {
                    yield 1;
                }
            }
            g1 = gf1();
            g2 = gf2();
            assert.areEqual({value: 1, done: false}, g2.next(), "Yield 1 from the inner generator's finally");
            assert.areEqual({value: 2, done: true}, g2.return(2), "Return 2 overriding the return value from the inner generator's try body");
        }
    },
    {
        name: "yield* when used with an iterator that has incorrect return, and throw properties",
        body: function () {
            var iteratorWithNoReturnOrThrow = CreateIterable(simpleNextFunc);
            var gf = function* () { yield* iteratorWithNoReturnOrThrow; }
            var g = gf();
            assert.areEqual({value: 1, done: false}, g.next(), "Get the first yield value from the inner iterator");
            assert.areEqual({value: 2, done: true}, g.return(2), "As the return property is missing the yield* just returns as is");
            g = gf();
            assert.areEqual({value: 1, done: false}, g.next(), "Get the first yield value from the inner iterator");
            assert.throws(function () { g.throw(new ExpectedException()); }, ExpectedException, "As the throw property is missing the yield* just throws as is");

            var iteratorWithBadReturnAndThrow = CreateIterable(simpleNextFunc, {}, {});
            gf = function* () { yield* iteratorWithBadReturnAndThrow; }
            g = gf();
            assert.areEqual({value: 1, done: false}, g.next(), "Get the first yield value from the inner iterator");
            assert.throws(function () { g.return(100); }, TypeError, "Trying to invoke the return method which is an object not method causes a TypeError", "Function expected");
            g = gf();
            assert.areEqual({value: 1, done: false}, g.next(), "Get the first yield value from the inner iterator");
            assert.throws(function () { g.throw(100); }, TypeError, "Trying to invoke the throw method which is an object not method causes a TypeError", "Function expected");

            var iteratorReturningNonObj = CreateIterable(simpleNextFunc, () => { return this.i; }, () => { return this.i; });
            gf = function* () { yield* iteratorReturningNonObj; }
            g = gf();
            assert.areEqual({value: 1, done: false}, g.next(), "Yield 1 from the iterator");
            assert.throws(function () { g.return(100); }, TypeError, "Result of the return method from iterator should be an object", "Object expected");
            g = gf();
            assert.areEqual({value: 1, done: false}, g.next(), "Yield 1 from the iterator");
            assert.throws(function () { g.throw(new ExpectedException()); }, ExpectedException, "Result of the throw is ignored even if it is not an object, so TypeError won't be thrown out");

            var iteratorReturningWithoutValue = CreateIterable(() => { return {done: false}; }, () => { return {done: true}; }, () => { return {done: true}; });
            gf = function* () { yield* iteratorReturningWithoutValue; }
            g = gf();
            assert.areEqual({value: undefined, done: false}, g.next(), "As the value property is missing undefined is returned");
            assert.areEqual({value: undefined, done: true}, g.return(100), "As the value property is missing undefined is returned");
        }
    },
    {
        name: "Yield* with iterator",
        body: function () {
            var simpleIterator = CreateIterable(simpleNextFunc);
            function* gf () { yield* simpleIterator; return 4; }
            var g = gf();
            assert.areEqual({value: 1, done: false}, g.next(), "Yield 1 from the iterator");
            assert.areEqual({value: 2, done: false}, g.next(), "Yield 2 from the iterator");
            assert.areEqual({value: 4, done: true}, g.next(), "Returned 4 from the generator");
        }
    },
    {
        name: "Yield* with multiple levels of generators",
        body: function () {
            function* gf1 () { yield 1; return 100; }
            function* gf2 () { yield* gf1(); yield* gf1(); }
            var g = gf2();
            assert.areEqual({value: 1, done: false}, g.next(), "Yield 1 from the first inner generator");
            assert.areEqual({value: 1, done: false}, g.next(), "Yield 1 from the second inner generator");
            assert.areEqual({value: undefined, done: true}, g.next(), "Generator is in complete state");
        }
    },
    {
        name: "yield* when used with iterators that have incorrect next methods",
        body: function () {
            function dummy() {}
            var gf = function* () { yield* dummy; };
            assert.throws(function () { gf().next(); }, TypeError, "yield* throws TypeError if its operand does not have the [Symbol.iterator] method", "Object doesn't support property or method 'Symbol.iterator'");

            var iteratorWithNoNextMethod = CreateIterable();
            gf = function* () { yield* iteratorWithNoNextMethod; };
            assert.throws(function () { gf().next(); }, TypeError, "yield* throws TypeError if its operand does not have the next method", "Object doesn't support property or method 'next'");

            var iteratorWithBadNextMethod = CreateIterable({});
            gf = function* () { yield* iteratorWithBadNextMethod; };
            assert.throws(function () { gf().next(); }, TypeError, "yield* throws TypeError if the next property is not a function", "Function expected");

            var iteratorWithBadNextMethod = CreateIterable(function () { return 100; });
            gf = function* () { yield* iteratorWithBadNextMethod; };
            assert.throws(function () { gf().next(); }, TypeError, "yield* throws TypeError if the value returned by next method is not an object", "Object expected");
        }
    },
    {
        name: "Testing 'super' reference inside a generator function",
        body: function () {
            class BASE {
                base0 () { return 0; }
                base1 () { return "BASE"; }
            }

            class A extends BASE {
                *gf () {
                    yield super.base0();
                    return super.base1();
                }
            };

            var o = new A();
            var g = o.gf();

            assert.areEqual(0, g.next().value, "Generator function should be able to yield with 'super' reference");
            assert.areEqual("BASE", g.next().value, "Generator function should be able to return with 'super' reference");
        }
    },
    {
        name: "Cross-site scenarios for generators",
        body: function () {
            if (WScript && WScript.LoadScript) {
                var global = WScript.LoadScript("var obj = { *gf() { yield this.x; return this.y; }, x : 10, y: 11 }; var g = obj.gf();", "samethread");
                var result = global.g.next();
                assert.areEqual(10, result.value, "Next call on the generator object created on a different context should yield fine on this thread");
                assert.areEqual(false, result.done, "The generator object is not closed yet");

                result = global.g.next();
                assert.areEqual(11, result.value, "Next call on the generator object created on a different context should return fine on this thread");
                assert.areEqual(true, result.done, "Generator object is in closed state");

                global = WScript.LoadScript("var obj = { *gf() { yield this.x; return this.y; }, x : 10, y: 11 }; var g = obj.gf();", "samethread");
                global.g.next();
                result = global.g.return(100);
                assert.areEqual(100, result.value, "Return call on the generator object created on a different context should return fine on this thread");
                assert.areEqual(true, result.done, "Generator object is closed by the return call");

                global = WScript.LoadScript("var obj = { *gf() { try { yield this.x; } catch (e) { throw { value : this.x } } }, x : 200 }; var g = obj.gf();", "samethread");
                global.g.next();
                try {
                    global.g.throw(100);
                } catch (e) {
                    assert.areEqual(200, e.value, "Throw call on the generator object created on a different context should should propogate the inner throw result");
                }
            }
        }
    },
    {
        name: "This object validation for strict and non-strict generator functions",
        body: function () {
            var thisValue = null;
            function* method1() { thisValue = this; };
            method1().next();
            assert.areEqual(global, thisValue, "In non-strict mode generators should use the global this");

            thisValue = null;
            function* method2() { 'use strict'; thisValue = this; };
            method2().next();
            assert.areEqual(undefined, thisValue, "In strict mode generators should follow the strict mode semantics and use undefined");
        }
    },
    {
        name: "Iterator protocol violation scenarios",
        body: function () {
            var closed = false;
            var g1, g2;
            function* gf1() { yield 1; }
            function* gf2() { yield *g1; };


            g1 = gf1();
            g1.throw = undefined,
            g1.return = function() { closed = true; return {done: true}; }
            g2 = gf2();
            g2.next();
            assert.throws(function() { g2['throw'](new ExpectedException()) }, ExpectedException, "Throw is propogated back to the caller");
            assert.isTrue(closed, "When throw method is not defined on the iterator IteratorClose is called");

            g1 = gf1();
            g1.throw = undefined,
            g1.return = function() {  throw new ExpectedException(); }
            g2 = gf2();
            g2.next();
            assert.throws(function () { g2['throw']({value : 1}); }, ExpectedException, "Inner exceptions from IteratorClose are propogated");

            g1 = gf1();
            g1.throw = undefined,
            g1.return = function() { return 10; }
            g2 = gf2();
            g2.next();
            assert.throws(function () { g2['throw']({value : 1}); }, TypeError, "A TypeError is thrown if the inner result of iteractor close is not an object", "Object expected");
        }
    },
    {
        name: "Generator method body using super property",
        body: function () {
            var obj = {
                *foo() {  return super.toString; }
            };

            obj.toString = null;

            assert.areEqual(Object.prototype.toString, obj.foo().next().value);
        }
    },
    {
        name: "Generator method body using super property as default argument",
        body: function () {
            var obj = {
                *foo(a = super.toString) {  return a; }
            };

            obj.toString = null;

            assert.areEqual(Object.prototype.toString, obj.foo().next().value);
        }
    },
    {
        name: "super method calls in object literal concise generator",
        body: function () {
            var proto = {
                method() {  return 42; }
            };

            var object = {
                *g() {  yield super.method();  }
            };

            Object.setPrototypeOf(object, proto);

            assert.areEqual(42, object.g().next().value,
                "The value of `object.g().next().value` is `42`, after executing `Object.setPrototypeOf(object, proto);`"
            );
        }
    },
    // TODO: Test yield in expression positions of control flow constructs, e.g. initializer, condition, and increment of a for loop
];

testRunner.runTests(tests, { verbose: WScript.Arguments[0] != "summary" });
