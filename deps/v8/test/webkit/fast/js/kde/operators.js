// Copyright 2013 the V8 project authors. All rights reserved.
// Copyright (C) 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 1.  Redistributions of source code must retain the above copyright
//     notice, this list of conditions and the following disclaimer.
// 2.  Redistributions in binary form must reproduce the above copyright
//     notice, this list of conditions and the following disclaimer in the
//     documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
// DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
// ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

description("KDE JS Test");
function nonSpeculativeNotInner(argument, o1, o2)
{
    // The + operator on objects is a reliable way to avoid the speculative JIT path for now at least.
    o1 + o2;
    return !argument;
}
function nonSpeculativeNot(argument)
{
    return nonSpeculativeNotInner(argument, {}, {});
}

function nonSpeculativeLessInner(a, b, o1, o2)
{
    // The + operator on objects is a reliable way to avoid the speculative JIT path for now at least.
    o1 + o2;
    return a < b;
}
function nonSpeculativeLess(a, b)
{
    return nonSpeculativeLessInner(a, b, {}, {});
}

function nonSpeculativeLessEqInner(a, b, o1, o2)
{
    // The + operator on objects is a reliable way to avoid the speculative JIT path for now at least.
    o1 + o2;
    return a <= b;
}
function nonSpeculativeLessEq(a, b)
{
    return nonSpeculativeLessEqInner(a, b, {}, {});
}

function nonSpeculativeGreaterInner(a, b, o1, o2)
{
    // The + operator on objects is a reliable way to avoid the speculative JIT path for now at least.
    o1 + o2;
    return a > b;
}
function nonSpeculativeGreater(a, b)
{
    return nonSpeculativeGreaterInner(a, b, {}, {});
}

function nonSpeculativeGreaterEqInner(a, b, o1, o2)
{
    // The + operator on objects is a reliable way to avoid the speculative JIT path for now at least.
    o1 + o2;
    return a >= b;
}
function nonSpeculativeGreaterEq(a, b)
{
    return nonSpeculativeGreaterEqInner(a, b, {}, {});
}

function nonSpeculativeEqualInner(a, b, o1, o2)
{
    // The + operator on objects is a reliable way to avoid the speculative JIT path for now at least.
    o1 + o2;
    return a == b;
}
function nonSpeculativeEqual(a, b)
{
    return nonSpeculativeEqualInner(a, b, {}, {});
}

function nonSpeculativeNotEqualInner(a, b, o1, o2)
{
    // The + operator on objects is a reliable way to avoid the speculative JIT path for now at least.
    o1 + o2;
    return a != b;
}
function nonSpeculativeNotEqual(a, b)
{
    return nonSpeculativeNotEqualInner(a, b, {}, {});
}

function nonSpeculativeStrictEqualInner(a, b, o1, o2)
{
    // The + operator on objects is a reliable way to avoid the speculative JIT path for now at least.
    o1 + o2;
    return a === b;
}
function nonSpeculativeStrictEqual(a, b)
{
    return nonSpeculativeStrictEqualInner(a, b, {}, {});
}

function nonSpeculativeStrictNotEqualInner(a, b, o1, o2)
{
    // The + operator on objects is a reliable way to avoid the speculative JIT path for now at least.
    o1 + o2;
    return a !== b;
}
function nonSpeculativeStrictNotEqual(a, b)
{
    return nonSpeculativeStrictNotEqualInner(a, b, {}, {});
}

// operator !
shouldBeTrue("!undefined");
shouldBeTrue("!null");
shouldBeTrue("!!true");
shouldBeTrue("!false");
shouldBeTrue("!!1");
shouldBeTrue("!0");
shouldBeTrue("!!'a'");
shouldBeTrue("!''");

shouldBeTrue("nonSpeculativeNot(undefined)");
shouldBeTrue("nonSpeculativeNot(null)");
shouldBeTrue("nonSpeculativeNot(!true)");
shouldBeTrue("nonSpeculativeNot(false)");
shouldBeTrue("nonSpeculativeNot(!1)");
shouldBeTrue("nonSpeculativeNot(0)");
shouldBeTrue("nonSpeculativeNot(!'a')");
shouldBeTrue("nonSpeculativeNot('')");

// unary plus
shouldBe("+9", "9");
shouldBe("var i = 10; +i", "10");

