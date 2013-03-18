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

// Tests of URI encoding and decoding.

assertEquals("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ-_.!~*'();/?:@&=+$,#",
             encodeURI("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ-_.!~*'();/?:@&=+$,#"));

var cc1 = 0x007D;
var s1 = String.fromCharCode(cc1);
var cc2 = 0x0000;
var s2 = String.fromCharCode(cc2);
var cc3 = 0x0080;
var s3 = String.fromCharCode(cc3);
var cc4 = 0x0555;
var s4 = String.fromCharCode(cc4);
var cc5 = 0x07FF;
var s5 = String.fromCharCode(cc5);
var cc6 = 0x0800;
var s6 = String.fromCharCode(cc6);
var cc7 = 0xAEEE;
var s7 = String.fromCharCode(cc7);
var cc8_1 = 0xD800;
var cc8_2 = 0xDC00;
var s8 = String.fromCharCode(cc8_1)+String.fromCharCode(cc8_2);
var cc9_1 = 0xDBFF;
var cc9_2 = 0xDFFF;
var s9 = String.fromCharCode(cc9_1)+String.fromCharCode(cc9_2);
var cc10 = 0xE000;
var s10 = String.fromCharCode(cc10);

assertEquals('%7D', encodeURI(s1));
assertEquals('%00', encodeURI(s2));
assertEquals('%C2%80', encodeURI(s3));
assertEquals('%D5%95', encodeURI(s4));
assertEquals('%DF%BF', encodeURI(s5));
assertEquals('%E0%A0%80', encodeURI(s6));
assertEquals('%EA%BB%AE', encodeURI(s7));
assertEquals('%F0%90%80%80', encodeURI(s8));
assertEquals('%F4%8F%BF%BF', encodeURI(s9));
assertEquals('%EE%80%80', encodeURI(s10));

assertEquals(cc1, decodeURI(encodeURI(s1)).charCodeAt(0));
assertEquals(cc2, decodeURI(encodeURI(s2)).charCodeAt(0));
assertEquals(cc3, decodeURI(encodeURI(s3)).charCodeAt(0));
assertEquals(cc4, decodeURI(encodeURI(s4)).charCodeAt(0));
assertEquals(cc5, decodeURI(encodeURI(s5)).charCodeAt(0));
assertEquals(cc6, decodeURI(encodeURI(s6)).charCodeAt(0));
assertEquals(cc7, decodeURI(encodeURI(s7)).charCodeAt(0));
assertEquals(cc8_1, decodeURI(encodeURI(s8)).charCodeAt(0));
assertEquals(cc8_2, decodeURI(encodeURI(s8)).charCodeAt(1));
assertEquals(cc9_1, decodeURI(encodeURI(s9)).charCodeAt(0));
assertEquals(cc9_2, decodeURI(encodeURI(s9)).charCodeAt(1));
assertEquals(cc10, decodeURI(encodeURI(s10)).charCodeAt(0));

assertEquals("", decodeURI(""));
assertEquals("", encodeURI(""));

function test(string) {
  assertEquals(string, decodeURI(encodeURI(string)));
}

test("\u1234\u0123\uabcd");
test("abcd");
test("ab<\u1234\u0123");
test("ab\u1234<\u0123");
