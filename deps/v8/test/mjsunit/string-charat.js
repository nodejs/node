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

var s = "test";

assertEquals("t", s.charAt());
assertEquals("t", s.charAt("string"));
assertEquals("t", s.charAt(null));
assertEquals("t", s.charAt(void 0));
assertEquals("t", s.charAt(false));
assertEquals("e", s.charAt(true));
assertEquals("", s.charAt(-1));
assertEquals("", s.charAt(4));
assertEquals("t", s.charAt(0));
assertEquals("t", s.charAt(3));
assertEquals("t", s.charAt(NaN));

assertEquals(116, s.charCodeAt());
assertEquals(116, s.charCodeAt("string"));
assertEquals(116, s.charCodeAt(null));
assertEquals(116, s.charCodeAt(void 0));
assertEquals(116, s.charCodeAt(false));
assertEquals(101, s.charCodeAt(true));
assertEquals(116, s.charCodeAt(0));
assertEquals(116, s.charCodeAt(3));
assertEquals(116, s.charCodeAt(NaN));
assertTrue(isNaN(s.charCodeAt(-1)));
assertTrue(isNaN(s.charCodeAt(4)));

