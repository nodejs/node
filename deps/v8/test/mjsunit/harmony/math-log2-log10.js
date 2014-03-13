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

// Flags: --harmony-maths

[Math.log10, Math.log2].forEach( function(fun) {
  assertTrue(isNaN(fun(NaN)));
  assertTrue(isNaN(fun(fun)));
  assertTrue(isNaN(fun({ toString: function() { return NaN; } })));
  assertTrue(isNaN(fun({ valueOf: function() { return -1; } })));
  assertTrue(isNaN(fun({ valueOf: function() { return "abc"; } })));
  assertTrue(isNaN(fun(-0.1)));
  assertTrue(isNaN(fun(-1)));
  assertEquals("-Infinity", String(fun(0)));
  assertEquals("-Infinity", String(fun(-0)));
  assertEquals(0, fun(1));
  assertEquals("Infinity", String(fun(Infinity)));
});

for (var i = -300; i < 300; i += 0.7) {
  assertEqualsDelta(i, Math.log10(Math.pow(10, i)), 1E-13);
  assertEqualsDelta(i, Math.log2(Math.pow(2, i)), 1E-13);
}