// negation
shouldBe("-11", "-11");
shouldBe("var i = 12; -i", "-12");

// increment
shouldBe("var i = 0; ++i;", "1");
shouldBe("var i = 0; ++i; i", "1");
shouldBe("var i = 0; i++;", "0");
shouldBe("var i = 0; i++; i", "1");
shouldBe("var i = true; i++", "1");
shouldBe("var i = true; i++; i", "2");

// decrement
shouldBe("var i = 0; --i;", "-1");
shouldBe("var i = 0; --i; i", "-1");
shouldBe("var i = 0; i--;", "0");
shouldBe("var i = 0; i--; i", "-1");
shouldBe("var i = true; i--", "1");
shouldBe("var i = true; i--; i", "0");

// bitwise operators
shouldBe("~0", "-1");
shouldBe("~1", "-2");
shouldBe("~NaN", "-1");
shouldBe("~Infinity", "-1");
shouldBe("~Math.pow(2, 33)", "-1"); // 32 bit overflow
shouldBe("~(Math.pow(2, 32) + Math.pow(2, 31) + 2)",
         "2147483645"); // a signedness issue
shouldBe("~null", "-1");
shouldBe("3 & 1", "1");
shouldBe("2 | true", "3");
shouldBe("'3' ^ 1", "2");
shouldBe("3^4&5", "7");
shouldBe("2|4^5", "3");

shouldBe("1 << 2", "4");
shouldBe("8 >> 1", "4");
shouldBe("1 >> 2", "0");
shouldBe("-8 >> 24", "-1");
shouldBe("8 >>> 2", "2");
shouldBe("-8 >>> 24", "255");
shouldBe("(-2200000000 >> 1) << 1", "2094967296");
shouldBe("Infinity >> 1", "0");
shouldBe("Infinity << 1", "0");
shouldBe("Infinity >>> 1", "0");
shouldBe("NaN >> 1", "0");
shouldBe("NaN << 1", "0");
shouldBe("NaN >>> 1", "0");
shouldBe("8.1 >> 1", "4");
shouldBe("8.1 << 1", "16");
shouldBe("8.1 >>> 1", "4");
shouldBe("8.9 >> 1", "4");
shouldBe("8.9 << 1", "16");
shouldBe("8.9 >>> 1", "4");
shouldBe("Math.pow(2, 32) >> 1", "0");
shouldBe("Math.pow(2, 32) << 1", "0");
shouldBe("Math.pow(2, 32) >>> 1", "0");

// Try shifting by variables, to test non-constant-folded cases.
var one = 1;
var two = 2;
var twentyFour = 24;

shouldBe("1 << two", "4");
shouldBe("8 >> one", "4");
shouldBe("1 >> two", "0");
shouldBe("-8 >> twentyFour", "-1");
shouldBe("8 >>> two", "2");
shouldBe("-8 >>> twentyFour", "255");
shouldBe("(-2200000000 >> one) << one", "2094967296");
shouldBe("Infinity >> one", "0");
shouldBe("Infinity << one", "0");
shouldBe("Infinity >>> one", "0");
shouldBe("NaN >> one", "0");
shouldBe("NaN << one", "0");
shouldBe("NaN >>> one", "0");
shouldBe("888.1 >> one", "444");
shouldBe("888.1 << one", "1776");
shouldBe("888.1 >>> one", "444");
shouldBe("888.9 >> one", "444");
shouldBe("888.9 << one", "1776");
shouldBe("888.9 >>> one", "444");
shouldBe("Math.pow(2, 32) >> one", "0");
shouldBe("Math.pow(2, 32) << one", "0");
shouldBe("Math.pow(2, 32) >>> one", "0");

// addition
shouldBe("1+2", "3");
shouldBe("'a'+'b'", "'ab'");
shouldBe("'a'+2", "'a2'");
shouldBe("'2'+'-1'", "'2-1'");
shouldBe("true+'a'", "'truea'");
shouldBe("'a' + null", "'anull'");
shouldBe("true+1", "2");
shouldBe("false+null", "0");

// subtraction
shouldBe("1-3", "-2");
shouldBe("isNaN('a'-3)", "true");
shouldBe("'3'-'-1'", "4");
shouldBe("'4'-2", "2");
shouldBe("true-false", "1");
shouldBe("false-1", "-1");
shouldBe("null-true", "-1");

