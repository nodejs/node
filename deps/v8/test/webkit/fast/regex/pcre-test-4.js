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
"A chunk of our port of PCRE's test suite, adapted to be more applicable to JavaScript."
);

var regex0 = /a.b/;
var input0 = "acb";
var results = ["acb"];
shouldBe('regex0.exec(input0);', 'results');
var input1 = "a\x7fb";
var results = ["a\u007fb"];
shouldBe('regex0.exec(input1);', 'results');
var input2 = "a\u0100b";
var results = ["a\u0100b"];
shouldBe('regex0.exec(input2);', 'results');
// Failers
var input3 = "a\nb";
var results = null;
shouldBe('regex0.exec(input3);', 'results');

var regex1 = /a(.{3})b/;
var input0 = "a\u4000xyb";
var results = ["a\u4000xyb", "\u4000xy"];
shouldBe('regex1.exec(input0);', 'results');
var input1 = "a\u4000\x7fyb";
var results = ["a\u4000\u007fyb", "\u4000\u007fy"];
shouldBe('regex1.exec(input1);', 'results');
var input2 = "a\u4000\u0100yb";
var results = ["a\u4000\u0100yb", "\u4000\u0100y"];
shouldBe('regex1.exec(input2);', 'results');
// Failers
var input3 = "a\u4000b";
var results = null;
shouldBe('regex1.exec(input3);', 'results');
var input4 = "ac\ncb";
var results = null;
shouldBe('regex1.exec(input4);', 'results');

var regex2 = /a(.*?)(.)/;
var input0 = "a\xc0\x88b";
var results = ["a\xc0", "", "\xc0"];
shouldBe('regex2.exec(input0);', 'results');

var regex3 = /a(.*?)(.)/;
var input0 = "a\u0100b";
var results = ["a\u0100", "", "\u0100"];
shouldBe('regex3.exec(input0);', 'results');

var regex4 = /a(.*)(.)/;
var input0 = "a\xc0\x88b";
var results = ["a\xc0\x88b", "\xc0\x88", "b"];
shouldBe('regex4.exec(input0);', 'results');

var regex5 = /a(.*)(.)/;
var input0 = "a\u0100b";
var results = ["a\u0100b", "\u0100", "b"];
shouldBe('regex5.exec(input0);', 'results');

var regex6 = /a(.)(.)/;
var input0 = "a\xc0\x92bcd";
var results = ["a\xc0\x92", "\xc0", "\x92"];
shouldBe('regex6.exec(input0);', 'results');

var regex7 = /a(.)(.)/;
var input0 = "a\u0240bcd";
var results = ["a\u0240b", "\u0240", "b"];
shouldBe('regex7.exec(input0);', 'results');

var regex8 = /a(.?)(.)/;
var input0 = "a\xc0\x92bcd";
var results = ["a\xc0\x92", "\xc0", "\x92"];
shouldBe('regex8.exec(input0);', 'results');

var regex9 = /a(.?)(.)/;
var input0 = "a\u0240bcd";
var results = ["a\u0240b", "\u0240", "b"];
shouldBe('regex9.exec(input0);', 'results');

var regex10 = /a(.??)(.)/;
var input0 = "a\xc0\x92bcd";
var results = ["a\xc0", "", "\xc0"];
shouldBe('regex10.exec(input0);', 'results');

var regex11 = /a(.??)(.)/;
var input0 = "a\u0240bcd";
var results = ["a\u0240", "", "\u0240"];
shouldBe('regex11.exec(input0);', 'results');

var regex12 = /a(.{3})b/;
var input0 = "a\u1234xyb";
var results = ["a\u1234xyb", "\u1234xy"];
shouldBe('regex12.exec(input0);', 'results');
var input1 = "a\u1234\u4321yb";
var results = ["a\u1234\u4321yb", "\u1234\u4321y"];
shouldBe('regex12.exec(input1);', 'results');
var input2 = "a\u1234\u4321\u3412b";
var results = ["a\u1234\u4321\u3412b", "\u1234\u4321\u3412"];
shouldBe('regex12.exec(input2);', 'results');
// Failers
var input3 = "a\u1234b";
var results = null;
shouldBe('regex12.exec(input3);', 'results');
var input4 = "ac\ncb";
var results = null;
shouldBe('regex12.exec(input4);', 'results');

var regex13 = /a(.{3,})b/;
var input0 = "a\u1234xyb";
var results = ["a\u1234xyb", "\u1234xy"];
shouldBe('regex13.exec(input0);', 'results');
var input1 = "a\u1234\u4321yb";
var results = ["a\u1234\u4321yb", "\u1234\u4321y"];
shouldBe('regex13.exec(input1);', 'results');
var input2 = "a\u1234\u4321\u3412b";
var results = ["a\u1234\u4321\u3412b", "\u1234\u4321\u3412"];
shouldBe('regex13.exec(input2);', 'results');
var input3 = "axxxxbcdefghijb";
var results = ["axxxxbcdefghijb", "xxxxbcdefghij"];
shouldBe('regex13.exec(input3);', 'results');
var input4 = "a\u1234\u4321\u3412\u3421b";
var results = ["a\u1234\u4321\u3412\u3421b", "\u1234\u4321\u3412\u3421"];
shouldBe('regex13.exec(input4);', 'results');
// Failers
var input5 = "a\u1234b";
var results = null;
shouldBe('regex13.exec(input5);', 'results');

