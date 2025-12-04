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

var s = "test test test";

var MAX_DOUBLE = 1.7976931348623157e+308;
var MIN_DOUBLE = -MAX_DOUBLE;
var MAX_SMI = Math.pow(2,30)-1;
var MIN_SMI = -Math.pow(2,30);

assertEquals(10, s.lastIndexOf("test", Infinity), "tinf");
assertEquals(10, s.lastIndexOf("test", MAX_DOUBLE), "tmaxdouble");
assertEquals(10, s.lastIndexOf("test", MAX_SMI), "tmaxsmi");
assertEquals(10, s.lastIndexOf("test", s.length * 2), "t2length");
assertEquals(10, s.lastIndexOf("test", 15), "t15");
assertEquals(10, s.lastIndexOf("test", 14), "t14");
assertEquals(10, s.lastIndexOf("test", 10), "t10");
assertEquals(5, s.lastIndexOf("test", 9), "t9");
assertEquals(5, s.lastIndexOf("test", 6), "t6");
assertEquals(5, s.lastIndexOf("test", 5), "t5");
assertEquals(0, s.lastIndexOf("test", 4), "t4");
assertEquals(0, s.lastIndexOf("test", 0), "t0");
assertEquals(0, s.lastIndexOf("test", -1), "t-1");
assertEquals(0, s.lastIndexOf("test", -s.length), "t-len");
assertEquals(0, s.lastIndexOf("test", MIN_SMI), "tminsmi");
assertEquals(0, s.lastIndexOf("test", MIN_DOUBLE), "tmindouble");
assertEquals(0, s.lastIndexOf("test", -Infinity), "tneginf");
assertEquals(10, s.lastIndexOf("test"), "t");
assertEquals(-1, s.lastIndexOf("notpresent"), "n");
assertEquals(-1, s.lastIndexOf(), "none");
assertEquals(10, s.lastIndexOf("test", "not a number"), "nan");

var longNonMatch = "overlong string that doesn't match";
var longAlmostMatch = "test test test!";
var longAlmostMatch2 = "!test test test";


assertEquals(-1, s.lastIndexOf(longNonMatch), "long");
assertEquals(-1, s.lastIndexOf(longNonMatch, 10), "longpos");
assertEquals(-1, s.lastIndexOf(longNonMatch, NaN), "longnan");
assertEquals(-1, s.lastIndexOf(longAlmostMatch), "tlong");
assertEquals(-1, s.lastIndexOf(longAlmostMatch, 10), "tlongpos");
assertEquals(-1, s.lastIndexOf(longAlmostMatch), "tlongnan");

var nonInitialMatch = "est";

assertEquals(-1, s.lastIndexOf(nonInitialMatch, 0), "noninit");
assertEquals(-1, s.lastIndexOf(nonInitialMatch, -1), "noninitneg");
assertEquals(-1, s.lastIndexOf(nonInitialMatch, MIN_SMI), "noninitminsmi");
assertEquals(-1, s.lastIndexOf(nonInitialMatch, MIN_DOUBLE), "noninitmindbl");
assertEquals(-1, s.lastIndexOf(nonInitialMatch, -Infinity), "noninitneginf");

for (var i = s.length + 10; i >= 0; i--) {
  var expected = i < s.length ? i : s.length;
  assertEquals(expected, s.lastIndexOf("", i), "empty" + i);
}


var reString = "asdf[a-z]+(asdf)?";

assertEquals(4, reString.lastIndexOf("[a-z]+"), "r4");
assertEquals(10, reString.lastIndexOf("(asdf)?"), "r10");

assertEquals(1, String.prototype.lastIndexOf.length, "length");