// multiplication
shouldBe("2 * 3", "6");
shouldBe("true * 3", "3");
shouldBe("2 * '3'", "6");

// division
shouldBe("6 / 4", "1.5");
//shouldBe("true / false", "Inf");
shouldBe("'6' / '2'", "3");
shouldBeTrue("isNaN('x' / 1)");
shouldBeTrue("isNaN(1 / NaN)");
shouldBeTrue("isNaN(Infinity / Infinity)");
shouldBe("Infinity / 0", "Infinity");
shouldBe("-Infinity / 0", "-Infinity");
shouldBe("Infinity / 1", "Infinity");
shouldBe("-Infinity / 1", "-Infinity");
shouldBeTrue("1 / Infinity == +0");
shouldBeTrue("1 / -Infinity == -0"); // how to check ?
shouldBeTrue("isNaN(0/0)");
shouldBeTrue("0 / 1 === 0");
shouldBeTrue("0 / -1 === -0"); // how to check ?
shouldBe("1 / 0", "Infinity");
shouldBe("-1 / 0", "-Infinity");

// modulo
shouldBe("6 % 4", "2");
shouldBe("'-6' % 4", "-2");

shouldBe("2==2", "true");
shouldBe("1==2", "false");

shouldBe("nonSpeculativeEqual(2,2)", "true");
shouldBe("nonSpeculativeEqual(1,2)", "false");

shouldBe("1<2", "true");
shouldBe("1<=2", "true");
shouldBe("2<1", "false");
shouldBe("2<=1", "false");

shouldBe("nonSpeculativeLess(1,2)", "true");
shouldBe("nonSpeculativeLessEq(1,2)", "true");
shouldBe("nonSpeculativeLess(2,1)", "false");
shouldBe("nonSpeculativeLessEq(2,1)", "false");

shouldBe("2>1", "true");
shouldBe("2>=1", "true");
shouldBe("1>=2", "false");
shouldBe("1>2", "false");

shouldBe("nonSpeculativeGreater(2,1)", "true");
shouldBe("nonSpeculativeGreaterEq(2,1)", "true");
shouldBe("nonSpeculativeGreaterEq(1,2)", "false");
shouldBe("nonSpeculativeGreater(1,2)", "false");

shouldBeTrue("'abc' == 'abc'");
shouldBeTrue("'abc' != 'xyz'");
shouldBeTrue("true == true");
shouldBeTrue("false == false");
shouldBeTrue("true != false");
shouldBeTrue("'a' != null");
shouldBeTrue("'a' != undefined");
shouldBeTrue("null == null");
shouldBeTrue("null == undefined");
shouldBeTrue("undefined == undefined");
shouldBeTrue("NaN != NaN");
shouldBeTrue("true != undefined");
shouldBeTrue("true != null");
shouldBeTrue("false != undefined");
shouldBeTrue("false != null");
shouldBeTrue("'0' == 0");
shouldBeTrue("1 == '1'");
shouldBeTrue("NaN != NaN");
shouldBeTrue("NaN != 0");
shouldBeTrue("NaN != undefined");
shouldBeTrue("true == 1");
shouldBeTrue("true != 2");
shouldBeTrue("1 == true");
shouldBeTrue("false == 0");
shouldBeTrue("0 == false");

shouldBeTrue("nonSpeculativeEqual('abc', 'abc')");
shouldBeTrue("nonSpeculativeNotEqual('abc', 'xyz')");
shouldBeTrue("nonSpeculativeEqual(true, true)");
shouldBeTrue("nonSpeculativeEqual(false, false)");
shouldBeTrue("nonSpeculativeNotEqual(true, false)");
shouldBeTrue("nonSpeculativeNotEqual('a', null)");
shouldBeTrue("nonSpeculativeNotEqual('a', undefined)");
shouldBeTrue("nonSpeculativeEqual(null, null)");
shouldBeTrue("nonSpeculativeEqual(null, undefined)");
shouldBeTrue("nonSpeculativeEqual(undefined, undefined)");
shouldBeTrue("nonSpeculativeNotEqual(NaN, NaN)");
shouldBeTrue("nonSpeculativeNotEqual(true, undefined)");
shouldBeTrue("nonSpeculativeNotEqual(true, null)");
shouldBeTrue("nonSpeculativeNotEqual(false, undefined)");
shouldBeTrue("nonSpeculativeNotEqual(false, null)");
shouldBeTrue("nonSpeculativeEqual('0', 0)");
shouldBeTrue("nonSpeculativeEqual(1, '1')");
shouldBeTrue("nonSpeculativeNotEqual(NaN, NaN)");
shouldBeTrue("nonSpeculativeNotEqual(NaN, 0)");
shouldBeTrue("nonSpeculativeNotEqual(NaN, undefined)");
shouldBeTrue("nonSpeculativeEqual(true, 1)");
shouldBeTrue("nonSpeculativeNotEqual(true, 2)");
shouldBeTrue("nonSpeculativeEqual(1, true)");
shouldBeTrue("nonSpeculativeEqual(false, 0)");
shouldBeTrue("nonSpeculativeEqual(0, false)");

