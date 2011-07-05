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

// Test const properties and pre/postfix operation.
function f() {
  const x = 1;
  x++;
  assertEquals(1, x);
  x--;
  assertEquals(1, x);
  ++x;
  assertEquals(1, x);
  --x;
  assertEquals(1, x);
  assertEquals(1, x++);
  assertEquals(1, x--);
  assertEquals(2, ++x);
  assertEquals(0, --x);
}

f();

// Test that the value is read eventhough assignment is disallowed.
// Spidermonkey does not do this, but it seems like the right thing to
// do so that 'o++' is equivalent to 'o = o + 1'.
var valueOfCount = 0;

function g() {
  const o = { valueOf: function() { valueOfCount++; return 42; } }
  assertEquals(42, o);
  assertEquals(1, valueOfCount);
  o++;
  assertEquals(42, o);
  assertEquals(3, valueOfCount);
  ++o;
  assertEquals(42, o);
  assertEquals(5, valueOfCount);
  o--;
  assertEquals(42, o);
  assertEquals(7, valueOfCount);
  --o;
  assertEquals(42, o);
  assertEquals(9, valueOfCount);
}

g();