var regex14 = /a(.{3,}?)b/;
var input0 = "a\u1234xyb";
var results = ["a\u1234xyb", "\u1234xy"];
shouldBe('regex14.exec(input0);', 'results');
var input1 = "a\u1234\u4321yb";
var results = ["a\u1234\u4321yb", "\u1234\u4321y"];
shouldBe('regex14.exec(input1);', 'results');
var input2 = "a\u1234\u4321\u3412b";
var results = ["a\u1234\u4321\u3412b", "\u1234\u4321\u3412"];
shouldBe('regex14.exec(input2);', 'results');
var input3 = "axxxxbcdefghijb";
var results = ["axxxxb", "xxxx"];
shouldBe('regex14.exec(input3);', 'results');
var input4 = "a\u1234\u4321\u3412\u3421b";
var results = ["a\u1234\u4321\u3412\u3421b", "\u1234\u4321\u3412\u3421"];
shouldBe('regex14.exec(input4);', 'results');
// Failers
var input5 = "a\u1234b";
var results = null;
shouldBe('regex14.exec(input5);', 'results');

var regex15 = /a(.{3,5})b/;
var input0 = "a\u1234xyb";
var results = ["a\u1234xyb", "\u1234xy"];
shouldBe('regex15.exec(input0);', 'results');
var input1 = "a\u1234\u4321yb";
var results = ["a\u1234\u4321yb", "\u1234\u4321y"];
shouldBe('regex15.exec(input1);', 'results');
var input2 = "a\u1234\u4321\u3412b";
var results = ["a\u1234\u4321\u3412b", "\u1234\u4321\u3412"];
shouldBe('regex15.exec(input2);', 'results');
var input3 = "axxxxbcdefghijb";
var results = ["axxxxb", "xxxx"];
shouldBe('regex15.exec(input3);', 'results');
var input4 = "a\u1234\u4321\u3412\u3421b";
var results = ["a\u1234\u4321\u3412\u3421b", "\u1234\u4321\u3412\u3421"];
shouldBe('regex15.exec(input4);', 'results');
var input5 = "axbxxbcdefghijb";
var results = ["axbxxb", "xbxx"];
shouldBe('regex15.exec(input5);', 'results');
var input6 = "axxxxxbcdefghijb";
var results = ["axxxxxb", "xxxxx"];
shouldBe('regex15.exec(input6);', 'results');
// Failers
var input7 = "a\u1234b";
var results = null;
shouldBe('regex15.exec(input7);', 'results');
var input8 = "axxxxxxbcdefghijb";
var results = null;
shouldBe('regex15.exec(input8);', 'results');

var regex16 = /a(.{3,5}?)b/;
var input0 = "a\u1234xyb";
var results = ["a\u1234xyb", "\u1234xy"];
shouldBe('regex16.exec(input0);', 'results');
var input1 = "a\u1234\u4321yb";
var results = ["a\u1234\u4321yb", "\u1234\u4321y"];
shouldBe('regex16.exec(input1);', 'results');
var input2 = "a\u1234\u4321\u3412b";
var results = ["a\u1234\u4321\u3412b", "\u1234\u4321\u3412"];
shouldBe('regex16.exec(input2);', 'results');
var input3 = "axxxxbcdefghijb";
var results = ["axxxxb", "xxxx"];
shouldBe('regex16.exec(input3);', 'results');
var input4 = "a\u1234\u4321\u3412\u3421b";
var results = ["a\u1234\u4321\u3412\u3421b", "\u1234\u4321\u3412\u3421"];
shouldBe('regex16.exec(input4);', 'results');
var input5 = "axbxxbcdefghijb";
var results = ["axbxxb", "xbxx"];
shouldBe('regex16.exec(input5);', 'results');
var input6 = "axxxxxbcdefghijb";
var results = ["axxxxxb", "xxxxx"];
shouldBe('regex16.exec(input6);', 'results');
// Failers
var input7 = "a\u1234b";
var results = null;
shouldBe('regex16.exec(input7);', 'results');
var input8 = "axxxxxxbcdefghijb";
var results = null;
shouldBe('regex16.exec(input8);', 'results');

var regex17 = /^[a\u00c0]/;
// Failers
var input0 = "\u0100";
var results = null;
shouldBe('regex17.exec(input0);', 'results');

var regex21 = /(?:\u0100){3}b/;
var input0 = "\u0100\u0100\u0100b";
var results = ["\u0100\u0100\u0100b"];
shouldBe('regex21.exec(input0);', 'results');
// Failers
var input1 = "\u0100\u0100b";
var results = null;
shouldBe('regex21.exec(input1);', 'results');

var regex22 = /\u00ab/;
var input0 = "\u00ab";
var results = ["\u00ab"];
shouldBe('regex22.exec(input0);', 'results');
var input1 = "\xc2\xab";
var results = ["\u00ab"];
shouldBe('regex22.exec(input1);', 'results');
// Failers
var input2 = "\x00{ab}";
var results = null;
shouldBe('regex22.exec(input2);', 'results');

var regex30 = /^[^a]{2}/;
var input0 = "\u0100bc";
var results = ["\u0100b"];
shouldBe('regex30.exec(input0);', 'results');

var regex31 = /^[^a]{2,}/;
var input0 = "\u0100bcAa";
var results = ["\u0100bcA"];
shouldBe('regex31.exec(input0);', 'results');

var regex32 = /^[^a]{2,}?/;
var input0 = "\u0100bca";
var results = ["\u0100b"];
shouldBe('regex32.exec(input0);', 'results');