shouldBe("'abc' < 'abx'", "true");
shouldBe("'abc' < 'abcd'", "true");
shouldBe("'abc' < 'abc'", "false");
shouldBe("'abcd' < 'abcd'", "false");
shouldBe("'abx' < 'abc'", "false");

shouldBe("nonSpeculativeLess('abc', 'abx')", "true");
shouldBe("nonSpeculativeLess('abc', 'abcd')", "true");
shouldBe("nonSpeculativeLess('abc', 'abc')", "false");
shouldBe("nonSpeculativeLess('abcd', 'abcd')", "false");
shouldBe("nonSpeculativeLess('abx', 'abc')", "false");

shouldBe("'abc' <= 'abc'", "true");
shouldBe("'abc' <= 'abx'", "true");
shouldBe("'abx' <= 'abc'", "false");
shouldBe("'abcd' <= 'abc'", "false");
shouldBe("'abc' <= 'abcd'", "true");

shouldBe("nonSpeculativeLessEq('abc', 'abc')", "true");
shouldBe("nonSpeculativeLessEq('abc', 'abx')", "true");
shouldBe("nonSpeculativeLessEq('abx', 'abc')", "false");
shouldBe("nonSpeculativeLessEq('abcd', 'abc')", "false");
shouldBe("nonSpeculativeLessEq('abc', 'abcd')", "true");

shouldBe("'abc' > 'abx'", "false");
shouldBe("'abc' > 'abc'", "false");
shouldBe("'abcd' > 'abc'", "true");
shouldBe("'abx' > 'abc'", "true");
shouldBe("'abc' > 'abcd'", "false");

shouldBe("nonSpeculativeGreater('abc', 'abx')", "false");
shouldBe("nonSpeculativeGreater('abc', 'abc')", "false");
shouldBe("nonSpeculativeGreater('abcd', 'abc')", "true");
shouldBe("nonSpeculativeGreater('abx', 'abc')", "true");
shouldBe("nonSpeculativeGreater('abc', 'abcd')", "false");

shouldBe("'abc' >= 'abc'", "true");
shouldBe("'abcd' >= 'abc'", "true");
shouldBe("'abx' >= 'abc'", "true");
shouldBe("'abc' >= 'abx'", "false");
shouldBe("'abc' >= 'abx'", "false");
shouldBe("'abc' >= 'abcd'", "false");

shouldBe("nonSpeculativeGreaterEq('abc', 'abc')", "true");
shouldBe("nonSpeculativeGreaterEq('abcd', 'abc')", "true");
shouldBe("nonSpeculativeGreaterEq('abx', 'abc')", "true");
shouldBe("nonSpeculativeGreaterEq('abc', 'abx')", "false");
shouldBe("nonSpeculativeGreaterEq('abc', 'abx')", "false");
shouldBe("nonSpeculativeGreaterEq('abc', 'abcd')", "false");

// mixed strings and numbers - results validated in NS+moz+IE5
shouldBeFalse("'abc' <= 0"); // #35246
shouldBeTrue("'' <= 0");
shouldBeTrue("' ' <= 0");
shouldBeTrue("null <= 0");
shouldBeFalse("0 <= 'abc'");
shouldBeTrue("0 <= ''");
shouldBeTrue("0 <= null");
shouldBeTrue("null <= null");
shouldBeTrue("6 < '52'");
shouldBeTrue("6 < '72'"); // #36087
shouldBeFalse("NaN < 0");
shouldBeFalse("NaN <= 0");
shouldBeFalse("NaN > 0");
shouldBeFalse("NaN >= 0");

