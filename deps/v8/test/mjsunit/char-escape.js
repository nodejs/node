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

// Check that character escapes are understood as one char
var escapes = ["\b", "\t", "\n", "\v", "\f", "\r", "\"", "\'", "\\", "\x4a", "\u005f"];
for (var i = 0; i < escapes.length; i++) {
  var str = escapes[i];
  assertEquals(1, str.length);
  assertEquals(str, str.charAt(0));
}

function code(str) {
  return str.charCodeAt(0);
}

// Do the single escape chars have the right value?
assertEquals(0x08, code("\b"));
assertEquals(0x09, code("\t"));
assertEquals(0x0A, code("\n"));
assertEquals(0x0B, code("\v"));
assertEquals(0x0C, code("\f"));
assertEquals(0x0D, code("\r"));
assertEquals(0x22, code("\""));
assertEquals(0x27, code("\'"));
assertEquals(0x5c, code("\\"));

// Do the hex and unicode escape chars have the right value?
assertEquals(0x4a, code("\x4a"));
assertEquals(0x5f, code("\u005f"));
