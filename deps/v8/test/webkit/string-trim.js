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

// Flags: --harmony-string-trimming

description("This test checks the `trim`, `trimStart`/`trimLeft`, and `trimEnd`/`trimRight` methods on `String.prototype`.");

// References to trim(), trimLeft() and trimRight() functions for testing Function's *.call() and *.apply() methods
const trim      = String.prototype.trim;
const trimStart = String.prototype.trimStart;
const trimLeft  = String.prototype.trimLeft;
const trimEnd   = String.prototype.trimEnd;
const trimRight = String.prototype.trimRight;

const whitespace = [
  {s: '\u0009', t: 'HORIZONTAL TAB'},
  {s: '\u000A', t: 'LINE FEED OR NEW LINE'},
  {s: '\u000B', t: 'VERTICAL TAB'},
  {s: '\u000C', t: 'FORMFEED'},
  {s: '\u000D', t: 'CARRIAGE RETURN'},
  {s: '\u0020', t: 'SPACE'},
  {s: '\u00A0', t: 'NO-BREAK SPACE'},
  {s: '\u2000', t: 'EN QUAD'},
  {s: '\u2001', t: 'EM QUAD'},
  {s: '\u2002', t: 'EN SPACE'},
  {s: '\u2003', t: 'EM SPACE'},
  {s: '\u2004', t: 'THREE-PER-EM SPACE'},
  {s: '\u2005', t: 'FOUR-PER-EM SPACE'},
  {s: '\u2006', t: 'SIX-PER-EM SPACE'},
  {s: '\u2007', t: 'FIGURE SPACE'},
  {s: '\u2008', t: 'PUNCTUATION SPACE'},
  {s: '\u2009', t: 'THIN SPACE'},
  {s: '\u200A', t: 'HAIR SPACE'},
  {s: '\u3000', t: 'IDEOGRAPHIC SPACE'},
  {s: '\u2028', t: 'LINE SEPARATOR'},
  {s: '\u2029', t: 'PARAGRAPH SEPARATOR'},
  {s: '\u200B', t: 'ZERO WIDTH SPACE (category Cf)'},
];

let wsString = '';
for (let i = 0; i < whitespace.length; i++) {
  shouldBe("whitespace["+i+"].s.trim()",      "''");
  shouldBe("whitespace["+i+"].s.trimStart()", "''");
  shouldBe("whitespace["+i+"].s.trimLeft()",  "''");
  shouldBe("whitespace["+i+"].s.trimEnd()",   "''");
  shouldBe("whitespace["+i+"].s.trimRight()", "''");
  wsString += whitespace[i].s;
}

const testString = 'foo bar';
const trimString      = wsString   + testString + wsString;
const leftTrimString  = testString + wsString;   //trimmed from the left
const rightTrimString = wsString   + testString; //trimmed from the right

shouldBe("wsString.trim()",      "''");
shouldBe("wsString.trimStart()", "''");
shouldBe("wsString.trimLeft()",  "''");
shouldBe("wsString.trimEnd()",   "''");
shouldBe("wsString.trimRight()", "''");

shouldBe("trimString.trim()",      "testString");
shouldBe("trimString.trimStart()", "leftTrimString");
shouldBe("trimString.trimLeft()",  "leftTrimString");
shouldBe("trimString.trimEnd()",   "rightTrimString");
shouldBe("trimString.trimRight()", "rightTrimString");

shouldBe("leftTrimString.trim()",      "testString");
shouldBe("leftTrimString.trimStart()", "leftTrimString");
shouldBe("leftTrimString.trimLeft()",  "leftTrimString");
shouldBe("leftTrimString.trimEnd()",   "testString");
shouldBe("leftTrimString.trimRight()", "testString");

shouldBe("rightTrimString.trim()",      "testString");
shouldBe("rightTrimString.trimStart()", "testString");
shouldBe("rightTrimString.trimLeft()",  "testString");
shouldBe("rightTrimString.trimEnd()",   "rightTrimString");
shouldBe("rightTrimString.trimRight()", "rightTrimString");

const testValues = [
  "0",
  "Infinity",
  "NaN",
  "true",
  "false",
  "({})",
  "({toString:function(){return 'wibble'}})",
  "['an','array']",
];
for (const testValue of testValues) {
  shouldBe("trim.call("+testValue+")", "'"+eval(testValue)+"'");
  shouldBe("trimStart.call("+testValue+")", "'"+eval(testValue)+"'");
  shouldBe("trimLeft.call("+testValue+")", "'"+eval(testValue)+"'");
  shouldBe("trimEnd.call("+testValue+")", "'"+eval(testValue)+"'");
  shouldBe("trimRight.call("+testValue+")", "'"+eval(testValue)+"'");
}
