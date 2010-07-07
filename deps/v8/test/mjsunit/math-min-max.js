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

// Flags: --allow-natives-syntax

// Test Math.min().

assertEquals(Infinity, Math.min());
assertEquals(1, Math.min(1));
assertEquals(1, Math.min(1, 2));
assertEquals(1, Math.min(2, 1));
assertEquals(1, Math.min(1, 2, 3));
assertEquals(1, Math.min(3, 2, 1));
assertEquals(1, Math.min(2, 3, 1));
assertEquals(1.1, Math.min(1.1, 2.2, 3.3));
assertEquals(1.1, Math.min(3.3, 2.2, 1.1));
assertEquals(1.1, Math.min(2.2, 3.3, 1.1));

// Prepare a non-Smi zero value.
function returnsNonSmi(){ return 0.25; }
var ZERO = (function() {
  var z;
  // We have to have a loop here because the first time we get a Smi from the
  // runtime system.  After a while the binary op IC settles down and we get
  // a non-Smi from the generated code.
  for (var i = 0; i < 10; i++) {
    z = returnsNonSmi() - returnsNonSmi();
  }
  return z;
})();
assertEquals(0, ZERO);
assertEquals(Infinity, 1/ZERO);
assertEquals(-Infinity, 1/-ZERO);
assertFalse(%_IsSmi(ZERO));
assertFalse(%_IsSmi(-ZERO));

var o = {};
o.valueOf = function() { return 1; };
assertEquals(1, Math.min(2, 3, '1'));
assertEquals(1, Math.min(3, o, 2));
assertEquals(1, Math.min(o, 2));
assertEquals(-Infinity, Infinity / Math.min(-0, +0));
assertEquals(-Infinity, Infinity / Math.min(+0, -0));
assertEquals(-Infinity, Infinity / Math.min(+0, -0, 1));
assertEquals(-Infinity, Infinity / Math.min(-0, ZERO));
assertEquals(-Infinity, Infinity / Math.min(ZERO, -0));
assertEquals(-Infinity, Infinity / Math.min(ZERO, -0, 1));
assertEquals(-1, Math.min(+0, -0, -1));
assertEquals(-1, Math.min(-1, +0, -0));
assertEquals(-1, Math.min(+0, -1, -0));
assertEquals(-1, Math.min(-0, -1, +0));
assertNaN(Math.min('oxen'));
assertNaN(Math.min('oxen', 1));
assertNaN(Math.min(1, 'oxen'));


// Test Math.max().

assertEquals(Number.NEGATIVE_INFINITY, Math.max());
assertEquals(1, Math.max(1));
assertEquals(2, Math.max(1, 2));
assertEquals(2, Math.max(2, 1));
assertEquals(3, Math.max(1, 2, 3));
assertEquals(3, Math.max(3, 2, 1));
assertEquals(3, Math.max(2, 3, 1));
assertEquals(3.3, Math.max(1.1, 2.2, 3.3));
assertEquals(3.3, Math.max(3.3, 2.2, 1.1));
assertEquals(3.3, Math.max(2.2, 3.3, 1.1));

var o = {};
o.valueOf = function() { return 3; };
assertEquals(3, Math.max(2, '3', 1));
assertEquals(3, Math.max(1, o, 2));
assertEquals(3, Math.max(o, 1));
assertEquals(Infinity, Infinity / Math.max(-0, +0));
assertEquals(Infinity, Infinity / Math.max(+0, -0));
assertEquals(Infinity, Infinity / Math.max(+0, -0, -1));
assertEquals(Infinity, Infinity / Math.max(-0, ZERO));
assertEquals(Infinity, Infinity / Math.max(ZERO, -0));
assertEquals(Infinity, Infinity / Math.max(ZERO, -0, -1));
assertEquals(1, Math.max(+0, -0, +1));
assertEquals(1, Math.max(+1, +0, -0));
assertEquals(1, Math.max(+0, +1, -0));
assertEquals(1, Math.max(-0, +1, +0));
assertNaN(Math.max('oxen'));
assertNaN(Math.max('oxen', 1));
assertNaN(Math.max(1, 'oxen'));

assertEquals(Infinity, 1/Math.max(ZERO, -0));
assertEquals(Infinity, 1/Math.max(-0, ZERO));
