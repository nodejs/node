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
var negativeZero = Math.atan2(-1, Infinity); // ### any nicer way?

function isNegativeZero(n)
{
  return n == 0 && 1 / n < 0;
}

// self tests
shouldBeTrue("isNegativeZero(negativeZero)");
shouldBeFalse("isNegativeZero(0)");

// Constants
shouldBe("String()+Math.E", "'2.718281828459045'");
shouldBe("String()+Math.LN2", "'0.6931471805599453'");
shouldBe("String()+Math.LN10", "'2.302585092994046'");
shouldBe("String()+Math.LOG2E", "'1.4426950408889634'");
shouldBe("String()+Math.LOG10E", "'0.4342944819032518'");
shouldBe("String()+Math.PI", "'3.141592653589793'");
shouldBe("String()+Math.SQRT1_2", "'0.7071067811865476'");
shouldBe("String()+Math.SQRT2", "'1.4142135623730951'");

shouldBe("String()+Number.NaN", "'NaN'");
shouldBe("String()+Number.NEGATIVE_INFINITY", "'-Infinity'");
shouldBe("String()+Number.POSITIVE_INFINITY", "'Infinity'");

// Functions
shouldBe("Math.abs(-5)", "5");
shouldBe("Math.acos(0)", "Math.PI/2");
shouldBe("Math.acos(1)", "0");
shouldBe("Math.ceil(1.1)", "2");
shouldBe("String()+Math.sqrt(2)", "String()+Math.SQRT2");
shouldBe("Math.ceil(1.6)", "2");
shouldBe("Math.round(0)", "0");
shouldBeFalse("isNegativeZero(Math.round(0))");
shouldBeTrue("isNegativeZero(Math.round(negativeZero))");
shouldBe("Math.round(0.2)", "0");
shouldBeTrue("isNegativeZero(Math.round(-0.2))");
shouldBeTrue("isNegativeZero(Math.round(-0.5))");
shouldBe("Math.round(1.1)", "1");
shouldBe("Math.round(1.6)", "2");
shouldBe("Math.round(-3.5)", "-3");
shouldBe("Math.round(-3.6)", "-4");
shouldBeTrue("isNaN(Math.round())");
shouldBeTrue("isNaN(Math.round(NaN))");
shouldBe("Math.round(-Infinity)", "-Infinity");
shouldBe("Math.round(Infinity)", "Infinity");
shouldBe("Math.round(99999999999999999999.99)", "100000000000000000000");
shouldBe("Math.round(-99999999999999999999.99)", "-100000000000000000000");

// Math.log()
shouldBe("Math.log(Math.E*Math.E)", "2");
shouldBeTrue("isNaN(Math.log(NaN))");
shouldBeTrue("isNaN(Math.log(-1))");
shouldBeFalse("isFinite(Math.log(0))");
shouldBe("Math.log(1)", "0");
shouldBeFalse("isFinite(Math.log(Infinity))");

// Math.min()
shouldBeTrue("isNegativeZero(Math.min(negativeZero, 0))");

// Math.max()
shouldBeFalse("isFinite(Math.max())");
shouldBe("Math.max(1)", "1"); // NS 4.x and IE 5.x seem to know about 2 arg version only
shouldBe("Math.max(1, 2, 3)", "3"); // NS 4.x and IE 5.x seem to know about 2 arg version only
shouldBeTrue("isNaN(Math.max(1,NaN,3))");
shouldBeTrue("!isNegativeZero(Math.max(negativeZero, 0))");


list=""
for ( var i in Math ) { list += i + ','; }
shouldBe("list","''");

var my = new Object;
my.v = 1;

// Deleting/assigning
shouldBe("delete my.v", "true")
shouldBeUndefined("my.v");
shouldBe("delete Math.PI", "false")
function myfunc( num ) { return num+1; }
shouldBe("my = myfunc, myfunc(4)", "5");

// Conversions
shouldBe("Boolean(Math)", "true");
shouldBeTrue("isNaN(Number(Math));");

// Unicity
shouldBe("Math.abs===Math.abs", "true")
shouldBe("Math.abs===Math.round", "false")

// Iteration
obj = new Object;
obj.a = 1;
obj.b = 2;
list=""
for ( var i in obj ) { list += i + ','; }
shouldBe("list","'a,b,'");

// (check that Math's properties and functions are not enumerable)
list=""
for ( var i in Math ) { list += i + ','; }
shouldBe("list","''");

Math.myprop=true; // adding a custom property to the math object (why not?)
list=""
for ( var i in Math ) { list += i + ','; }
shouldBe("list","'myprop,'");
