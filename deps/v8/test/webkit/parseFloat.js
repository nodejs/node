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

description('Tests for the parseFloat function.');

var nonASCIINonSpaceCharacter = String.fromCharCode(0x13A0);
var illegalUTF16Sequence = String.fromCharCode(0xD800);

var tab = String.fromCharCode(9);
var nbsp = String.fromCharCode(0xA0);
var ff = String.fromCharCode(0xC);
var vt = String.fromCharCode(0xB);
var cr = String.fromCharCode(0xD);
var lf = String.fromCharCode(0xA);
var ls = String.fromCharCode(0x2028);
var ps = String.fromCharCode(0x2029);

var oghamSpaceMark = String.fromCharCode(0x1680);
var mongolianVowelSeparator = String.fromCharCode(0x180E);
var enQuad = String.fromCharCode(0x2000);
var emQuad = String.fromCharCode(0x2001);
var enSpace = String.fromCharCode(0x2002);
var emSpace = String.fromCharCode(0x2003);
var threePerEmSpace = String.fromCharCode(0x2004);
var fourPerEmSpace = String.fromCharCode(0x2005);
var sixPerEmSpace = String.fromCharCode(0x2006);
var figureSpace = String.fromCharCode(0x2007);
var punctuationSpace = String.fromCharCode(0x2008);
var thinSpace = String.fromCharCode(0x2009);
var hairSpace = String.fromCharCode(0x200A);
var narrowNoBreakSpace = String.fromCharCode(0x202F);
var mediumMathematicalSpace = String.fromCharCode(0x205F);
var ideographicSpace = String.fromCharCode(0x3000);

shouldBe("parseFloat()", "NaN");
shouldBe("parseFloat('')", "NaN");
shouldBe("parseFloat(' ')", "NaN");
shouldBe("parseFloat(' 0')", "0");
shouldBe("parseFloat('0 ')", "0");
shouldBe("parseFloat('x0')", "NaN");
shouldBe("parseFloat('0x')", "0");
shouldBe("parseFloat(' 1')", "1");
shouldBe("parseFloat('1 ')", "1");
shouldBe("parseFloat('x1')", "NaN");
shouldBe("parseFloat('1x')", "1");
shouldBe("parseFloat(' 2.3')", "2.3");
shouldBe("parseFloat('2.3 ')", "2.3");
shouldBe("parseFloat('x2.3')", "NaN");
shouldBe("parseFloat('2.3x')", "2.3");
shouldBe("parseFloat('0x2')", "0");
shouldBe("parseFloat('1' + nonASCIINonSpaceCharacter)", "1");
shouldBe("parseFloat(nonASCIINonSpaceCharacter + '1')", "NaN");
shouldBe("parseFloat('1' + illegalUTF16Sequence)", "1");
shouldBe("parseFloat(illegalUTF16Sequence + '1')", "NaN");
shouldBe("parseFloat(tab + '1')", "1");
shouldBe("parseFloat(nbsp + '1')", "1");
shouldBe("parseFloat(ff + '1')", "1");
shouldBe("parseFloat(vt + '1')", "1");
shouldBe("parseFloat(cr + '1')", "1");
shouldBe("parseFloat(lf + '1')", "1");
shouldBe("parseFloat(ls + '1')", "1");
shouldBe("parseFloat(ps + '1')", "1");
shouldBe("parseFloat(oghamSpaceMark + '1')", "1");
shouldBe("parseFloat(mongolianVowelSeparator + '1')", "1");
shouldBe("parseFloat(enQuad + '1')", "1");
shouldBe("parseFloat(emQuad + '1')", "1");
shouldBe("parseFloat(enSpace + '1')", "1");
shouldBe("parseFloat(emSpace + '1')", "1");
shouldBe("parseFloat(threePerEmSpace + '1')", "1");
shouldBe("parseFloat(fourPerEmSpace + '1')", "1");
shouldBe("parseFloat(sixPerEmSpace + '1')", "1");
shouldBe("parseFloat(figureSpace + '1')", "1");
shouldBe("parseFloat(punctuationSpace + '1')", "1");
shouldBe("parseFloat(thinSpace + '1')", "1");
shouldBe("parseFloat(hairSpace + '1')", "1");
shouldBe("parseFloat(narrowNoBreakSpace + '1')", "1");
shouldBe("parseFloat(mediumMathematicalSpace + '1')", "1");
shouldBe("parseFloat(ideographicSpace + '1')", "1");
