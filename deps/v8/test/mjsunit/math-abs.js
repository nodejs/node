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
  assertEquals(0, Math.abs(0));
  assertEquals(0, Math.abs(zero()));
  assertEquals(1/0, 1/Math.abs(-0));  // 0 == -0, so we use reciprocals.
  assertEquals(Infinity, Math.abs(Infinity));
  assertEquals(Infinity, Math.abs(-Infinity));
  assertNaN(Math.abs(NaN));
  assertNaN(Math.abs(-NaN));
  assertEquals('Infinity', Math.abs(Number('+Infinity').toString()));
  assertEquals('Infinity', Math.abs(Number('-Infinity').toString()));
  assertEquals('NaN', Math.abs(NaN).toString());
  assertEquals('NaN', Math.abs(-NaN).toString());

  assertEquals(0.1, Math.abs(0.1));
  assertEquals(0.5, Math.abs(0.5));
  assertEquals(0.1, Math.abs(-0.1));
  assertEquals(0.5, Math.abs(-0.5));
  assertEquals(1, Math.abs(1));
  assertEquals(1.1, Math.abs(1.1));
  assertEquals(1.5, Math.abs(1.5));
  assertEquals(1, Math.abs(-1));
  assertEquals(1.1, Math.abs(-1.1));
  assertEquals(1.5, Math.abs(-1.5));

  assertEquals(Number.MIN_VALUE, Math.abs(Number.MIN_VALUE));
  assertEquals(Number.MIN_VALUE, Math.abs(-Number.MIN_VALUE));
  assertEquals(Number.MAX_VALUE, Math.abs(Number.MAX_VALUE));
  assertEquals(Number.MAX_VALUE, Math.abs(-Number.MAX_VALUE));

  // 2^30 is a smi boundary on arm and ia32.
  var two_30 = 1 << 30;

  assertEquals(two_30, Math.abs(two_30));
  assertEquals(two_30, Math.abs(-two_30));

  assertEquals(two_30 + 1, Math.abs(two_30 + 1));
  assertEquals(two_30 + 1, Math.abs(-two_30 - 1));

  assertEquals(two_30 - 1, Math.abs(two_30 - 1));
  assertEquals(two_30 - 1, Math.abs(-two_30 + 1));

  // 2^31 is a smi boundary on x64.
  var two_31 = 2 * two_30;

  assertEquals(two_31, Math.abs(two_31));
  assertEquals(two_31, Math.abs(-two_31));

  assertEquals(two_31 + 1, Math.abs(two_31 + 1));
  assertEquals(two_31 + 1, Math.abs(-two_31 - 1));

  assertEquals(two_31 - 1, Math.abs(two_31 - 1));
  assertEquals(two_31 - 1, Math.abs(-two_31 + 1));

  assertNaN(Math.abs("not a number"));
  assertNaN(Math.abs([1, 2, 3]));
  assertEquals(42, Math.abs({valueOf: function() { return 42; } }));
  assertEquals(42, Math.abs({valueOf: function() { return -42; } }));
}


// Test in a loop to cover the custom IC and GC-related issues.
for (var i = 0; i < 500; i++) {
  test();
}