var regex33 = /^[^a]{2}/i;
var input0 = "\u0100bc";
var results = ["\u0100b"];
shouldBe('regex33.exec(input0);', 'results');

var regex34 = /^[^a]{2,}/i;
var input0 = "\u0100bcAa";
var results = ["\u0100bc"];
shouldBe('regex34.exec(input0);', 'results');

var regex35 = /^[^a]{2,}?/i;
var input0 = "\u0100bca";
var results = ["\u0100b"];
shouldBe('regex35.exec(input0);', 'results');

var regex36 = /\u0100{0,0}/;
var input0 = "abcd";
var results = [""];
shouldBe('regex36.exec(input0);', 'results');

var regex37 = /\u0100?/;
var input0 = "abcd";
var results = [""];
shouldBe('regex37.exec(input0);', 'results');
var input1 = "\u0100\u0100";
var results = ["\u0100"];
shouldBe('regex37.exec(input1);', 'results');

var regex38 = /\u0100{0,3}/;
var input0 = "\u0100\u0100";
var results = ["\u0100\u0100"];
shouldBe('regex38.exec(input0);', 'results');
var input1 = "\u0100\u0100\u0100\u0100";
var results = ["\u0100\u0100\u0100"];
shouldBe('regex38.exec(input1);', 'results');

var regex39 = /\u0100*/;
var input0 = "abce";
var results = [""];
shouldBe('regex39.exec(input0);', 'results');
var input1 = "\u0100\u0100\u0100\u0100";
var results = ["\u0100\u0100\u0100\u0100"];
shouldBe('regex39.exec(input1);', 'results');

var regex40 = /\u0100{1,1}/;
var input0 = "abcd\u0100\u0100\u0100\u0100";
var results = ["\u0100"];
shouldBe('regex40.exec(input0);', 'results');

var regex41 = /\u0100{1,3}/;
var input0 = "abcd\u0100\u0100\u0100\u0100";
var results = ["\u0100\u0100\u0100"];
shouldBe('regex41.exec(input0);', 'results');

var regex42 = /\u0100+/;
var input0 = "abcd\u0100\u0100\u0100\u0100";
var results = ["\u0100\u0100\u0100\u0100"];
shouldBe('regex42.exec(input0);', 'results');

var regex43 = /\u0100{3}/;
var input0 = "abcd\u0100\u0100\u0100XX";
var results = ["\u0100\u0100\u0100"];
shouldBe('regex43.exec(input0);', 'results');

var regex44 = /\u0100{3,5}/;
var input0 = "abcd\u0100\u0100\u0100\u0100\u0100\u0100\u0100XX";
var results = ["\u0100\u0100\u0100\u0100\u0100"];
shouldBe('regex44.exec(input0);', 'results');

var regex45 = /\u0100{3,}/;
var input0 = "abcd\u0100\u0100\u0100\u0100\u0100\u0100\u0100XX";
var results = ["\u0100\u0100\u0100\u0100\u0100\u0100\u0100"];
shouldBe('regex45.exec(input0);', 'results');

var regex47 = /\D*/;
var input0 = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
var results = ["aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"];
shouldBe('regex47.exec(input0);', 'results');

var regex48 = /\D*/;
var input0 = "\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100";
var results = ["\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100\u0100"];
shouldBe('regex48.exec(input0);', 'results');

var regex49 = /\D/;
var input0 = "1X2";
var results = ["X"];
shouldBe('regex49.exec(input0);', 'results');
var input1 = "1\u01002";
var results = ["\u0100"];
shouldBe('regex49.exec(input1);', 'results');

var regex50 = />\S/;
var input0 = "> >X Y";
var results = [">X"];
shouldBe('regex50.exec(input0);', 'results');
var input1 = "> >\u0100 Y";
var results = [">\u0100"];
shouldBe('regex50.exec(input1);', 'results');

var regex51 = /\d/;
var input0 = "\u01003";
var results = ["3"];
shouldBe('regex51.exec(input0);', 'results');

var regex52 = /\s/;
var input0 = "\u0100 X";
var results = [" "];
shouldBe('regex52.exec(input0);', 'results');

var regex53 = /\D+/;
var input0 = "12abcd34";
var results = ["abcd"];
shouldBe('regex53.exec(input0);', 'results');
// Failers
var input1 = "1234";
var results = null;
shouldBe('regex53.exec(input1);', 'results');

var regex54 = /\D{2,3}/;
var input0 = "12abcd34";
var results = ["abc"];
shouldBe('regex54.exec(input0);', 'results');
var input1 = "12ab34";
var results = ["ab"];
shouldBe('regex54.exec(input1);', 'results');
// Failers
var input2 = "1234";
var results = null;
shouldBe('regex54.exec(input2);', 'results');
var input3 = "12a34";
var results = null;
shouldBe('regex54.exec(input3);', 'results');

var regex55 = /\D{2,3}?/;
var input0 = "12abcd34";
var results = ["ab"];
shouldBe('regex55.exec(input0);', 'results');
var input1 = "12ab34";
var results = ["ab"];
shouldBe('regex55.exec(input1);', 'results');
// Failers
var input2 = "1234";
var results = null;
shouldBe('regex55.exec(input2);', 'results');
var input3 = "12a34";
var results = null;
shouldBe('regex55.exec(input3);', 'results');

var regex56 = /\d+/;
var input0 = "12abcd34";
var results = ["12"];
shouldBe('regex56.exec(input0);', 'results');

