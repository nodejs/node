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

description(
    'This test checks a few Number.toFixed cases, including ' +
    '<a href="https://bugs.webkit.org/show_bug.cgi?id=5307">5307: Number.toFixed does not round 0.5 up</a>' +
    ' and ' +
    '<a href="https://bugs.webkit.org/show_bug.cgi?id=5308">5308: Number.toFixed does not include leading zero</a>' +
    '.');

shouldBe("(0).toFixed(0)", "'0'");

shouldBe("(0.49).toFixed(0)", "'0'");
shouldBe("(0.5).toFixed(0)", "'1'");
shouldBe("(0.51).toFixed(0)", "'1'");

shouldBe("(-0.49).toFixed(0)", "'-0'");
shouldBe("(-0.5).toFixed(0)", "'-1'");
shouldBe("(-0.51).toFixed(0)", "'-1'");

shouldBe("(0).toFixed(1)", "'0.0'");

shouldBe("(0.449).toFixed(1)", "'0.4'");
shouldBe("(0.45).toFixed(1)", "'0.5'");
shouldBe("(0.451).toFixed(1)", "'0.5'");
shouldBe("(0.5).toFixed(1)", "'0.5'");
shouldBe("(0.549).toFixed(1)", "'0.5'");
shouldBe("(0.55).toFixed(1)", "'0.6'");
shouldBe("(0.551).toFixed(1)", "'0.6'");

shouldBe("(-0.449).toFixed(1)", "'-0.4'");
shouldBe("(-0.45).toFixed(1)", "'-0.5'");
shouldBe("(-0.451).toFixed(1)", "'-0.5'");
shouldBe("(-0.5).toFixed(1)", "'-0.5'");
shouldBe("(-0.549).toFixed(1)", "'-0.5'");
shouldBe("(-0.55).toFixed(1)", "'-0.6'");
shouldBe("(-0.551).toFixed(1)", "'-0.6'");

var posInf = 1/0;
var negInf = -1/0;
var nan = 0/0;

// From Acid3, http://bugs.webkit.org/show_bug.cgi?id=16640
shouldBeEqualToString("(0.0).toFixed(4)", "0.0000");
shouldBeEqualToString("(-0.0).toFixed(4)", "0.0000");
shouldBeEqualToString("(0.0).toFixed()", "0");
shouldBeEqualToString("(-0.0).toFixed()", "0");

// From http://bugs.webkit.org/show_bug.cgi?id=5258
shouldBeEqualToString("(1234.567).toFixed()", "1235");
shouldBeEqualToString("(1234.567).toFixed(0)", "1235");
// 0 equivilents
shouldBeEqualToString("(1234.567).toFixed(null)", "1235");
shouldBeEqualToString("(1234.567).toFixed(false)", "1235");
shouldBeEqualToString("(1234.567).toFixed('foo')", "1235");
shouldBeEqualToString("(1234.567).toFixed(nan)", "1235"); // nan is treated like 0

shouldBeEqualToString("(1234.567).toFixed(1)", "1234.6");
shouldBeEqualToString("(1234.567).toFixed(true)", "1234.6"); // just like 1
shouldBeEqualToString("(1234.567).toFixed('1')", "1234.6"); // just like 1

shouldBeEqualToString("(1234.567).toFixed(2)", "1234.57");
shouldBeEqualToString("(1234.567).toFixed(2.9)", "1234.57");
shouldBeEqualToString("(1234.567).toFixed(5)", "1234.56700");
shouldBeEqualToString("(1234.567).toFixed(20)", "1234.56700000000000727596");

// SpiderMonkey allows precision values -20 to 100, the spec only allows 0 to 20
shouldThrow("(1234.567).toFixed(21)");
shouldThrow("(1234.567).toFixed(100)");
shouldThrow("(1234.567).toFixed(101)");
shouldThrow("(1234.567).toFixed(-1)");
shouldThrow("(1234.567).toFixed(-4)");
shouldThrow("(1234.567).toFixed(-5)");
shouldThrow("(1234.567).toFixed(-20)");
shouldThrow("(1234.567).toFixed(-21)");

shouldThrow("(1234.567).toFixed(posInf)");
shouldThrow("(1234.567).toFixed(negInf)");

shouldBeEqualToString("posInf.toFixed()", "Infinity");
shouldBeEqualToString("negInf.toFixed()", "-Infinity");
shouldBeEqualToString("nan.toFixed()", "NaN");
