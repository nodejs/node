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

var s = "test test test";

assertEquals(5, s.lastIndexOf("test", 5));
assertEquals(5, s.lastIndexOf("test", 6));
assertEquals(0, s.lastIndexOf("test", 4));
assertEquals(0, s.lastIndexOf("test", 0));
assertEquals(-1, s.lastIndexOf("test", -1));
assertEquals(10, s.lastIndexOf("test"));
assertEquals(-1, s.lastIndexOf("notpresent"));
assertEquals(-1, s.lastIndexOf());
assertEquals(10, s.lastIndexOf("test", "not a number"));

for (var i = s.length + 10; i >= 0; i--) {
  var expected = i < s.length ? i : s.length;
  assertEquals(expected, s.lastIndexOf("", i));
}


var reString = "asdf[a-z]+(asdf)?";

assertEquals(4, reString.lastIndexOf("[a-z]+"));
assertEquals(10, reString.lastIndexOf("(asdf)?"));

assertEquals(1, String.prototype.lastIndexOf.length);
