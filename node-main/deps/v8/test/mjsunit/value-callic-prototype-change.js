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

// Test that the inline caches correctly detect that constant
// functions on value prototypes change.

function testString() {
  function f(s, expected) {
    var result = s.toString();
    assertEquals(expected, result);
  };

  for (var i = 0; i < 10; i++) {
    var s = String.fromCharCode(i);
    f(s, s);
  }

  String.prototype.toString = function() { return "ostehaps"; };

  for (var i = 0; i < 10; i++) {
    var s = String.fromCharCode(i);
    f(s, "ostehaps");
  }
}

testString();


function testNumber() {
  Number.prototype.toString = function() { return 0; };

  function f(n, expected) {
    var result = n.toString();
    assertEquals(expected, result);
  };

  for (var i = 0; i < 10; i++) {
    f(i, 0);
  }

  Number.prototype.toString = function() { return 42; };

  for (var i = 0; i < 10; i++) {
    f(i, 42);
  }
}

testNumber();


function testBoolean() {
  Boolean.prototype.toString = function() { return 0; };

  function f(b, expected) {
    var result = b.toString();
    assertEquals(expected, result);
  };

  for (var i = 0; i < 10; i++) {
    f((i % 2 == 0), 0);
  }

  Boolean.prototype.toString = function() { return 42; };

  for (var i = 0; i < 10; i++) {
    f((i % 2 == 0), 42);
  }
}

testBoolean();
