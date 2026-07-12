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

// Test postfix count operations with smis.

function f1() { var x = 0x3fffffff; x++; return x; }
assertEquals(0x40000000, f1());


function f2() { var x = -0x40000000; x--; return x; }
assertEquals(-0x40000001, f2());


function f3(x) { x = x & 0x3fffffff; x++; return x; }
assertEquals(0x40000000, f3(0x3fffffff));


function f4() {
  var i;
  for (i = 0x3ffffffe; i <= 0x3fffffff; i++) {}
  return i;
}
assertEquals(0x40000000, f4());


function f5() {
  var i;
  for (i = -0x3fffffff; i >= -0x40000000; i--) {}
  return i;
}
assertEquals(-0x40000001, f5());


function f6() { var x = 0x3fffffff; x++; return x+1; }
assertEquals(0x40000001, f6());


function f7() {
  var i;
  for (i = 0x3ffffffd; i <= 0x3ffffffe; i++) {}
  i++; i = i + 1;
  return i;
}
assertEquals(0x40000001, f7());


function f8() {
  var i;
  for (i = 0x3ffffffd; i <= 0x3fffffff; i++) {}
  i++; i++;
  return i;
}
assertEquals(0x40000002, f8());


function f9() {
  var i;
  for (i = 0; i < 42; i++) {
    return 42;
  }
}
assertEquals(42, f9());


function f10(x) {
  for (x = 0; x < 4; x++) {}
}
f10(42);