shouldBeFalse("nonSpeculativeLessEq('abc', 0)"); // #35246
shouldBeTrue("nonSpeculativeLessEq('', 0)");
shouldBeTrue("nonSpeculativeLessEq(' ', 0)");
shouldBeTrue("nonSpeculativeLessEq(null, 0)");
shouldBeFalse("nonSpeculativeLessEq(0, 'abc')");
shouldBeTrue("nonSpeculativeLessEq(0, '')");
shouldBeTrue("nonSpeculativeLessEq(0, null)");
shouldBeTrue("nonSpeculativeLessEq(null, null)");
shouldBeTrue("nonSpeculativeLess(6, '52')");
shouldBeTrue("nonSpeculativeLess(6, '72')"); // #36087
shouldBeFalse("nonSpeculativeLess(NaN, 0)");
shouldBeFalse("nonSpeculativeLessEq(NaN, 0)");
shouldBeFalse("nonSpeculativeGreater(NaN, 0)");
shouldBeFalse("nonSpeculativeGreaterEq(NaN, 0)");

// strict comparison ===
shouldBeFalse("0 === false");
//shouldBe("undefined === undefined", "true"); // aborts in IE5 (undefined is not defined ;)
shouldBeTrue("null === null");
shouldBeFalse("NaN === NaN");
shouldBeTrue("0.0 === 0");
shouldBeTrue("'abc' === 'abc'");
shouldBeFalse("'a' === 'x'");
shouldBeFalse("1 === '1'");
shouldBeFalse("'1' === 1");
shouldBeTrue("true === true");
shouldBeTrue("false === false");
shouldBeFalse("true === false");
shouldBeTrue("Math === Math");
shouldBeFalse("Math === Boolean");
shouldBeTrue("Infinity === Infinity");

// strict comparison ===
shouldBeFalse("nonSpeculativeStrictEqual(0, false)");
//shouldBe("undefined === undefined", "true"); // aborts in IE5 (undefined is not defined ;)
shouldBeTrue("nonSpeculativeStrictEqual(null, null)");
shouldBeFalse("nonSpeculativeStrictEqual(NaN, NaN)");
shouldBeTrue("nonSpeculativeStrictEqual(0.0, 0)");
shouldBeTrue("nonSpeculativeStrictEqual('abc', 'abc')");
shouldBeFalse("nonSpeculativeStrictEqual('a', 'x')");
shouldBeFalse("nonSpeculativeStrictEqual(1, '1')");
shouldBeFalse("nonSpeculativeStrictEqual('1', 1)");
shouldBeTrue("nonSpeculativeStrictEqual(true, true)");
shouldBeTrue("nonSpeculativeStrictEqual(false, false)");
shouldBeFalse("nonSpeculativeStrictEqual(true, false)");
shouldBeTrue("nonSpeculativeStrictEqual(Math, Math)");
shouldBeFalse("nonSpeculativeStrictEqual(Math, Boolean)");
shouldBeTrue("nonSpeculativeStrictEqual(Infinity, Infinity)");

// !==
shouldBe("0 !== 0", "false");
shouldBe("0 !== 1", "true");

// !==
shouldBe("nonSpeculativeStrictNotEqual(0, 0)", "false");
shouldBe("nonSpeculativeStrictNotEqual(0, 1)", "true");

shouldBe("typeof undefined", "'undefined'");
shouldBe("typeof null", "'object'");
shouldBe("typeof true", "'boolean'");
shouldBe("typeof false", "'boolean'");
shouldBe("typeof 1", "'number'");
shouldBe("typeof 'a'", "'string'");
shouldBe("typeof shouldBe", "'function'");
shouldBe("typeof Number.NaN", "'number'");

shouldBe("11 && 22", "22");
shouldBe("null && true", "null");
shouldBe("11 || 22", "11");
shouldBe("null || 'a'", "'a'");

shouldBeUndefined("void 1");

shouldBeTrue("1 in [1, 2]");
shouldBeFalse("3 in [1, 2]");
shouldBeTrue("'a' in { a:1, b:2 }");

// instanceof
// Those 2 lines don't parse in Netscape...
shouldBe("(new Boolean()) instanceof Boolean", "true");
shouldBe("(new Boolean()) instanceof Number", "false");
