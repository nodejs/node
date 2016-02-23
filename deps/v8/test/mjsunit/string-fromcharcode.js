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

// Flags: --allow-natives-syntax

// Test String.fromCharCode.

// Test char codes larger than 0xffff.
var expected = "";
for (var i = 100; i < 500; i++) {
  expected += String.fromCharCode(i);
}

function testCharCodeTruncation() {
  var result = "";
  for (var i = 0x100000 + 100; i < 0x100000 + 500; i++) {
    result += String.fromCharCode(i);
  }
  assertEquals(String.fromCharCode(0xFFFF), String.fromCharCode(0xFFFFFFFF));
  return result;
}

assertEquals(expected, testCharCodeTruncation());
assertEquals(expected, testCharCodeTruncation());
%OptimizeFunctionOnNextCall(testCharCodeTruncation);
assertEquals(expected, testCharCodeTruncation());

// Test various receivers and arguments passed to String.fromCharCode.

Object.prototype.fromCharCode = function(x) { return this; };

var fcc = String.fromCharCode;
var fcc2 = fcc;

function constFun(x) { return function(y) { return x; }; }

function test(num) {
  assertEquals(" ", String.fromCharCode(0x20));
  assertEquals(" ", String.fromCharCode(0x20 + 0x10000));
  assertEquals(" ", String.fromCharCode(0x20 - 0x10000));
  assertEquals(" ", String.fromCharCode(0x20 + 0.5));

  assertEquals("\u1234", String.fromCharCode(0x1234));
  assertEquals("\u1234", String.fromCharCode(0x1234 + 0x10000));
  assertEquals("\u1234", String.fromCharCode(0x1234 - 0x10000));
  assertEquals("\u1234", String.fromCharCode(0x1234 + 0.5));

  assertEquals("  ", String.fromCharCode(0x20, 0x20));
  assertEquals("  ", String.fromCharCode(0x20 + 0.5, 0x20));

  assertEquals(" ", fcc(0x20));
  assertEquals(" ", fcc(0x20 + 0x10000));
  assertEquals(" ", fcc(0x20 - 0x10000));
  assertEquals(" ", fcc(0x20 + 0.5));

  assertEquals("\u1234", fcc(0x1234));
  assertEquals("\u1234", fcc(0x1234 + 0x10000));
  assertEquals("\u1234", fcc(0x1234 - 0x10000));
  assertEquals("\u1234", fcc(0x1234 + 0.5));

  assertEquals("  ", fcc(0x20, 0x20));
  assertEquals("  ", fcc(0x20 + 0.5, 0x20));

  var receiver = (num < 5) ? String : (num < 9) ? "dummy" : 42;
  fcc2 = (num < 5) ? fcc
                   : (num < 9) ? constFun(Object("dummy"))
                               : constFun(Object(42));
  var expected = (num < 5) ? " " : (num < 9) ? Object("dummy") : Object(42);
  assertEquals(expected, receiver.fromCharCode(0x20));
  assertEquals(expected, receiver.fromCharCode(0x20 - 0x10000));
  assertEquals(expected, receiver.fromCharCode(0x20 + 0.5));
  assertEquals(expected, fcc2(0x20));
  assertEquals(expected, fcc2(0x20 - 0x10000));
  assertEquals(expected, fcc2(0x20 + 0.5));
}

// Use loop to test the custom IC.
for (var i = 0; i < 10; i++) {
  test(i);
}

assertEquals("AAAA", String.fromCharCode(65, 65, 65, 65));
assertEquals("AAAA", String.fromCharCode(65, 65, 65, 65));
%OptimizeFunctionOnNextCall(String.fromCharCode);
assertEquals("AAAA", String.fromCharCode(65, 65, 65, 65));

// Test the custom IC works correctly when the map changes.
for (var i = 0; i < 10; i++) {
  var expected = (i < 5) ? " " : 42;
  if (i == 5) String.fromCharCode = function() { return 42; };
  assertEquals(expected, String.fromCharCode(0x20));
}
