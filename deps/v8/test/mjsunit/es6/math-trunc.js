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

assertEquals("Infinity", String(1/Math.trunc(0)));
assertEquals("-Infinity", String(1/Math.trunc(-0)));
assertEquals("Infinity", String(1/Math.trunc(Math.PI/4)));
assertEquals("-Infinity", String(1/Math.trunc(-Math.sqrt(2)/2)));
assertEquals(100, Math.trunc(100));
assertEquals(-199, Math.trunc(-199));
assertEquals(100, Math.trunc(100.1));
assertTrue(isNaN(Math.trunc("abc")));
assertTrue(isNaN(Math.trunc({})));
assertEquals(0, Math.trunc([]));
assertEquals(1, Math.trunc([1]));
assertEquals(-100, Math.trunc([-100.1]));
assertTrue(isNaN(Math.trunc([1, 1])));
assertEquals(-100, Math.trunc({ toString: function() { return "-100.3"; } }));
assertEquals(10, Math.trunc({ toString: function() { return 10.1; } }));
assertEquals(-1, Math.trunc({ valueOf: function() { return -1.1; } }));
assertEquals("-Infinity",
             String(1/Math.trunc({ valueOf: function() { return "-0.1"; } })));
assertEquals("-Infinity", String(Math.trunc(-Infinity)));
assertEquals("Infinity", String(Math.trunc(Infinity)));
assertEquals("-Infinity", String(Math.trunc("-Infinity")));
assertEquals("Infinity", String(Math.trunc("Infinity")));