var regex57 = /\d{2,3}/;
var input0 = "12abcd34";
var results = ["12"];
shouldBe('regex57.exec(input0);', 'results');
var input1 = "1234abcd";
var results = ["123"];
shouldBe('regex57.exec(input1);', 'results');
// Failers
var input2 = "1.4";
var results = null;
shouldBe('regex57.exec(input2);', 'results');

var regex58 = /\d{2,3}?/;
var input0 = "12abcd34";
var results = ["12"];
shouldBe('regex58.exec(input0);', 'results');
var input1 = "1234abcd";
var results = ["12"];
shouldBe('regex58.exec(input1);', 'results');
// Failers
var input2 = "1.4";
var results = null;
shouldBe('regex58.exec(input2);', 'results');

var regex59 = /\S+/;
var input0 = "12abcd34";
var results = ["12abcd34"];
shouldBe('regex59.exec(input0);', 'results');
// Failers
var input1 = "    ";
var results = null;
shouldBe('regex59.exec(input1);', 'results');

var regex60 = /\S{2,3}/;
var input0 = "12abcd34";
var results = ["12a"];
shouldBe('regex60.exec(input0);', 'results');
var input1 = "1234abcd";
var results = ["123"];
shouldBe('regex60.exec(input1);', 'results');
// Failers
var input2 = "    ";
var results = null;
shouldBe('regex60.exec(input2);', 'results');

var regex61 = /\S{2,3}?/;
var input0 = "12abcd34";
var results = ["12"];
shouldBe('regex61.exec(input0);', 'results');
var input1 = "1234abcd";
var results = ["12"];
shouldBe('regex61.exec(input1);', 'results');
// Failers
var input2 = "    ";
var results = null;
shouldBe('regex61.exec(input2);', 'results');

var regex62 = />\s+</;
var input0 = "12>      <34";
var results = [">      <"];
shouldBe('regex62.exec(input0);', 'results');

var regex63 = />\s{2,3}</;
var input0 = "ab>  <cd";
var results = [">  <"];
shouldBe('regex63.exec(input0);', 'results');
var input1 = "ab>   <ce";
var results = [">   <"];
shouldBe('regex63.exec(input1);', 'results');
// Failers
var input2 = "ab>    <cd";
var results = null;
shouldBe('regex63.exec(input2);', 'results');

var regex64 = />\s{2,3}?</;
var input0 = "ab>  <cd";
var results = [">  <"];
shouldBe('regex64.exec(input0);', 'results');
var input1 = "ab>   <ce";
var results = [">   <"];
shouldBe('regex64.exec(input1);', 'results');
// Failers
var input2 = "ab>    <cd";
var results = null;
shouldBe('regex64.exec(input2);', 'results');

var regex65 = /\w+/;
var input0 = "12      34";
var results = ["12"];
shouldBe('regex65.exec(input0);', 'results');
// Failers
var input1 = "+++=*!";
var results = null;
shouldBe('regex65.exec(input1);', 'results');

var regex66 = /\w{2,3}/;
var input0 = "ab  cd";
var results = ["ab"];
shouldBe('regex66.exec(input0);', 'results');
var input1 = "abcd ce";
var results = ["abc"];
shouldBe('regex66.exec(input1);', 'results');
// Failers
var input2 = "a.b.c";
var results = null;
shouldBe('regex66.exec(input2);', 'results');

var regex67 = /\w{2,3}?/;
var input0 = "ab  cd";
var results = ["ab"];
shouldBe('regex67.exec(input0);', 'results');
var input1 = "abcd ce";
var results = ["ab"];
shouldBe('regex67.exec(input1);', 'results');
// Failers
var input2 = "a.b.c";
var results = null;
shouldBe('regex67.exec(input2);', 'results');

var regex68 = /\W+/;
var input0 = "12====34";
var results = ["===="];
shouldBe('regex68.exec(input0);', 'results');
// Failers
var input1 = "abcd";
var results = null;
shouldBe('regex68.exec(input1);', 'results');

var regex69 = /\W{2,3}/;
var input0 = "ab====cd";
var results = ["==="];
shouldBe('regex69.exec(input0);', 'results');
var input1 = "ab==cd";
var results = ["=="];
shouldBe('regex69.exec(input1);', 'results');
// Failers
var input2 = "a.b.c";
var results = null;
shouldBe('regex69.exec(input2);', 'results');

var regex70 = /\W{2,3}?/;
var input0 = "ab====cd";
var results = ["=="];
shouldBe('regex70.exec(input0);', 'results');
var input1 = "ab==cd";
var results = ["=="];
shouldBe('regex70.exec(input1);', 'results');
// Failers
var input2 = "a.b.c";
var results = null;
shouldBe('regex70.exec(input2);', 'results');

var regex71 = /[\u0100]/;
var input0 = "\u0100";
var results = ["\u0100"];
shouldBe('regex71.exec(input0);', 'results');
var input1 = "Z\u0100";
var results = ["\u0100"];
shouldBe('regex71.exec(input1);', 'results');
var input2 = "\u0100Z";
var results = ["\u0100"];
shouldBe('regex71.exec(input2);', 'results');

var regex72 = /[Z\u0100]/;
var input0 = "Z\u0100";
var results = ["Z"];
shouldBe('regex72.exec(input0);', 'results');
var input1 = "\u0100";
var results = ["\u0100"];
shouldBe('regex72.exec(input1);', 'results');
var input2 = "\u0100Z";
var results = ["\u0100"];
shouldBe('regex72.exec(input2);', 'results');

