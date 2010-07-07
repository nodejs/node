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

// Flags: --expose-externalize-string

assertEquals("0123", "aa0bb1cc2dd3".replace(/[a-z]/g, ""));
assertEquals("0123", "\u1234a0bb1cc2dd3".replace(/[\u1234a-z]/g, ""));

var expected = "0123";
var cons = "a0b1c2d3";
for (var i = 0; i < 5; i++) {
  expected += expected;
  cons += cons;
}
assertEquals(expected, cons.replace(/[a-z]/g, ""));
cons = "\u12340b1c2d3";
for (var i = 0; i < 5; i++) {
  cons += cons;
}
assertEquals(expected, cons.replace(/[\u1234a-z]/g, ""));

cons = "a0b1c2d3";
for (var i = 0; i < 5; i++) {
  cons += cons;
}
externalizeString(cons, true/* force two-byte */);
assertEquals(expected, cons.replace(/[a-z]/g, ""));
cons = "\u12340b1c2d3";
for (var i = 0; i < 5; i++) {
  cons += cons;
}
externalizeString(cons);
assertEquals(expected, cons.replace(/[\u1234a-z]/g, ""));
