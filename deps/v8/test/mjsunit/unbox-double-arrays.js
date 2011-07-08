// Copyright 2011 the V8 project authors. All rights reserved.
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

// Test dictionary -> double elements -> dictionary elements round trip

var foo = new Array(500000);

function func(a) {
  for (var i= 0; i < 100000; ++i ) {
    a[i] = i+0.5;
  }
}

func(foo);

for (var i= 0; i < 100000; i += 500 ) {
  assertEquals(i+0.5, foo[i]);
}

delete foo[5];
// Don't use assertEquals for comparison to undefined due to
assertTrue(undefined === foo[5]);
assertTrue(undefined === foo[500000-1]);
assertTrue(undefined === foo[-1]);
assertEquals(500000, foo.length);

// Cause the array to grow beyond it's JSArray length. This will double the
// size of the capacity and force the array into "slow" dictionary case.
foo[500001] = 50;
assertEquals(50, foo[500001]);
assertEquals(500002, foo.length);
assertTrue(undefined === foo[5])
assertTrue(undefined === foo[500000-1])
assertTrue(undefined === foo[-1])
assertEquals(500002, foo.length);

// Test dictionary -> double elements -> fast elements.

var foo2 = new Array(500000);
func(foo2);
delete foo2[5];

// Convert back to fast elements and make sure the contents of the array are
// unchanged.
foo2[25] = new Object();
for (var i= 0; i < 100000; i += 500 ) {
  if (i != 25 && i != 5) {
    assertEquals(i+0.5, foo2[i]);
  }
}
assertTrue(undefined === foo2[5])
assertTrue(undefined === foo2[500000-1])
assertTrue(undefined === foo2[-1])
assertEquals(500000, foo2.length);
