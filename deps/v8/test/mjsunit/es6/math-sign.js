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

assertEquals("Infinity", String(1/Math.sign(0)));
assertEquals("-Infinity", String(1/Math.sign(-0)));
assertEquals(1, Math.sign(100));
assertEquals(-1, Math.sign(-199));
assertEquals(1, Math.sign(100.1));
assertTrue(isNaN(Math.sign("abc")));
assertTrue(isNaN(Math.sign({})));
assertEquals(0, Math.sign([]));
assertEquals(1, Math.sign([1]));
assertEquals(-1, Math.sign([-100.1]));
assertTrue(isNaN(Math.sign([1, 1])));
assertEquals(1, Math.sign({ toString: function() { return "100"; } }));
assertEquals(1, Math.sign({ toString: function() { return 100; } }));
assertEquals(-1, Math.sign({ valueOf: function() { return -1.1; } }));
assertEquals(-1, Math.sign({ valueOf: function() { return "-1.1"; } }));
assertEquals(-1, Math.sign(-Infinity));
assertEquals(1, Math.sign(Infinity));
assertEquals(-1, Math.sign("-Infinity"));
assertEquals(1, Math.sign("Infinity"));
