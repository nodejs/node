// Copyright 2008 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

/* Many of the Mozilla regexp tests used 'toSource' to test their
 * results.  Since we don't currently support toSource, those tests
 * are disabled and standalone versions are included here.
 */

// Tests from ecma_3/RegExp/regress-78156.js
var string = 'aaa\n789\r\nccc\r\n345';
var pattern = /^\d/gm;
var result = string.match(pattern);
assertEquals(2, result.length, "1");
assertEquals('7', result[0], "2");
assertEquals('3', result[1], "3");

pattern = /\d$/gm;
result = string.match(pattern);
assertEquals(2, result.length, "4");
assertEquals('9', result[0], "5");
assertEquals('5', result[1], "6");

string = 'aaa\n789\r\nccc\r\nddd';
pattern = /^\d/gm;
result = string.match(pattern);
assertEquals(1, result.length, "7");
assertEquals('7', result[0], "8");

pattern = /\d$/gm;
result = string.match(pattern);
assertEquals(1, result.length, "9");
assertEquals('9', result[0], "10");

// Tests from ecma_3/RegExp/regress-72964.js
pattern = /[\S]+/;
string = '\u00BF\u00CD\u00BB\u00A7';
result = string.match(pattern);
assertEquals(1, result.length, "11");
assertEquals(string, result[0], "12");

string = '\u00BF\u00CD \u00BB\u00A7';
result = string.match(pattern);
assertEquals(1, result.length, "13");
assertEquals('\u00BF\u00CD', result[0], "14");

string = '\u4e00\uac00\u4e03\u4e00';
result = string.match(pattern);
assertEquals(1, result.length, "15");
assertEquals(string, result[0], "16");

string = '\u4e00\uac00 \u4e03\u4e00';
result = string.match(pattern);
assertEquals(1, result.length, "17");
assertEquals('\u4e00\uac00', result[0], "18");