var regex73 = /[\u0100\u0200]/;
var input0 = "ab\u0100cd";
var results = ["\u0100"];
shouldBe('regex73.exec(input0);', 'results');
var input1 = "ab\u0200cd";
var results = ["\u0200"];
shouldBe('regex73.exec(input1);', 'results');

var regex74 = /[\u0100-\u0200]/;
var input0 = "ab\u0100cd";
var results = ["\u0100"];
shouldBe('regex74.exec(input0);', 'results');
var input1 = "ab\u0200cd";
var results = ["\u0200"];
shouldBe('regex74.exec(input1);', 'results');
var input2 = "ab\u0111cd";
var results = ["\u0111"];
shouldBe('regex74.exec(input2);', 'results');

var regex75 = /[z-\u0200]/;
var input0 = "ab\u0100cd";
var results = ["\u0100"];
shouldBe('regex75.exec(input0);', 'results');
var input1 = "ab\u0200cd";
var results = ["\u0200"];
shouldBe('regex75.exec(input1);', 'results');
var input2 = "ab\u0111cd";
var results = ["\u0111"];
shouldBe('regex75.exec(input2);', 'results');
var input3 = "abzcd";
var results = ["z"];
shouldBe('regex75.exec(input3);', 'results');
var input4 = "ab|cd";
var results = ["|"];
shouldBe('regex75.exec(input4);', 'results');

var regex76 = /[Q\u0100\u0200]/;
var input0 = "ab\u0100cd";
var results = ["\u0100"];
shouldBe('regex76.exec(input0);', 'results');
var input1 = "ab\u0200cd";
var results = ["\u0200"];
shouldBe('regex76.exec(input1);', 'results');
var input2 = "Q?";
var results = ["Q"];
shouldBe('regex76.exec(input2);', 'results');

var regex77 = /[Q\u0100-\u0200]/;
var input0 = "ab\u0100cd";
var results = ["\u0100"];
shouldBe('regex77.exec(input0);', 'results');
var input1 = "ab\u0200cd";
var results = ["\u0200"];
shouldBe('regex77.exec(input1);', 'results');
var input2 = "ab\u0111cd";
var results = ["\u0111"];
shouldBe('regex77.exec(input2);', 'results');
var input3 = "Q?";
var results = ["Q"];
shouldBe('regex77.exec(input3);', 'results');

var regex78 = /[Qz-\u0200]/;
var input0 = "ab\u0100cd";
var results = ["\u0100"];
shouldBe('regex78.exec(input0);', 'results');
var input1 = "ab\u0200cd";
var results = ["\u0200"];
shouldBe('regex78.exec(input1);', 'results');
var input2 = "ab\u0111cd";
var results = ["\u0111"];
shouldBe('regex78.exec(input2);', 'results');
var input3 = "abzcd";
var results = ["z"];
shouldBe('regex78.exec(input3);', 'results');
var input4 = "ab|cd";
var results = ["|"];
shouldBe('regex78.exec(input4);', 'results');
var input5 = "Q?";
var results = ["Q"];
shouldBe('regex78.exec(input5);', 'results');

var regex79 = /[\u0100\u0200]{1,3}/;
var input0 = "ab\u0100cd";
var results = ["\u0100"];
shouldBe('regex79.exec(input0);', 'results');
var input1 = "ab\u0200cd";
var results = ["\u0200"];
shouldBe('regex79.exec(input1);', 'results');
var input2 = "ab\u0200\u0100\u0200\u0100cd";
var results = ["\u0200\u0100\u0200"];
shouldBe('regex79.exec(input2);', 'results');

var regex80 = /[\u0100\u0200]{1,3}?/;
var input0 = "ab\u0100cd";
var results = ["\u0100"];
shouldBe('regex80.exec(input0);', 'results');
var input1 = "ab\u0200cd";
var results = ["\u0200"];
shouldBe('regex80.exec(input1);', 'results');
var input2 = "ab\u0200\u0100\u0200\u0100cd";
var results = ["\u0200"];
shouldBe('regex80.exec(input2);', 'results');

var regex81 = /[Q\u0100\u0200]{1,3}/;
var input0 = "ab\u0100cd";
var results = ["\u0100"];
shouldBe('regex81.exec(input0);', 'results');
var input1 = "ab\u0200cd";
var results = ["\u0200"];
shouldBe('regex81.exec(input1);', 'results');
var input2 = "ab\u0200\u0100\u0200\u0100cd";
var results = ["\u0200\u0100\u0200"];
shouldBe('regex81.exec(input2);', 'results');

var regex82 = /[Q\u0100\u0200]{1,3}?/;
var input0 = "ab\u0100cd";
var results = ["\u0100"];
shouldBe('regex82.exec(input0);', 'results');
var input1 = "ab\u0200cd";
var results = ["\u0200"];
shouldBe('regex82.exec(input1);', 'results');
var input2 = "ab\u0200\u0100\u0200\u0100cd";
var results = ["\u0200"];
shouldBe('regex82.exec(input2);', 'results');

var regex86 = /[^\u0100\u0200]X/;
var input0 = "AX";
var results = ["AX"];
shouldBe('regex86.exec(input0);', 'results');
var input1 = "\u0150X";
var results = ["\u0150X"];
shouldBe('regex86.exec(input1);', 'results');
var input2 = "\u0500X";
var results = ["\u0500X"];
shouldBe('regex86.exec(input2);', 'results');
// Failers
var input3 = "\u0100X";
var results = null;
shouldBe('regex86.exec(input3);', 'results');
var input4 = "\u0200X";
var results = null;
shouldBe('regex86.exec(input4);', 'results');

