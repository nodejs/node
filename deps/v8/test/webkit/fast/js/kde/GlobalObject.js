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
var h = "a\xefc";
var u = "a\u1234c";
var z = "\x00";

shouldBe("h.charCodeAt(1)", "239");
shouldBe("u.charCodeAt(1)", "4660");
shouldBe("escape(h)", "'a%EFc'");
shouldBe("escape(u)", "'a%u1234c'");
shouldBe("escape(z)", "'%00'");
shouldBe("unescape(escape(h))", "h");
shouldBe("unescape(escape(u))", "u");
shouldBe("unescape(escape(z))", "z");

shouldBeTrue("isNaN(NaN)");
shouldBeTrue("isNaN('NaN')");
shouldBeFalse("isNaN('1')");

shouldBeTrue("isFinite(1)");
shouldBeTrue("isFinite('1')");

// all should return NaN because 1st char is non-number
shouldBeFalse("isFinite('a')");
shouldBe('isNaN(parseInt("Hello", 8))', "true");
shouldBe('isNaN(parseInt("FFF", 10))', "true");
shouldBe('isNaN(parseInt(".5", 10))', "true");

shouldBeFalse("isFinite(Infinity)");
shouldBeFalse("isFinite('Infinity')");

shouldBeTrue("isNaN(parseInt())");
shouldBeTrue("isNaN(parseInt(''))");
shouldBeTrue("isNaN(parseInt(' '))");
shouldBeTrue("isNaN(parseInt('a'))");
shouldBe("parseInt(1)", "1");
shouldBe("parseInt(1234567890123456)", "1234567890123456");
shouldBe("parseInt(1.2)", "1");
shouldBe("parseInt(' 2.3')", "2");
shouldBe("parseInt('0x10')", "16");
shouldBe("parseInt('11', 0)", "11");
shouldBe("parseInt('F', 16)", "15");

shouldBeTrue("isNaN(parseInt('10', 40))");
shouldBe("parseInt('3x')", "3");
shouldBe("parseInt('3 x')", "3");
shouldBeTrue("isNaN(parseInt('Infinity'))");

// all should return 15
shouldBe('parseInt("15")', "15");
shouldBe('parseInt("015")', "15"); // ES5 prohibits parseInt from handling octal, see annex E.
shouldBe('parseInt("0xf")', "15");
shouldBe('parseInt("15", 0)', "15");
shouldBe('parseInt("15", 10)', "15");
shouldBe('parseInt("F", 16)', "15");
shouldBe('parseInt("17", 8)', "15");
shouldBe('parseInt("15.99", 10)', "15");
shouldBe('parseInt("FXX123", 16)', "15");
shouldBe('parseInt("1111", 2)', "15");
shouldBe('parseInt("15*3", 10)', "15");

// this should be 0
shouldBe('parseInt("0x7", 10)', "0");
shouldBe('parseInt("1x7", 10)', "1");

shouldBeTrue("isNaN(parseFloat())");
shouldBeTrue("isNaN(parseFloat(''))");
shouldBeTrue("isNaN(parseFloat(' '))");
shouldBeTrue("isNaN(parseFloat('a'))");
shouldBe("parseFloat(1)", "1");
shouldBe("parseFloat(' 2.3')", "2.3");
shouldBe("parseFloat('3.1 x', 3)", "3.1");
shouldBe("parseFloat('3.1x', 3)", "3.1");
shouldBeFalse("isFinite(parseFloat('Infinity'))");
shouldBeFalse("delete NaN");
shouldBeFalse("delete Infinity");
shouldBeFalse("delete undefined");
