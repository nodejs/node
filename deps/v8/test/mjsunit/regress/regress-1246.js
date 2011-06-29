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

// This regression tests the behaviour of the parseInt function when
// the given radix is not a SMI.

// Flags: --allow-natives-syntax

var nonSmi10 = Math.log(Math.exp(10));
var nonSmi16 = Math.log(Math.exp(16));

assertTrue(!%_IsSmi(nonSmi10) && nonSmi10 == 10);
assertTrue(!%_IsSmi(nonSmi16) && nonSmi16 == 16);

// Giving these values as the radix argument triggers radix detection.
var radix_detect = [0, -0, NaN, Infinity, -Infinity, undefined, null,
                    "0", "-0", "a"];

// These values will result in an integer radix outside of the valid range.
var radix_invalid = [1, 37, -2, "-2", "37"];

// These values will trigger decimal parsing.
var radix10 = [10, 10.1, "10", "10.1", nonSmi10];

// These values will trigger hexadecimal parsing.
var radix16 = [16, 16.1, 0x10, "0X10", nonSmi16];

for (var i = 0; i < radix_detect.length; i++) {
  var radix = radix_detect[i];
  assertEquals(NaN, parseInt("", radix));
  assertEquals(23, parseInt("23", radix));
  assertEquals(0xaf, parseInt("0xaf", radix));
  assertEquals(NaN, parseInt("af", radix));
}

for (var i = 0; i < radix_invalid.length; i++) {
  var radix = radix_invalid[i];
  assertEquals(NaN, parseInt("", radix));
  assertEquals(NaN, parseInt("23", radix));
  assertEquals(NaN, parseInt("0xaf", radix));
  assertEquals(NaN, parseInt("af", radix));
}

for (var i = 0; i < radix10.length; i++) {
  var radix = radix10[i];
  assertEquals(NaN, parseInt("", radix));
  assertEquals(23, parseInt("23", radix));
  assertEquals(0, parseInt("0xaf", radix));
  assertEquals(NaN, parseInt("af", radix));
}

for (var i = 0; i < radix16.length; i++) {
  var radix = radix16[i];
  assertEquals(NaN, parseInt("", radix));
  assertEquals(0x23, parseInt("23", radix));
  assertEquals(0xaf, parseInt("0xaf", radix));
  assertEquals(0xaf, parseInt("af", radix));
}

