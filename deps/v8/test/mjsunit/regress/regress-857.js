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

// Make sure ES5 15.9.1.15 (ISO 8601 / RFC 3339) time zone offsets of
// the form "+09:00" & "-09:00" get parsed as expected
assertEquals(1283326536000, Date.parse("2010-08-31T22:35:36-09:00"));
assertEquals(1283261736000, Date.parse("2010-08-31T22:35:36+09:00"));
assertEquals(1283326536000, Date.parse("2010-08-31T22:35:36.0-09:00"));
assertEquals(1283261736000, Date.parse("2010-08-31T22:35:36.0+09:00"));
// colon-less time expressions in time zone offsets are not conformant
// with ES5 15.9.1.15 but are nonetheless supported in V8
assertEquals(1283326536000, Date.parse("2010-08-31T22:35:36-0900"));
assertEquals(1283261736000, Date.parse("2010-08-31T22:35:36+0900"));
