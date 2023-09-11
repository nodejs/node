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

const SMI_MAX = (1 << 30) - 1;
const SMI_MIN = -(1 << 30);

function testmulneg(a, b) {
  var base = a * b;
  assertEquals(-base, a * -b, "a * -b where a = " + a + ", b = " + b);
  assertEquals(-base, -a * b, "-a * b where a = " + a + ", b = " + b);
  assertEquals(base, -a * -b, "*-a * -b where a = " + a + ", b = " + b);
}

testmulneg(2, 3);
testmulneg(SMI_MAX, 3);
testmulneg(SMI_MIN, 3);
testmulneg(3.2, 2.3);

var x = { valueOf: function() { return 2; } };
var y = { valueOf: function() { return 3; } };

testmulneg(x, y);

// The test below depends on the correct evaluation order, which is not
// implemented by any of the known JS engines.
var z;
var v = { valueOf: function() { z+=2; return z; } };
var w = { valueOf: function() { z+=3; return z; } };

z = 0;
var base = v * w;
z = 0;
assertEquals(-base, -v * w);
z = 0;
assertEquals(base, -v * -w);
