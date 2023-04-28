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

function toInt32(x) {
  return x | 0;
}

assertEquals(0, toInt32(Infinity), "Inf");
assertEquals(0, toInt32(-Infinity), "-Inf");
assertEquals(0, toInt32(NaN), "NaN");
assertEquals(0, toInt32(0.0), "zero");
assertEquals(0, toInt32(-0.0), "-zero");

assertEquals(0, toInt32(Number.MIN_VALUE));
assertEquals(0, toInt32(-Number.MIN_VALUE));
assertEquals(0, toInt32(0.1));
assertEquals(0, toInt32(-0.1));
assertEquals(1, toInt32(1), "one");
assertEquals(1, toInt32(1.1), "onepointone");
assertEquals(-1, toInt32(-1), "-one");
assertEquals(0, toInt32(0.6), "truncate positive (0.6)");
assertEquals(1, toInt32(1.6), "truncate positive (1.6)");
assertEquals(0, toInt32(-0.6), "truncate negative (-0.6)");
assertEquals(-1, toInt32(-1.6), "truncate negative (-1.6)");

assertEquals(2147483647, toInt32(2147483647));
assertEquals(-2147483648, toInt32(2147483648));
assertEquals(-2147483647, toInt32(2147483649));

assertEquals(-1, toInt32(4294967295));
assertEquals(0, toInt32(4294967296));
assertEquals(1, toInt32(4294967297));

assertEquals(-2147483647, toInt32(-2147483647));
assertEquals(-2147483648, toInt32(-2147483648));
assertEquals(2147483647, toInt32(-2147483649));

assertEquals(1, toInt32(-4294967295));
assertEquals(0, toInt32(-4294967296));
assertEquals(-1, toInt32(-4294967297));

assertEquals(-2147483648, toInt32(2147483648.25));
assertEquals(-2147483648, toInt32(2147483648.5));
assertEquals(-2147483648, toInt32(2147483648.75));
assertEquals(-1, toInt32(4294967295.25));
assertEquals(-1, toInt32(4294967295.5));
assertEquals(-1, toInt32(4294967295.75));
assertEquals(-1294967296, toInt32(3000000000.25));
assertEquals(-1294967296, toInt32(3000000000.5));
assertEquals(-1294967296, toInt32(3000000000.75));

assertEquals(-2147483648, toInt32(-2147483648.25));
assertEquals(-2147483648, toInt32(-2147483648.5));
assertEquals(-2147483648, toInt32(-2147483648.75));
assertEquals(1, toInt32(-4294967295.25));
assertEquals(1, toInt32(-4294967295.5));
assertEquals(1, toInt32(-4294967295.75));
assertEquals(1294967296, toInt32(-3000000000.25));
assertEquals(1294967296, toInt32(-3000000000.5));
assertEquals(1294967296, toInt32(-3000000000.75));

var base = Math.pow(2, 64);
assertEquals(0, toInt32(base + 0));
assertEquals(0, toInt32(base + 1117));
assertEquals(4096, toInt32(base + 2234));
assertEquals(4096, toInt32(base + 3351));
assertEquals(4096, toInt32(base + 4468));
assertEquals(4096, toInt32(base + 5585));
assertEquals(8192, toInt32(base + 6702));
assertEquals(8192, toInt32(base + 7819));
assertEquals(8192, toInt32(base + 8936));
assertEquals(8192, toInt32(base + 10053));
assertEquals(12288, toInt32(base + 11170));
assertEquals(12288, toInt32(base + 12287));
assertEquals(12288, toInt32(base + 13404));
assertEquals(16384, toInt32(base + 14521));
assertEquals(16384, toInt32(base + 15638));
assertEquals(16384, toInt32(base + 16755));
assertEquals(16384, toInt32(base + 17872));
assertEquals(20480, toInt32(base + 18989));
assertEquals(20480, toInt32(base + 20106));
assertEquals(20480, toInt32(base + 21223));
assertEquals(20480, toInt32(base + 22340));
assertEquals(24576, toInt32(base + 23457));
assertEquals(24576, toInt32(base + 24574));
assertEquals(24576, toInt32(base + 25691));
assertEquals(28672, toInt32(base + 26808));
assertEquals(28672, toInt32(base + 27925));
assertEquals(28672, toInt32(base + 29042));
assertEquals(28672, toInt32(base + 30159));
assertEquals(32768, toInt32(base + 31276));

// bignum is (2^53 - 1) * 2^31 - highest number with bit 31 set.
var bignum = Math.pow(2, 84) - Math.pow(2, 31);
assertEquals(-Math.pow(2,31), toInt32(bignum));
assertEquals(-Math.pow(2,31), toInt32(-bignum));
assertEquals(0, toInt32(2 * bignum));
assertEquals(0, toInt32(-(2 * bignum)));
assertEquals(0, toInt32(bignum - Math.pow(2,31)));
assertEquals(0, toInt32(-(bignum - Math.pow(2,31))));

// max_fraction is largest number below 1.
var max_fraction = (1 - Math.pow(2,-53));
assertEquals(0, toInt32(max_fraction));
assertEquals(0, toInt32(-max_fraction));
