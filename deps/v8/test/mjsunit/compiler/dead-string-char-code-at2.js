// Copyright 2013 the V8 project authors. All rights reserved.
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

var S1 = "string1";
var S2 = "@@string2";

function dead1(a, b) {
    var x = %_StringCharCodeAt(a, 4);
    return a; // x is dead code
}

function dead2(a, b) {
    var x = %_StringCharCodeAt(a, 3);
    var y = %_StringCharCodeAt(b, 1);
    return a; // x and y are both dead
}

function dead3(a, b) {
    a = a ? "11" : "12";
    b = b ? "13" : "14";
    var x = %_StringCharCodeAt(a, 2);
    var y = %_StringCharCodeAt(b, 0);
    return a; // x and y are both dead
}

function test() {
  var S3 = S1 + S2;

  assertEquals(S1, dead1(S1, S2));
  assertEquals(S1, dead2(S1, S2));
  assertEquals("11", dead3(S1, S2));

  assertEquals(S2, dead1(S2, 677));
  assertEquals(S2, dead2(S2, S3));
  assertEquals("11", dead3(S2, S3));

  assertEquals(S3, dead1(S3, 399));
  assertEquals(S3, dead2(S3, "false"));
  assertEquals("12", dead3(0, 32));

  assertEquals(S3, dead1(S3, 0));
  assertEquals(S3, dead2(S3, S1));
  assertEquals("11", dead3(S3, 0));

  assertEquals("true", dead1("true", 0));
  assertEquals("true", dead2("true", S3));
  assertEquals("11", dead3("true", 0));
}

test();
test();
%OptimizeFunctionOnNextCall(dead1);
%OptimizeFunctionOnNextCall(dead2);
%OptimizeFunctionOnNextCall(dead3);
test();
