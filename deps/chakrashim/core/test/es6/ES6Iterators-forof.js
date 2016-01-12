//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// ES6 Iterators for-of loop tests -- verifies the for-of loop behavior

WScript.LoadScriptFile("..\\UnitTestFramework\\UnitTestFramework.js");

var emptyIterator = {
    [Symbol.iterator]: function () {
        return {
            next: function () {
                return {
                    done: true,
                    value: 0
                };
            }
        };
    }
};

var simpleIterator = {
    [Symbol.iterator]: function () {
        return {
            i: 0,
            next: function () {
                return {
                    done: this.i == 3,
                    value: this.i++
                };
            }
        };
    }
};

var infiniteIterator = {
    [Symbol.iterator]: function () {
        return {
            i: 0,
            next: function () {
                return {
                    done: false,
                    value: this.i++
                };
            }
        };
    }
};

var tests = [
    {
        name: "for (var ... form declares a variable at function scope and assigns it every iteration",
        body: function () {
            var i = 0;
            var a = [];
            assert.areEqual(undefined, x, "x is available before for-of loop and is initialized with undefined");

            for (var x of simpleIterator) {
                a.push(x);
                assert.areEqual(i++, x, "x is available within the for-of loop and has a new value each iteration");
            }

            assert.areEqual([0, 1, 2], a, "verify the correct number of iterations occurred");

            assert.areEqual(2, x, "x is still available after for-of loop and has the last iteration's value");
        }
    },
    {
        name: "for (let ... form declares a variable at loop scope and assigns it every iteration",
        body: function () {
            var i = 0;
            var a = [];
            assert.throws(function () { eval('x'); }, ReferenceError, "x is not available before for-of loop", "'x' is undefined");

            for (let x of simpleIterator) {
                a.push(x);
                assert.areEqual(i++, x, "x is available within the for-of loop and has a new value each iteration");

                x = 5;
                assert.areEqual(5, x, "x can be reassigned; it is not const");
            }

            assert.areEqual([0, 1, 2], a, "verify the correct number of iterations occurred");

            assert.throws(function () { eval('x'); }, ReferenceError, "x is not available after for-of loop", "'x' is undefined");
        }
    },
    {
        name: "for (let/const x of x) is a use before declaration error",
        body: function () {
            assert.throws(function () { eval("for (let x of x) { }"); }, ReferenceError, "for (let x of x) is a use before declaration error", "Use before declaration");
            // TODO: for-in and for-of incorrectly give an uninitialized const error when const is used to declare the variable
            //assert.throws(function () { eval("for (const x of x) { }"); }, ReferenceError, "for (const x of x) is a use before declaration error", "Use before declaration");
        }
    },
    {
        name: "for (const ... form declares a variable at loop scope and assigns it every iteration, disallows assignments to it",
        body: function () {
            // TODO: for-in and for-of incorrectly give an uninitialized const error when const is used to declare the variable
            /*
            var i = 0;
            var a = [];
            assert.throws(function () { eval('x'); }, ReferenceError, "x is not available before for-of loop", "'x' is undefined");

            for (const x of simpleIterator) {
                a.push(x);
                assert.areEqual(i++, x, "x is available within the for-of loop and has a new value each iteration");

                assert.throws(function () { eval("x = 5"); }, ReferenceError, "x cannot be reassigned because it is const", "Assignment to const");
            }

            assert.areEqual([0, 1, 2], a, "verify the correct number of iterations occurred");

            assert.throws(function () { eval('x'); }, ReferenceError, "x is not available after for-of loop", "'x' is undefined");
            */
       }
    },
    {
        name: "for (<identifer> ... form does not declare a variable and assigns to <identifier> every iteration",
        body: function () {
            var i = 0;
            var a = [];
            var x;

            for (x of simpleIterator) {
                a.push(x);
                assert.areEqual(i++, x, "x is available within the for-of loop and has a new value each iteration");
            }

            assert.areEqual([0, 1, 2], a, "verify the correct number of iterations occurred");

            assert.areEqual(2, x, "x has the last iteration's value");


            assert.throws(
                function () {
                    "use strict";
                    for (y of simpleIterator) { }
                },
                ReferenceError,
                "for (<identifier> ... form does not declare a new variable and thus in strict mode throws if the identifier is not already defined",
                "Variable undefined in strict mode");
        }
    },
    {
        name: "for (<expr> ... form does not declare a variable and evaluates and assigns to <expr> every iteration",
        body: function () {
            var i = 0;
            var a = [];
            var callCount = 0;
            var o = {
                i: 0,
                f: function () {
                    callCount++;
                    return this;
                }
            };

            for (o.f().i of simpleIterator) {
                a.push(o.i);
                assert.areEqual(i++, o.i, "o.i is assigned each value of the iterator");
            }

            assert.areEqual([0, 1, 2], a, "verify the correct number of iterations occurred");
            assert.areEqual(3, callCount, "verify that o.f() was called once for each iteration");
            assert.areEqual(2, o.i, "o.i still has the last value of the last iteration");
        }
    },
    {
        name: "for-of does not execute body for iterator that is initially complete",
        body: function () {
            for (let x of emptyIterator) {
                assert.fail("loop body should not execute");
            }
        }
    },
    {
        name: "infinite iterators are no different, break and continue work correctly",
        body: function () {
            var iterationCount = 0;
            var nonContinueCount = 0;

            for (var x of infiniteIterator) {
                iterationCount++;

                if (x == 3 || x == 5 || x == 7)
                    continue;

                nonContinueCount++;

                if (x == 9)
                    break;
            }

            assert.areEqual(10, iterationCount, "loop iterated 10 times");
            assert.areEqual(7, nonContinueCount, "loop continued 3 times");
            assert.areEqual(9, x, "x has last iteration's value");
        }
    },
    {
        name: "for-of does not execute loop body for null and undefined collections (i.e. before trying to call @@iterator)",
        body: function () {
            for (let x of null) {
                assert.fail("loop body should not execute for null iterator");
            }

            for (let x of undefined) {
                assert.fail("loop body should not execute for undefined iterator");
            }
        }
    },
    {
        name: "for-of throws TypeError for non-object iterator values",
        body: function () {
            var iteratorReturnsNull = { [Symbol.iterator]: function () { return null; } };
            var iteratorReturnsUndefined = { [Symbol.iterator]: function () { return undefined; } };
            var iteratorReturnsBoolean = { [Symbol.iterator]: function () { return true; } };
            var iteratorReturnsNumber = { [Symbol.iterator]: function () { return 10; } };
            var iteratorReturnsString = { [Symbol.iterator]: function () { return "hello"; } };
            var iteratorReturnsSymbol = { [Symbol.iterator]: function () { return Symbol(); } };

            assert.throws(function () { for (let x of iteratorReturnsNull) { } }, TypeError, "for-of throws when @@iterator returns non-object; null", "Object expected");
            assert.throws(function () { for (let x of iteratorReturnsUndefined) { } }, TypeError, "for-of throws when @@iterator returns non-object; undefined", "Object expected");
            assert.throws(function () { for (let x of iteratorReturnsBoolean) { } }, TypeError, "for-of throws when @@iterator returns non-object; boolean", "Object expected");
            assert.throws(function () { for (let x of iteratorReturnsNumber) { } }, TypeError, "for-of throws when @@iterator returns non-object; number", "Object expected");
            assert.throws(function () { for (let x of iteratorReturnsString) { } }, TypeError, "for-of throws when @@iterator returns non-object; string", "Object expected");
            assert.throws(function () { for (let x of iteratorReturnsSymbol) { } }, TypeError, "for-of throws when @@iterator returns non-object; symbol", "Object expected");
        }
    },
    {
        name: "for-of throws TypeError for collection expressions that do not have an @@iterator method",
        body: function () {
            assert.throws(function () { for (let x of { }) { } }, TypeError, "for-of throws when the collection object does not have an @@iterator property", "Object doesn't support property or method 'Symbol.iterator'");
            assert.throws(function () { for (let x of { [Symbol.iterator]: { } }) { } }, TypeError, "for-of throws when the collection object has an @@iterator property but the value is not a function", "Function expected");
            assert.throws(function () { for (let x of 0) { } }, TypeError, "for-of throws when the collection object does not have an @@iterator property; number literal", "Object doesn't support property or method 'Symbol.iterator'");
        }
    },
    {
        name: "for-of behavior when the special named properties are missing or iterator result is not an object",
        body: function () {
            var iteratorNoNextMethod = { [Symbol.iterator]: function () { return { }; } };
            var iteratorResultNotObject = { [Symbol.iterator]: function () { return { next: function () { return undefined; } }; } };
            var iteratorNoDoneProperty = { [Symbol.iterator]: function () { return { next: function () { return { }; } }; } };
            var iteratorNoValuePropertyWhenDoneIsFalse = { [Symbol.iterator]: function () { return { next: function () { return { done: false }; } }; } };

            assert.throws(function () { for (let x of iteratorNoNextMethod) { } }, TypeError, "for-of throws TypeError if the iterator object does not have a next method", "Object doesn't support property or method 'next'");
            assert.throws(function () { for (let x of iteratorResultNotObject) { } }, TypeError, "for-of throws TypeError if the iterator object whose next method does not return an object", "Object expected");

            for (let x of iteratorNoDoneProperty) {
                // infinite loop because no done property produces undefined which ToBooleans to false
                assert.areEqual(undefined, x, "x gets undefined because value is not defined on the iterator result");
                break;
            }

            for (let x of iteratorNoValuePropertyWhenDoneIsFalse) {
                assert.areEqual(undefined, x, "x gets undefined because value is not defined on the iterator result");
                break;
            }
        }
    },
    {
        name: "for-of parsing with interesting combinations of the pseudo keywords 'let' and 'of'",
        body: function () {
            var a = [];
            var i = 0;
            for (let of of simpleIterator) {
                a.push(of);
                assert.areEqual(i++, of, "of is the local loop variable of a for-of loop");
            }
            assert.areEqual([0, 1, 2], a, "verify the correct number of iterations occurred");

            a = [];
            i = 0;
            for (let of in { a0: 0, a1: 0, a2: 0 }) {
                a.push(of);
                assert.areEqual('a' + i++, of, "of is the local loop variable of a for-in loop");
            }
            assert.areEqual(['a0', 'a1', 'a2'], a, "verify the correct number of iterations occurred");

            // These two cases would be ambiguous if the spec allowed 'let' to start
            // the LHSExpression of the LHSExpression form of for-of loops
            //
            // for (let of of [0]) { }
            // for (let of of ([0])) { }
            a = [];
            i = 0;
            for (let of of [0]) {
                a.push(of);
                assert.areEqual(i++, of, "of is the local loop variable of a for-of loop with array literal object expression");
            }
            assert.areEqual([0], a, "verify the correct number of iterations occurred");

            a = [];
            i = 0;
            for (let of of ([0])) {
                a.push(of);
                assert.areEqual(i++, of, "of is the local loop variable of a for-of loop with parenthesized array literal object expression");
            }
            assert.areEqual([0], a, "verify the correct number of iterations occurred");


            // Make sure for loop still works using let to declare of variable
            for (let of; false; ) { }
            for (let of, bar; false; ) { }
            for (let of = 10; false; ) { }

            // 'let' cannot be used as an identifier name at the beginning of the LHSExpression of a for-of
            // and thus 'for (let of' always parses as a let declaration of a variable named 'of'.
            assert.throws(function () { eval('for (let of simpleIterator) { }'); }, SyntaxError, "'for (let of' is always parsed such that it is a let declaration of a variable named 'of'");
            assert.throws(function () { eval('for (let of of) { }'); }, SyntaxError, "'for (let of' is always parsed such that it is a let declaration of a variable named 'of'");

            // Should still be able to use let identifier in LHSExpression via parentheses
            a = [];
            i = 0;
            var let;
            for ((let) of simpleIterator) {
                a.push(let);
                assert.areEqual(i++, let, "let is the loop variable of a for-of loop, declared outside the for-of loop");
            }
            assert.areEqual([0, 1, 2], a, "verify the correct number of iterations occurred");
            assert.areEqual(2, let, "let is equal to the last iteration value");
        }
    },
    {
        name: "break and continue work correctly with nested and labels",
        body: function () {
            var iterationCount = 0;
            var nonContinueCount = 0;

            while (true) {
                for (var x of infiniteIterator) {
                    iterationCount++;

                    if (x == 3 || x == 5 || x == 7)
                        continue;

                    nonContinueCount++;

                    if (x == 9)
                        break;
                }

                // non-trivialize the while(true) loop's break
                // to avoid optimizing away the while loop.
                if (infiniteIterator)
                    break;
            }

            assert.areEqual(10, iterationCount, "loop iterated 10 times");
            assert.areEqual(7, nonContinueCount, "loop continued 3 times");
            assert.areEqual(9, x, "x has last iteration's value");

            var outerCount = 0;
            var innerCount = 0;

            outer: for (var x of infiniteIterator) {
                outerCount++;

                for (var y of infiniteIterator) {
                    innerCount++;

                    if (x + y === 0)
                        break;

                    if (x + y === 1)
                        continue;

                    if (y === 1)
                        continue outer;

                    if (x === 2)
                        break outer;
                }
            }

            assert.areEqual(3, outerCount, "breaks and continues lead outer loop to execute 3 times");
            assert.areEqual(4, innerCount, "breaks and continues lead inner loop to execute 4 times");

            var aCount = 0;
            var bCount = 0;
            var cCount = 0;
            do {
                aCount++;
                for (var x of infiniteIterator) {
                    bCount++;
                    for (var i = 0; true; i++) {
                        cCount++;
                        break;
                    }
                    break;
                }
                break;
            } while (true);

            assert.areEqual(1, aCount, "do-while executes once");
            assert.areEqual(1, bCount, "for-of executes once");
            assert.areEqual(1, cCount, "for executes once");
        }
    },
];

testRunner.runTests(tests, { verbose: WScript.Arguments[0] != "summary" });
