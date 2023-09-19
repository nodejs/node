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

var posInf = 1/0;
var negInf = -1/0;
var nan = 0/0;

shouldBeEqualToString("(0.0).toString(4)", "0");
shouldBeEqualToString("(-0.0).toString(4)", "0");
shouldBeEqualToString("(0.0).toString()", "0");
shouldBeEqualToString("(-0.0).toString()", "0");

// From http://bugs.webkit.org/show_bug.cgi?id=5258
shouldBeEqualToString("(1234.567).toString()", "1234.567");
shouldThrow("(1234.567).toString(0)");
// 0 equivilents
shouldThrow("(1234.567).toString(null)");
shouldThrow("(1234.567).toString(false)");
shouldThrow("(1234.567).toString('foo')");
shouldThrow("(1234.567).toString(nan)"); // nan is treated like 0

shouldThrow("(1234.567).toString(1)");
shouldThrow("(1234.567).toString(true)");
shouldThrow("(1234.567).toString('1')");

// These test for Firefox compatibility, the spec is "implementation defined"
shouldBeEqualToString("(1234.567).toString(2)", "10011010010.1001000100100110111010010111100011010101");
shouldBeEqualToString("(1234.567).toString(3)", "1200201.120022100021001021021002202");
shouldBeEqualToString("(1234.567).toString(4)", "103102.21010212322113203111");
shouldBeEqualToString("(1234.567).toString(4.9)", "103102.21010212322113203111");
shouldBeEqualToString("(1234.567).toString(5)", "14414.240414141414141414");
shouldBeEqualToString("(1234.567).toString(6)", "5414.32224554134430233");
shouldBeEqualToString("(1234.567).toString(7)", "3412.365323661111653");
shouldBeEqualToString("(1234.567).toString(8)", "2322.44223351361524");
shouldBeEqualToString("(1234.567).toString(9)", "1621.50830703723265");
shouldBeEqualToString("(1234.567).toString(10)", "1234.567");
shouldBeEqualToString("(1234.567).toString(11)", "a22.62674a0a5885");
shouldBeEqualToString("(1234.567).toString(12)", "86a.697938b17701");
shouldBeEqualToString("(1234.567).toString(13)", "73c.74a91191a65");
shouldBeEqualToString("(1234.567).toString(14)", "642.7d1bc2caa757");
shouldBeEqualToString("(1234.567).toString(15)", "574.87895959596");
shouldBeEqualToString("(1234.567).toString(16)", "4d2.9126e978d5");
shouldBeEqualToString("(1234.567).toString(17)", "44a.9aeb6faa0da");
shouldBeEqualToString("(1234.567).toString(18)", "3ea.a3cd7102ac");
shouldBeEqualToString("(1234.567).toString(19)", "37i.aed102a04d");
shouldBeEqualToString("(1234.567).toString(20)", "31e.b6g");
shouldBeEqualToString("(1234.567).toString(21)", "2gg.bj0kf5cfe9");
shouldBeEqualToString("(1234.567).toString(22)", "2c2.ca9937cak");
shouldBeEqualToString("(1234.567).toString(23)", "27f.d0lfjb1a7c");
shouldBeEqualToString("(1234.567).toString(24)", "23a.dee4nj99j");
shouldBeEqualToString("(1234.567).toString(25)", "1o9.e49999999");
shouldBeEqualToString("(1234.567).toString(26)", "1lc.ej7fa4pkf");
shouldBeEqualToString("(1234.567).toString(27)", "1ij.f8971772k");
shouldBeEqualToString("(1234.567).toString(28)", "1g2.foelqia8e");
shouldBeEqualToString("(1234.567).toString(29)", "1dg.gcog9e05q");
shouldBeEqualToString("(1234.567).toString(30)", "1b4.h09");
shouldBeEqualToString("(1234.567).toString(31)", "18p.hhrfcj3t");
shouldBeEqualToString("(1234.567).toString(32)", "16i.i4jeiu6l");
shouldBeEqualToString("(1234.567).toString(33)", "14d.inf96rdvm");
shouldBeEqualToString("(1234.567).toString(34)", "12a.j9fchdtm");
shouldBeEqualToString("(1234.567).toString(35)", "109.jtk4d4d4e");
shouldBeEqualToString("(1234.567).toString(36)", "ya.kety9sifl");

shouldThrow("(1234.567).toString(37)");
shouldThrow("(1234.567).toString(-1)");
shouldThrow("(1234.567).toString(posInf)");
shouldThrow("(1234.567).toString(negInf)");

shouldBeEqualToString("posInf.toString()", "Infinity");
shouldBeEqualToString("negInf.toString()", "-Infinity");
shouldBeEqualToString("nan.toString()", "NaN");

shouldBeEqualToString('"" + -0.0', "0");
