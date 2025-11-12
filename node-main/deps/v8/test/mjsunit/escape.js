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

// Flags: --allow-natives-syntax

/**
 * @fileoverview Check that the global escape and unescape functions work
 * right.
 */

// Section B.2.1 of ECMAScript 3
var unescaped = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789@*_+-./";

// Check the unescape chars are not escaped
assertEquals(unescaped, escape(unescaped));
// Check spaces are escaped
assertEquals("%20/%20", escape(" / "));
// Check that null chars are escaped and do not terminate the string
assertEquals("%000", escape("\0" + "0"));
// Check a unicode escape
assertEquals("A%20B%u1234%00%20C", escape(String.fromCharCode(0x41, 0x20, 0x42, 0x1234, 0, 0x20, 0x43)));
// Check unicode escapes have a leading zero to pad to 4 digits
assertEquals("%u0123", escape(String.fromCharCode(0x123)));
// Check escapes are upper case
assertEquals("%uABCD", escape(String.fromCharCode(0xabcd)));
assertEquals("%AB", escape(String.fromCharCode(0xab)));
assertEquals("%0A", escape("\n"));

// Check first 1000 chars individually for escaped/not escaped
for (var i = 0; i < 1000; i++) {
  var s = String.fromCharCode(i);
  if (unescaped.indexOf(s, 0) == -1) {
    assertFalse(s == escape(s));
  } else {
    assertTrue(s == escape(s));
  }
}

// Check all chars up to 1000 in groups of 10 using unescape as a check
for (var i = 0; i < 1000; i += 10) {
  var s = String.fromCharCode(i, i+1, i+2, i+3, i+4, i+5, i+6, i+7, i+8, i+9);
  assertEquals(s, unescape(escape(s)));
}

// Benchmark
var example = "Now is the time for all good men to come to the aid of the party.";
example = example + String.fromCharCode(267, 0x1234, 0x6667, 0xabcd);
example = example + " The quick brown fox jumps over the lazy dog."
example = example + String.fromCharCode(171, 172, 173, 174, 175, 176, 178, 179);


function testRoundTrip() {
  assertEquals(example, unescape(escape(example)));
}
%PrepareFunctionForOptimization(testRoundTrip);
for (var i = 0; i < 3; i++) {
  testRoundTrip();
};
%OptimizeFunctionOnNextCall(testRoundTrip);
testRoundTrip();

// Check unescape can cope with upper and lower case
assertEquals(unescape("%41%4A%4a"), "AJJ");

// Check upper case U
assertEquals("%U1234", unescape("%U1234"));

// Check malformed unescapes
assertEquals("%", unescape("%"));
assertEquals("%4", unescape("%4"));
assertEquals("%u", unescape("%u"));
assertEquals("%u4", unescape("%u4"));
assertEquals("%u44", unescape("%u44"));
assertEquals("%u444", unescape("%u444"));
assertEquals("%4z", unescape("%4z"));
assertEquals("%uzzzz", unescape("%uzzzz"));
assertEquals("%u4zzz", unescape("%u4zzz"));
assertEquals("%u44zz", unescape("%u44zz"));
assertEquals("%u444z", unescape("%u444z"));
assertEquals("%4+", unescape("%4+"));
assertEquals("%u++++", unescape("%u++++"));
assertEquals("%u4+++", unescape("%u4+++"));
assertEquals("%u44++", unescape("%u44++"));
assertEquals("%u444+", unescape("%u444+"));
assertEquals("foo%4+", unescape("foo%4+"));
assertEquals("foo%u++++", unescape("foo%u++++"));
assertEquals("foo%u4+++", unescape("foo%u4+++"));
assertEquals("foo%u44++", unescape("foo%u44++"));
assertEquals("foo%u444+", unescape("foo%u444+"));
assertEquals("foo%4+bar", unescape("foo%4+bar"));
assertEquals("foo%u++++bar", unescape("foo%u++++bar"));
assertEquals("foo%u4+++bar", unescape("foo%u4+++bar"));
assertEquals("foo%u44++bar", unescape("foo%u44++bar"));
assertEquals("foo%u444+bar", unescape("foo%u444+bar"));
assertEquals("% ", unescape("%%20"));
assertEquals("%% ", unescape("%%%20"));

// Unescape stress
var eexample = escape(example);
function stressTestUnescape() {
  assertEquals(example, unescape(eexample));
}
%PrepareFunctionForOptimization(stressTestUnescape);
for (var i = 0; i < 3; i++) {
  stressTestUnescape();
}
%OptimizeFunctionOnNextCall(stressTestUnescape);
stressTestUnescape();
