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

description("This page tests handling of parenthetical assertions.");

var regex1 = /(x)(?=\1)x/;
shouldBe("regex1.exec('xx')", "['xx','x']");

var regex2 = /(.*?)a(?!(a+)b\2c)\2(.*)/;
shouldBe("regex2.exec('baaabaac')", "['baaabaac','ba',undefined,'abaac']");

var regex3 = /(?=(a+?))(\1ab)/;
shouldBe("regex3.exec('aaab')", "['aab','a','aab']");

var regex4 = /(?=(a+?))(\1ab)/;
shouldBe("regex4.exec('aaab')", "['aab','a','aab']");

var regex5 = /^P([1-6])(?=\1)([1-6])$/;
shouldBe("regex5.exec('P11')", "['P11','1','1']");

var regex6 = /(([a-c])b*?\2)*/;
shouldBe("regex6.exec('ababbbcbc')", "['ababb','bb','b']");

var regex7 = /(x)(?=x)x/;
shouldBe("regex7.exec('xx')", "['xx','x']");

var regex8 = /(x)(\1)/;
shouldBe("regex8.exec('xx')", "['xx','x','x']");

var regex9 = /(x)(?=\1)x/;
shouldBeNull("regex9.exec('xy')");

var regex10 = /(x)(?=x)x/;
shouldBeNull("regex10.exec('xy')");

var regex11 = /(x)(\1)/;
shouldBeNull("regex11.exec('xy')");

var regex12 = /(x)(?=\1)x/;
shouldBeNull("regex12.exec('x')");
shouldBe("regex12.exec('xx')", "['xx','x']");
shouldBe("regex12.exec('xxy')", "['xx','x']");

var regex13 = /(x)zzz(?=\1)x/;
shouldBe("regex13.exec('xzzzx')", "['xzzzx','x']");
shouldBe("regex13.exec('xzzzxy')", "['xzzzx','x']");

var regex14 = /(a)\1(?=(b*c))bc/;
shouldBe("regex14.exec('aabc')", "['aabc','a','bc']");
shouldBe("regex14.exec('aabcx')", "['aabc','a','bc']");

var regex15 = /(a)a(?=(b*c))bc/;
shouldBe("regex15.exec('aabc')", "['aabc','a','bc']");
shouldBe("regex15.exec('aabcx')", "['aabc','a','bc']");

var regex16 = /a(?=(b*c))bc/;
shouldBeNull("regex16.exec('ab')");
shouldBe("regex16.exec('abc')", "['abc','bc']");

var regex17 = /(?=((?:ab)*))a/;
shouldBe("regex17.exec('ab')", "['a','ab']");
shouldBe("regex17.exec('abc')", "['a','ab']");

var regex18 = /(?=((?:xx)*))x/;
shouldBe("regex18.exec('x')", "['x','']");
shouldBe("regex18.exec('xx')", "['x','xx']");
shouldBe("regex18.exec('xxx')", "['x','xx']");

var regex19 = /(?=((xx)*))x/;
shouldBe("regex19.exec('x')", "['x','',undefined]");
shouldBe("regex19.exec('xx')", "['x','xx','xx']");
shouldBe("regex19.exec('xxx')", "['x','xx','xx']");

var regex20 = /(?=(xx))+x/;
shouldBeNull("regex20.exec('x')");
shouldBe("regex20.exec('xx')", "['x','xx']");
shouldBe("regex20.exec('xxx')", "['x','xx']");

var regex21 = /(?=a+b)aab/;
shouldBe("regex21.exec('aab')", "['aab']");

var regex22 = /(?!(u|m{0,}g+)u{1,}|2{2,}!1%n|(?!K|(?=y)|(?=ip))+?)(?=(?=(((?:7))*?)*?))p/m;
shouldBe("regex22.exec('55up')", "null");

var regex23 = /(?=(a)b|c?)()*d/;
shouldBe("regex23.exec('ax')", "null");

var regex24 = /(?=a|b?)c/;
shouldBe("regex24.exec('x')", "null");
