// Copyright 2010 the V8 project authors. All rights reserved.
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

// Flags: --max-new-space-size=256

function zero() {
  var x = 0.5;
  return (function() { return x - 0.5; })();
}

function test() {
  assertEquals(0, Math.floor(0));
  assertEquals(0, Math.floor(zero()));
  assertEquals(1/-0, 1/Math.floor(-0));  // 0 == -0, so we use reciprocals.
  assertEquals(Infinity, Math.floor(Infinity));
  assertEquals(-Infinity, Math.floor(-Infinity));
  assertNaN(Math.floor(NaN));

  assertEquals(0, Math.floor(0.1));
  assertEquals(0, Math.floor(0.5));
  assertEquals(0, Math.floor(0.7));
  assertEquals(-1, Math.floor(-0.1));
  assertEquals(-1, Math.floor(-0.5));
  assertEquals(-1, Math.floor(-0.7));
  assertEquals(1, Math.floor(1));
  assertEquals(1, Math.floor(1.1));
  assertEquals(1, Math.floor(1.5));
  assertEquals(1, Math.floor(1.7));
  assertEquals(-1, Math.floor(-1));
  assertEquals(-2, Math.floor(-1.1));
  assertEquals(-2, Math.floor(-1.5));
  assertEquals(-2, Math.floor(-1.7));

  assertEquals(0, Math.floor(Number.MIN_VALUE));
  assertEquals(-1, Math.floor(-Number.MIN_VALUE));
  assertEquals(Number.MAX_VALUE, Math.floor(Number.MAX_VALUE));
  assertEquals(-Number.MAX_VALUE, Math.floor(-Number.MAX_VALUE));
  assertEquals(Infinity, Math.floor(Infinity));
  assertEquals(-Infinity, Math.floor(-Infinity));

  // 2^30 is a smi boundary.
  var two_30 = 1 << 30;

  assertEquals(two_30, Math.floor(two_30));
  assertEquals(two_30, Math.floor(two_30 + 0.1));
  assertEquals(two_30, Math.floor(two_30 + 0.5));
  assertEquals(two_30, Math.floor(two_30 + 0.7));

  assertEquals(two_30 - 1, Math.floor(two_30 - 1));
  assertEquals(two_30 - 1, Math.floor(two_30 - 1 + 0.1));
  assertEquals(two_30 - 1, Math.floor(two_30 - 1 + 0.5));
  assertEquals(two_30 - 1, Math.floor(two_30 - 1 + 0.7));

  assertEquals(-two_30, Math.floor(-two_30));
  assertEquals(-two_30, Math.floor(-two_30 + 0.1));
  assertEquals(-two_30, Math.floor(-two_30 + 0.5));
  assertEquals(-two_30, Math.floor(-two_30 + 0.7));

  assertEquals(-two_30 + 1, Math.floor(-two_30 + 1));
  assertEquals(-two_30 + 1, Math.floor(-two_30 + 1 + 0.1));
  assertEquals(-two_30 + 1, Math.floor(-two_30 + 1 + 0.5));
  assertEquals(-two_30 + 1, Math.floor(-two_30 + 1 + 0.7));

  // 2^52 is a precision boundary.
  var two_52 = (1 << 30) * (1 << 22);

  assertEquals(two_52, Math.floor(two_52));
  assertEquals(two_52, Math.floor(two_52 + 0.1));
  assertEquals(two_52, two_52 + 0.5);
  assertEquals(two_52, Math.floor(two_52 + 0.5));
  assertEquals(two_52 + 1, two_52 + 0.7);
  assertEquals(two_52 + 1, Math.floor(two_52 + 0.7));

  assertEquals(two_52 - 1, Math.floor(two_52 - 1));
  assertEquals(two_52 - 1, Math.floor(two_52 - 1 + 0.1));
  assertEquals(two_52 - 1, Math.floor(two_52 - 1 + 0.5));
  assertEquals(two_52 - 1, Math.floor(two_52 - 1 + 0.7));

  assertEquals(-two_52, Math.floor(-two_52));
  assertEquals(-two_52, Math.floor(-two_52 + 0.1));
  assertEquals(-two_52, Math.floor(-two_52 + 0.5));
  assertEquals(-two_52, Math.floor(-two_52 + 0.7));

  assertEquals(-two_52 + 1, Math.floor(-two_52 + 1));
  assertEquals(-two_52 + 1, Math.floor(-two_52 + 1 + 0.1));
  assertEquals(-two_52 + 1, Math.floor(-two_52 + 1 + 0.5));
  assertEquals(-two_52 + 1, Math.floor(-two_52 + 1 + 0.7));
}


// Test in a loop to cover the custom IC and GC-related issues.
for (var i = 0; i < 500; i++) {
  test();
}