var regex87 = /[^Q\u0100\u0200]X/;
var input0 = "AX";
var results = ["AX"];
shouldBe('regex87.exec(input0);', 'results');
var input1 = "\u0150X";
var results = ["\u0150X"];
shouldBe('regex87.exec(input1);', 'results');
var input2 = "\u0500X";
var results = ["\u0500X"];
shouldBe('regex87.exec(input2);', 'results');
// Failers
var input3 = "\u0100X";
var results = null;
shouldBe('regex87.exec(input3);', 'results');
var input4 = "\u0200X";
var results = null;
shouldBe('regex87.exec(input4);', 'results');
var input5 = "QX";
var results = null;
shouldBe('regex87.exec(input5);', 'results');

var regex88 = /[^\u0100-\u0200]X/;
var input0 = "AX";
var results = ["AX"];
shouldBe('regex88.exec(input0);', 'results');
var input1 = "\u0500X";
var results = ["\u0500X"];
shouldBe('regex88.exec(input1);', 'results');
// Failers
var input2 = "\u0100X";
var results = null;
shouldBe('regex88.exec(input2);', 'results');
var input3 = "\u0150X";
var results = null;
shouldBe('regex88.exec(input3);', 'results');
var input4 = "\u0200X";
var results = null;
shouldBe('regex88.exec(input4);', 'results');

var regex91 = /[z-\u0100]/i;
var input0 = "z";
var results = ["z"];
shouldBe('regex91.exec(input0);', 'results');
var input1 = "Z";
var results = ["Z"];
shouldBe('regex91.exec(input1);', 'results');
var input2 = "\u0100";
var results = ["\u0100"];
shouldBe('regex91.exec(input2);', 'results');
// Failers
var input3 = "\u0102";
var results = null;
shouldBe('regex91.exec(input3);', 'results');
var input4 = "y";
var results = null;
shouldBe('regex91.exec(input4);', 'results');

var regex92 = /[\xFF]/;
var input0 = ">\xff<";
var results = ["\xff"];
shouldBe('regex92.exec(input0);', 'results');

var regex93 = /[\xff]/;
var input0 = ">\u00ff<";
var results = ["\u00ff"];
shouldBe('regex93.exec(input0);', 'results');

var regex94 = /[^\xFF]/;
var input0 = "XYZ";
var results = ["X"];
shouldBe('regex94.exec(input0);', 'results');

var regex95 = /[^\xff]/;
var input0 = "XYZ";
var results = ["X"];
shouldBe('regex95.exec(input0);', 'results');
var input1 = "\u0123";
var results = ["\u0123"];
shouldBe('regex95.exec(input1);', 'results');

var regex96 = /^[ac]*b/;
var input0 = "xb";
var results = null;
shouldBe('regex96.exec(input0);', 'results');

var regex97 = /^[ac\u0100]*b/;
var input0 = "xb";
var results = null;
shouldBe('regex97.exec(input0);', 'results');

var regex98 = /^[^x]*b/i;
var input0 = "xb";
var results = null;
shouldBe('regex98.exec(input0);', 'results');

var regex99 = /^[^x]*b/;
var input0 = "xb";
var results = null;
shouldBe('regex99.exec(input0);', 'results');

var regex100 = /^\d*b/;
var input0 = "xb";
var results = null;
shouldBe('regex100.exec(input0);', 'results');

var regex102 = /^\u0085$/i;
var input0 = "\u0085";
var results = ["\u0085"];
shouldBe('regex102.exec(input0);', 'results');

var regex103 = /^\xe1\x88\xb4/;
var input0 = "\xe1\x88\xb4";
var results = ["\xe1\x88\xb4"];
shouldBe('regex103.exec(input0);', 'results');

var regex104 = /^\xe1\x88\xb4/;
var input0 = "\xe1\x88\xb4";
var results = ["\xe1\x88\xb4"];
shouldBe('regex104.exec(input0);', 'results');

var regex105 = /(.{1,5})/;
var input0 = "abcdefg";
var results = ["abcde", "abcde"];
shouldBe('regex105.exec(input0);', 'results');
var input1 = "ab";
var results = ["ab", "ab"];
shouldBe('regex105.exec(input1);', 'results');

var regex106 = /a*\u0100*\w/;
var input0 = "a";
var results = ["a"];
shouldBe('regex106.exec(input0);', 'results');

var regex107 = /[\S\s]*/;
var input0 = "abc\n\r\u0442\u0435\u0441\u0442xyz";
var results = ["abc\u000a\u000d\u0442\u0435\u0441\u0442xyz"];
shouldBe('regex107.exec(input0);', 'results');

var regexGlobal0 = /[^a]+/g;
var input0 = "bcd";
var results = ["bcd"];
shouldBe('input0.match(regexGlobal0);', 'results');
var input1 = "\u0100aY\u0256Z";
var results = ["\u0100", "Y\u0256Z"];
shouldBe('input1.match(regexGlobal0);', 'results');

var regexGlobal1 = /\S\S/g;
var input0 = "A\u00a3BC";
var results = ["A\u00a3", "BC"];
shouldBe('input0.match(regexGlobal1);', 'results');

