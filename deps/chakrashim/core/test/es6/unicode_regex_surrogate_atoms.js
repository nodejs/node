//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

Object.defineProperty(Object.getPrototypeOf({}), "echo", { value: function () { WScript.Echo(this); } });
function AssertEqual(actual, expected, msg) { ((actual === expected ? "Passed! " : "Failed (actual: " + actual + ", expected: " + expected + "). Message: ") + msg).echo(); };
Object.defineProperty(Object.getPrototypeOf({}), "equalTo", { value: function (other, msg) { AssertEqual(this.constructor(this), other, msg); } });

//Basic test, true
/a\u{20BB7}*b/u.test("a\uD842\uDFB7\uD842\uDFB7b").echo();

//Should be true
/a\u{20BB7}*\uD800\uDC00*b/u.test("a\uD842\uDFB7\u{20BB7}\uD800\uDC00\uD800\uDC00b").echo();

//Should be error(no unicode option)
try {
    eval("/a\\u{20BB7}*\\uD800\\uDC00*b/").test("a\uD842\uDFB7\u{DFB7}\uD800\uDC00\uDC00b").echo();
} catch (ex) {
    ex.echo();
}

//The case where a lookback is incorrect due to a group being added.
/a\uD800\uDC00*(\u{20BB7})\1b/u.test("a\u{20BB7}\u{20BB7}b").echo();

//These are interesting, they shouldn't combine, but without quantifier should be true
/a\uD800(\uDC00)b/u.test("a\uD800\uDC00b").echo();
/a\uD800[\uDC00]b/u.test("a\uD800\uDC00b").echo();

//Reverse of above, with a quantifier should be true
/a\uD800(\uDC00)*b/u.test("a\uD800\uDC00\uDC00b").echo();
/a\uD800[\uDC00]*b/u.test("a\uD800\uDC00\uDC00b").echo();

//True for ranges
/a[\u{20BB7}]b/u.test("a\uD842\uDFB7b").echo();
/a[\u0000 - \u{20BB7}]b/u.test("a\uD842\uDFB7b").echo();
/a[\u0000-\u{20BB7}]b/u.test("a\uFFFFb").echo();
/a[\u0000-\u{20BB7}]b/u.test("a\uD900b").echo();
/a[\u0000-\u{20BB7}]b/u.test("a\u{10000}b").echo();
/a[\u0000-\u{20BB7}]b/u.test("a\u{20BB7}b").echo();
/a[\u0000-\u{20400}]b/u.test("a\u{20400}b").echo();
/a[\u0000-\u{203FF}]b/u.test("a\u{203FF}b").echo();
/a[\u0000-\u{20BB7}]b/u.test("a\u{0000}b").echo();
/a.b/u.test("a\uD900b").echo();

//False for ranges
/a[\u0000-\u{20BB7}]b/u.test("a\u{20BB8}b").echo();
/a[\u1000-\u{20BB7}]b/u.test("a\u{0000}b").echo();
