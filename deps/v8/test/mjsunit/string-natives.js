// Copyright 2012 the V8 project authors. All rights reserved.
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

// Flags: --expose-gc --allow-natives-syntax

function test() {
  var s1 = %NewString(26, true);
  for (i = 0; i < 26; i++) %_OneByteSeqStringSetChar(i, 65, s1);
  assertEquals("AAAAAAAAAAAAAAAAAAAAAAAAAA", s1);
  %_OneByteSeqStringSetChar(25, 66, s1);
  assertEquals("AAAAAAAAAAAAAAAAAAAAAAAAAB", s1);
  for (i = 0; i < 26; i++) %_OneByteSeqStringSetChar(i, i+65, s1);
  assertEquals("ABCDEFGHIJKLMNOPQRSTUVWXYZ", s1);
  s1 = %TruncateString(s1, 13);
  assertEquals("ABCDEFGHIJKLM", s1);

  var s2 = %NewString(26, false);
  for (i = 0; i < 26; i++) %_TwoByteSeqStringSetChar(i, 65, s2);
  assertEquals("AAAAAAAAAAAAAAAAAAAAAAAAAA", s2);
  %_TwoByteSeqStringSetChar(25, 66, s2);
  assertEquals("AAAAAAAAAAAAAAAAAAAAAAAAAB", s2);
  for (i = 0; i < 26; i++) %_TwoByteSeqStringSetChar(i, i+65, s2);
  assertEquals("ABCDEFGHIJKLMNOPQRSTUVWXYZ", s2);
  s2 = %TruncateString(s2, 13);
  assertEquals("ABCDEFGHIJKLM", s2);

  var s3 = %NewString(26, false);
  for (i = 0; i < 26; i++) %_TwoByteSeqStringSetChar(i, i+1000, s3);
  for (i = 0; i < 26; i++) assertEquals(s3[i], String.fromCharCode(i+1000));

  var a = [];
  for (var i = 0; i < 1000; i++) {
    var s = %NewString(10000, i % 2 == 1);
    a.push(s);
  }

  gc();

  for (var i = 0; i < 1000; i++) {
    assertEquals(10000, a[i].length);
    a[i] = %TruncateString(a[i], 5000);
  }

  gc();

  for (var i = 0; i < 1000; i++) {
    assertEquals(5000, a[i].length);
  }
}


test();
test();
%OptimizeFunctionOnNextCall(test);
test();