var regexGlobal2 = /\S{2}/g;
var input0 = "A\u00a3BC";
var results = ["A\u00a3", "BC"];
shouldBe('input0.match(regexGlobal2);', 'results');

var regexGlobal3 = /\W\W/g;
var input0 = "+\u00a3==";
var results = ["+\u00a3", "=="];
shouldBe('input0.match(regexGlobal3);', 'results');

var regexGlobal4 = /\W{2}/g;
var input0 = "+\u00a3==";
var results = ["+\u00a3", "=="];
shouldBe('input0.match(regexGlobal4);', 'results');

var regexGlobal5 = /\S/g;
var input0 = "\u0442\u0435\u0441\u0442";
var results = ["\u0442", "\u0435", "\u0441", "\u0442"];
shouldBe('input0.match(regexGlobal5);', 'results');

var regexGlobal6 = /[\S]/g;
var input0 = "\u0442\u0435\u0441\u0442";
var results = ["\u0442", "\u0435", "\u0441", "\u0442"];
shouldBe('input0.match(regexGlobal6);', 'results');

var regexGlobal7 = /\D/g;
var input0 = "\u0442\u0435\u0441\u0442";
var results = ["\u0442", "\u0435", "\u0441", "\u0442"];
shouldBe('input0.match(regexGlobal7);', 'results');

var regexGlobal8 = /[\D]/g;
var input0 = "\u0442\u0435\u0441\u0442";
var results = ["\u0442", "\u0435", "\u0441", "\u0442"];
shouldBe('input0.match(regexGlobal8);', 'results');

var regexGlobal9 = /\W/g;
var input0 = "\u2442\u2435\u2441\u2442";
var results = ["\u2442", "\u2435", "\u2441", "\u2442"];
shouldBe('input0.match(regexGlobal9);', 'results');

var regexGlobal10 = /[\W]/g;
var input0 = "\u2442\u2435\u2441\u2442";
var results = ["\u2442", "\u2435", "\u2441", "\u2442"];
shouldBe('input0.match(regexGlobal10);', 'results');

var regexGlobal11 = /[\u041f\S]/g;
var input0 = "\u0442\u0435\u0441\u0442";
var results = ["\u0442", "\u0435", "\u0441", "\u0442"];
shouldBe('input0.match(regexGlobal11);', 'results');

var regexGlobal12 = /.[^\S]./g;
var input0 = "abc def\u0442\u0443xyz\npqr";
var results = ["c d", "z\u000ap"];
shouldBe('input0.match(regexGlobal12);', 'results');

var regexGlobal13 = /.[^\S\n]./g;
var input0 = "abc def\u0442\u0443xyz\npqr";
var results = ["c d"];
shouldBe('input0.match(regexGlobal13);', 'results');

var regexGlobal14 = /[\W]/g;
var input0 = "+\u2442";
var results = ["+", "\u2442"];
shouldBe('input0.match(regexGlobal14);', 'results');

var regexGlobal15 = /[^a-zA-Z]/g;
var input0 = "+\u2442";
var results = ["+", "\u2442"];
shouldBe('input0.match(regexGlobal15);', 'results');

var regexGlobal16 = /[^a-zA-Z]/g;
var input0 = "A\u0442";
var results = ["\u0442"];
shouldBe('input0.match(regexGlobal16);', 'results');

var regexGlobal17 = /[\S]/g;
var input0 = "A\u0442";
var results = ["A", "\u0442"];
shouldBe('input0.match(regexGlobal17);', 'results');

var regexGlobal19 = /[\D]/g;
var input0 = "A\u0442";
var results = ["A", "\u0442"];
shouldBe('input0.match(regexGlobal19);', 'results');

var regexGlobal21 = /[^a-z]/g;
var input0 = "A\u0422";
var results = ["A", "\u0422"];
shouldBe('input0.match(regexGlobal21);', 'results');

var regexGlobal24 = /[\S]/g;
var input0 = "A\u0442";
var results = ["A", "\u0442"];
shouldBe('input0.match(regexGlobal24);', 'results');

var regexGlobal25 = /[^A-Z]/g;
var input0 = "a\u0442";
var results = ["a", "\u0442"];
shouldBe('input0.match(regexGlobal25);', 'results');

var regexGlobal26 = /[\W]/g;
var input0 = "+\u2442";
var results = ["+", "\u2442"];
shouldBe('input0.match(regexGlobal26);', 'results');

var regexGlobal27 = /[\D]/g;
var input0 = "M\u0442";
var results = ["M", "\u0442"];
shouldBe('input0.match(regexGlobal27);', 'results');

var regexGlobal28 = /[^a]+/ig;
var input0 = "bcd";
var results = ["bcd"];
shouldBe('input0.match(regexGlobal28);', 'results');
var input1 = "\u0100aY\u0256Z";
var results = ["\u0100", "Y\u0256Z"];
shouldBe('input1.match(regexGlobal28);', 'results');

var regexGlobal29 = /(a|)/g;
var input0 = "catac";
var results = ["", "a", "", "a", "", ""];
shouldBe('input0.match(regexGlobal29);', 'results');
var input1 = "a\u0256a";
var results = ["a", "", "a", ""];
shouldBe('input1.match(regexGlobal29);', 'results');

// DISABLED:
// These tests use (?<) or (?>) constructs. These are not currently valid in ECMAScript,
// but these tests may be useful if similar constructs are introduced in the future.

