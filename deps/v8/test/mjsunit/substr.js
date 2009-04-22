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

var s = 'abcdefghijklmn';
assertEquals(s, s.substr());
assertEquals(s, s.substr(0));
assertEquals(s, s.substr('0'));
assertEquals(s, s.substr(void 0));
assertEquals(s, s.substr(null));
assertEquals(s, s.substr(false));
assertEquals(s, s.substr(0.9));
assertEquals(s, s.substr({ valueOf: function() { return 0; } }));
assertEquals(s, s.substr({ toString: function() { return '0'; } }));

var s1 = s.substring(1);
assertEquals(s1, s.substr(1));
assertEquals(s1, s.substr('1'));
assertEquals(s1, s.substr(true));
assertEquals(s1, s.substr(1.1));
assertEquals(s1, s.substr({ valueOf: function() { return 1; } }));
assertEquals(s1, s.substr({ toString: function() { return '1'; } }));

for (var i = 0; i < s.length; i++)
  for (var j = i; j < s.length + 5; j++)
    assertEquals(s.substring(i, j), s.substr(i, j - i));

assertEquals(s.substring(s.length - 1), s.substr(-1));
assertEquals(s.substring(s.length - 1), s.substr(-1.2));
assertEquals(s.substring(s.length - 1), s.substr(-1.7));
assertEquals(s.substring(s.length - 2), s.substr(-2));
assertEquals(s.substring(s.length - 2), s.substr(-2.3));
assertEquals(s.substring(s.length - 2, s.length - 1), s.substr(-2, 1));
assertEquals(s, s.substr(-100));
assertEquals('abc', s.substr(-100, 3));
assertEquals(s1, s.substr(-s.length + 1));

// assertEquals('', s.substr(0, void 0)); // smjs and rhino 
assertEquals('abcdefghijklmn', s.substr(0, void 0));  // kjs and v8
assertEquals('', s.substr(0, null));
assertEquals(s, s.substr(0, String(s.length)));
assertEquals('a', s.substr(0, true));