//var regex18 = /(?<=aXb)cd/;
//var input0 = "aXbcd";
//var results = ["cd"];
//shouldBe('regex18.exec(input0);', 'results');
//
//var regex19 = /(?<=a\u0100b)cd/;
//var input0 = "a\u0100bcd";
//var results = ["cd"];
//shouldBe('regex19.exec(input0);', 'results');
//
//var regex20 = /(?<=a\u100000b)cd/;
//var input0 = "a\u100000bcd";
//var results = ["cd"];
//shouldBe('regex20.exec(input0);', 'results');
//
//var regex23 = /(?<=(.))X/;
//var input0 = "WXYZ";
//var results = ["X", "W"];
//shouldBe('regex23.exec(input0);', 'results');
//var input1 = "\u0256XYZ";
//var results = ["X", "\u0256"];
//shouldBe('regex23.exec(input1);', 'results');
//// Failers
//var input2 = "XYZ";
//var results = null;
//shouldBe('regex23.exec(input2);', 'results');
//
//var regex46 = /(?<=a\u0100{2}b)X/;
//var input0 = "Xyyya\u0100\u0100bXzzz";
//var results = ["X"];
//shouldBe('regex46.exec(input0);', 'results');
//
//var regex83 = /(?<=[\u0100\u0200])X/;
//var input0 = "abc\u0200X";
//var results = ["X"];
//shouldBe('regex83.exec(input0);', 'results');
//var input1 = "abc\u0100X";
//var results = ["X"];
//shouldBe('regex83.exec(input1);', 'results');
//// Failers
//var input2 = "X";
//var results = null;
//shouldBe('regex83.exec(input2);', 'results');
//
//var regex84 = /(?<=[Q\u0100\u0200])X/;
//var input0 = "abc\u0200X";
//var results = ["X"];
//shouldBe('regex84.exec(input0);', 'results');
//var input1 = "abc\u0100X";
//var results = ["X"];
//shouldBe('regex84.exec(input1);', 'results');
//var input2 = "abQX";
//var results = ["X"];
//shouldBe('regex84.exec(input2);', 'results');
//// Failers
//var input3 = "X";
//var results = null;
//shouldBe('regex84.exec(input3);', 'results');
//
//var regex85 = /(?<=[\u0100\u0200]{3})X/;
//var input0 = "abc\u0100\u0200\u0100X";
//var results = ["X"];
//shouldBe('regex85.exec(input0);', 'results');
//// Failers
//var input1 = "abc\u0200X";
//var results = null;
//shouldBe('regex85.exec(input1);', 'results');
//var input2 = "X";
//var results = null;
//shouldBe('regex85.exec(input2);', 'results');

// DISABLED:
// These tests use PCRE's \C token. This is not currently valid in ECMAScript,
// but these tests may be useful if similar constructs are introduced in the future.

//var regex24 = /X(\C{3})/;
//var input0 = "X\u1234";
//var results = ["X\u1234", "\u1234"];
//shouldBe('regex24.exec(input0);', 'results');
//
//var regex25 = /X(\C{4})/;
//var input0 = "X\u1234YZ";
//var results = ["X\u1234Y", "\u1234Y"];
//shouldBe('regex25.exec(input0);', 'results');
//
//var regex26 = /X\C*/;
//var input0 = "XYZabcdce";
//var results = ["XYZabcdce"];
//shouldBe('regex26.exec(input0);', 'results');
//
//var regex27 = /X\C*?/;
//var input0 = "XYZabcde";
//var results = ["X"];
//shouldBe('regex27.exec(input0);', 'results');
//
//var regex28 = /X\C{3,5}/;
//var input0 = "Xabcdefg";
//var results = ["Xabcde"];
//shouldBe('regex28.exec(input0);', 'results');
//var input1 = "X\u1234";
//var results = ["X\u1234"];
//shouldBe('regex28.exec(input1);', 'results');
//var input2 = "X\u1234YZ";
//var results = ["X\u1234YZ"];
//shouldBe('regex28.exec(input2);', 'results');
//var input3 = "X\u1234\u0512";
//var results = ["X\u1234\u0512"];
//shouldBe('regex28.exec(input3);', 'results');
//var input4 = "X\u1234\u0512YZ";
//var results = ["X\u1234\u0512"];
//shouldBe('regex28.exec(input4);', 'results');
//
//var regex29 = /X\C{3,5}?/;
//var input0 = "Xabcdefg";
//var results = ["Xabc"];
//shouldBe('regex29.exec(input0);', 'results');
//var input1 = "X\u1234";
//var results = ["X\u1234"];
//shouldBe('regex29.exec(input1);', 'results');
//var input2 = "X\u1234YZ";
//var results = ["X\u1234"];
//shouldBe('regex29.exec(input2);', 'results');
//var input3 = "X\u1234\u0512";
//var results = ["X\u1234"];
//shouldBe('regex29.exec(input3);', 'results');
//
//var regex89 = /a\Cb/;
//var input0 = "aXb";
//var results = ["aXb"];
//shouldBe('regex89.exec(input0);', 'results');
//var input1 = "a\nb";
//var results = ["a\x0ab"];
//shouldBe('regex89.exec(input1);', 'results');
//
//var regex90 = /a\Cb/;
//var input0 = "aXb";
//var results = ["aXb"];
//shouldBe('regex90.exec(input0);', 'results');
//var input1 = "a\nb";
//var results = ["a\u000ab"];
//shouldBe('regex90.exec(input1);', 'results');
//// Failers
//var input2 = "a\u0100b";
//var results = null;
//shouldBe('regex90.exec(input2);', 'results');
