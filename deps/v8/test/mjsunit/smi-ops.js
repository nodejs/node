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
const ONE = 1;
const ONE_HUNDRED = 100;

const OBJ_42 = new (function() {
  this.valueOf = function() { return 42; };
})();

assertEquals(42, OBJ_42.valueOf());


function Add1(x) {
  return x + 1;
}

function Add100(x) {
  return x + 100;
}

function Add1Reversed(x) {
  return 1 + x;
}

function Add100Reversed(x) {
  return 100 + x;
}


assertEquals(1, Add1(0));  // fast case
assertEquals(1, Add1Reversed(0));  // fast case
assertEquals(SMI_MAX + ONE, Add1(SMI_MAX), "smimax + 1");
assertEquals(SMI_MAX + ONE, Add1Reversed(SMI_MAX), "1 + smimax");
assertEquals(42 + ONE, Add1(OBJ_42));  // non-smi
assertEquals(42 + ONE, Add1Reversed(OBJ_42));  // non-smi

assertEquals(100, Add100(0));  // fast case
assertEquals(100, Add100Reversed(0));  // fast case
assertEquals(SMI_MAX + ONE_HUNDRED, Add100(SMI_MAX), "smimax + 100");
assertEquals(SMI_MAX + ONE_HUNDRED, Add100Reversed(SMI_MAX), " 100 + smimax");
assertEquals(42 + ONE_HUNDRED, Add100(OBJ_42));  // non-smi
assertEquals(42 + ONE_HUNDRED, Add100Reversed(OBJ_42));  // non-smi



function Sub1(x) {
  return x - 1;
}

function Sub100(x) {
  return x - 100;
}

function Sub1Reversed(x) {
  return 1 - x;
}

function Sub100Reversed(x) {
  return 100 - x;
}


assertEquals(0, Sub1(1));  // fast case
assertEquals(-1, Sub1Reversed(2));  // fast case
assertEquals(SMI_MIN - ONE, Sub1(SMI_MIN));  // overflow
assertEquals(ONE - SMI_MIN, Sub1Reversed(SMI_MIN));  // overflow
assertEquals(42 - ONE, Sub1(OBJ_42));  // non-smi
assertEquals(ONE - 42, Sub1Reversed(OBJ_42));  // non-smi

assertEquals(0, Sub100(100));  // fast case
assertEquals(1, Sub100Reversed(99));  // fast case
assertEquals(SMI_MIN - ONE_HUNDRED, Sub100(SMI_MIN));  // overflow
assertEquals(ONE_HUNDRED - SMI_MIN, Sub100Reversed(SMI_MIN));  // overflow
assertEquals(42 - ONE_HUNDRED, Sub100(OBJ_42));  // non-smi
assertEquals(ONE_HUNDRED - 42, Sub100Reversed(OBJ_42));  // non-smi


function Shr1(x) {
  return x >>> 1;
}

function Shr100(x) {
  return x >>> 100;
}

function Shr1Reversed(x) {
  return 1 >>> x;
}

function Shr100Reversed(x) {
  return 100 >>> x;
}

function Sar1(x) {
  return x >> 1;
}

function Sar100(x) {
  return x >> 100;
}

function Sar1Reversed(x) {
  return 1 >> x;
}

function Sar100Reversed(x) {
  return 100 >> x;
}


assertEquals(0, Shr1(1));
assertEquals(0, Sar1(1));
assertEquals(0, Shr1Reversed(2));
assertEquals(0, Sar1Reversed(2));
assertEquals(1610612736, Shr1(SMI_MIN));
assertEquals(-536870912, Sar1(SMI_MIN));
assertEquals(1, Shr1Reversed(SMI_MIN));
assertEquals(1, Sar1Reversed(SMI_MIN));
assertEquals(21, Shr1(OBJ_42));
assertEquals(21, Sar1(OBJ_42));
assertEquals(0, Shr1Reversed(OBJ_42));
assertEquals(0, Sar1Reversed(OBJ_42));

assertEquals(6, Shr100(100), "100 >>> 100");
assertEquals(6, Sar100(100), "100 >> 100");
assertEquals(12, Shr100Reversed(99));
assertEquals(12, Sar100Reversed(99));
assertEquals(201326592, Shr100(SMI_MIN));
assertEquals(-67108864, Sar100(SMI_MIN));
assertEquals(100, Shr100Reversed(SMI_MIN));
assertEquals(100, Sar100Reversed(SMI_MIN));
assertEquals(2, Shr100(OBJ_42));
assertEquals(2, Sar100(OBJ_42));
assertEquals(0, Shr100Reversed(OBJ_42));
assertEquals(0, Sar100Reversed(OBJ_42));


function Xor1(x) {
  return x ^ 1;
}

function Xor100(x) {
  return x ^ 100;
}

function Xor1Reversed(x) {
  return 1 ^ x;
}

function Xor100Reversed(x) {
  return 100 ^ x;
}


assertEquals(0, Xor1(1));
assertEquals(3, Xor1Reversed(2));
assertEquals(SMI_MIN + 1, Xor1(SMI_MIN));
assertEquals(SMI_MIN + 1, Xor1Reversed(SMI_MIN));
assertEquals(43, Xor1(OBJ_42));
assertEquals(43, Xor1Reversed(OBJ_42));

assertEquals(0, Xor100(100));
assertEquals(7, Xor100Reversed(99));
assertEquals(-1073741724, Xor100(SMI_MIN));
assertEquals(-1073741724, Xor100Reversed(SMI_MIN));
assertEquals(78, Xor100(OBJ_42));
assertEquals(78, Xor100Reversed(OBJ_42));

var x = 0x23; var y = 0x35;
assertEquals(0x16, x ^ y);


// Bitwise not.
var v = 0;
assertEquals(-1, ~v);
v = SMI_MIN;
assertEquals(0x3fffffff, ~v, "~smimin");
v = SMI_MAX;
assertEquals(-0x40000000, ~v, "~smimax");

// Overflowing ++ and --.
v = SMI_MAX;
v++;
assertEquals(0x40000000, v, "smimax++");
v = SMI_MIN;
v--;
assertEquals(-0x40000001, v, "smimin--");

// Not actually Smi operations.
// Check that relations on unary ops work.
var v = -1.2;
assertTrue(v == v);
assertTrue(v === v);
assertTrue(v <= v);
assertTrue(v >= v);
assertFalse(v < v);
assertFalse(v > v);
assertFalse(v != v);
assertFalse(v !== v);

// Right hand side of unary minus is overwritable.
v = 1.5
assertEquals(-2.25, -(v * v));

// Smi input to bitop gives non-smi result where the rhs is a float that
// can be overwritten.
var x1 = 0x10000000;
var x2 = 0x40000002;
var x3 = 0x40000000;
assertEquals(0x40000000, x1 << (x2 - x3), "0x10000000<<1(1)");

// Smi input to bitop gives non-smi result where the rhs could be overwritten
// if it were a float, but it isn't.
x1 = 0x10000000
x2 = 4
x3 = 2
assertEquals(0x40000000, x1 << (x2 - x3), "0x10000000<<2(2)");


// Test shift operators on non-smi inputs, giving smi and non-smi results.
function testShiftNonSmis() {
  var pos_non_smi = 2000000000;
  var neg_non_smi = -pos_non_smi;
  var pos_smi = 1000000000;
  var neg_smi = -pos_smi;

  // Begin block A
  assertEquals(pos_non_smi, (pos_non_smi) >> 0);
  assertEquals(pos_non_smi, (pos_non_smi) >>> 0);
  assertEquals(pos_non_smi, (pos_non_smi) << 0);
  assertEquals(neg_non_smi, (neg_non_smi) >> 0);
  assertEquals(neg_non_smi + 0x100000000, (neg_non_smi) >>> 0);
  assertEquals(neg_non_smi, (neg_non_smi) << 0);
  assertEquals(pos_smi, (pos_smi) >> 0, "possmi >> 0");
  assertEquals(pos_smi, (pos_smi) >>> 0, "possmi >>>0");
  assertEquals(pos_smi, (pos_smi) << 0, "possmi << 0");
  assertEquals(neg_smi, (neg_smi) >> 0, "negsmi >> 0");
  assertEquals(neg_smi + 0x100000000, (neg_smi) >>> 0, "negsmi >>> 0");
  assertEquals(neg_smi, (neg_smi) << 0), "negsmi << 0";

  assertEquals(pos_non_smi / 2, (pos_non_smi) >> 1);
  assertEquals(pos_non_smi / 2, (pos_non_smi) >>> 1);
  assertEquals(-0x1194D800, (pos_non_smi) << 1);
  assertEquals(pos_non_smi / 8, (pos_non_smi) >> 3);
  assertEquals(pos_non_smi / 8, (pos_non_smi) >>> 3);
  assertEquals(-0x46536000, (pos_non_smi) << 3);
  assertEquals(0x73594000, (pos_non_smi) << 4);
  assertEquals(pos_non_smi, (pos_non_smi + 0.5) >> 0);
  assertEquals(pos_non_smi, (pos_non_smi + 0.5) >>> 0);
  assertEquals(pos_non_smi, (pos_non_smi + 0.5) << 0);
  assertEquals(pos_non_smi / 2, (pos_non_smi + 0.5) >> 1);
  assertEquals(pos_non_smi / 2, (pos_non_smi + 0.5) >>> 1);
  assertEquals(-0x1194D800, (pos_non_smi + 0.5) << 1);
  assertEquals(pos_non_smi / 8, (pos_non_smi + 0.5) >> 3);
  assertEquals(pos_non_smi / 8, (pos_non_smi + 0.5) >>> 3);
  assertEquals(-0x46536000, (pos_non_smi + 0.5) << 3);
  assertEquals(0x73594000, (pos_non_smi + 0.5) << 4);

  assertEquals(neg_non_smi / 2, (neg_non_smi) >> 1, "negnonsmi >> 1");

  assertEquals(neg_non_smi / 2 + 0x100000000 / 2, (neg_non_smi) >>> 1,
               "negnonsmi >>> 1");
  assertEquals(0x1194D800, (neg_non_smi) << 1);
  assertEquals(neg_non_smi / 8, (neg_non_smi) >> 3);
  assertEquals(neg_non_smi / 8 + 0x100000000 / 8, (neg_non_smi) >>> 3);
  assertEquals(0x46536000, (neg_non_smi) << 3);
  assertEquals(-0x73594000, (neg_non_smi) << 4);
  assertEquals(neg_non_smi, (neg_non_smi - 0.5) >> 0);
  assertEquals(neg_non_smi + 0x100000000, (neg_non_smi - 0.5) >>> 0,
               "negnonsmi.5 >>> 0");
  assertEquals(neg_non_smi, (neg_non_smi - 0.5) << 0);
  assertEquals(neg_non_smi / 2, (neg_non_smi - 0.5) >> 1);
  assertEquals(neg_non_smi / 2 + 0x100000000 / 2, (neg_non_smi - 0.5) >>> 1,
               "negnonsmi.5 >>> 1");
  assertEquals(0x1194D800, (neg_non_smi - 0.5) << 1);
  assertEquals(neg_non_smi / 8, (neg_non_smi - 0.5) >> 3);
  assertEquals(neg_non_smi / 8 + 0x100000000 / 8, (neg_non_smi - 0.5) >>> 3);
  assertEquals(0x46536000, (neg_non_smi - 0.5) << 3);
  assertEquals(-0x73594000, (neg_non_smi - 0.5) << 4);

  assertEquals(pos_smi / 2, (pos_smi) >> 1);
  assertEquals(pos_smi / 2, (pos_smi) >>> 1);
  assertEquals(pos_non_smi, (pos_smi) << 1);
  assertEquals(pos_smi / 8, (pos_smi) >> 3);
  assertEquals(pos_smi / 8, (pos_smi) >>> 3);
  assertEquals(-0x2329b000, (pos_smi) << 3);
  assertEquals(0x73594000, (pos_smi) << 5);
  assertEquals(pos_smi, (pos_smi + 0.5) >> 0, "possmi.5 >> 0");
  assertEquals(pos_smi, (pos_smi + 0.5) >>> 0, "possmi.5 >>> 0");
  assertEquals(pos_smi, (pos_smi + 0.5) << 0, "possmi.5 << 0");
  assertEquals(pos_smi / 2, (pos_smi + 0.5) >> 1);
  assertEquals(pos_smi / 2, (pos_smi + 0.5) >>> 1);
  assertEquals(pos_non_smi, (pos_smi + 0.5) << 1);
  assertEquals(pos_smi / 8, (pos_smi + 0.5) >> 3);
  assertEquals(pos_smi / 8, (pos_smi + 0.5) >>> 3);
  assertEquals(-0x2329b000, (pos_smi + 0.5) << 3);
  assertEquals(0x73594000, (pos_smi + 0.5) << 5);

  assertEquals(neg_smi / 2, (neg_smi) >> 1);
  assertEquals(neg_smi / 2 + 0x100000000 / 2, (neg_smi) >>> 1);
  assertEquals(neg_non_smi, (neg_smi) << 1);
  assertEquals(neg_smi / 8, (neg_smi) >> 3);
  assertEquals(neg_smi / 8 + 0x100000000 / 8, (neg_smi) >>> 3);
  assertEquals(0x46536000, (neg_smi) << 4);
  assertEquals(-0x73594000, (neg_smi) << 5);
  assertEquals(neg_smi, (neg_smi - 0.5) >> 0, "negsmi.5 >> 0");
  assertEquals(neg_smi + 0x100000000, (neg_smi - 0.5) >>> 0, "negsmi.5 >>> 0");
  assertEquals(neg_smi, (neg_smi - 0.5) << 0, "negsmi.5 << 0");
  assertEquals(neg_smi / 2, (neg_smi - 0.5) >> 1);
  assertEquals(neg_smi / 2 + 0x100000000 / 2, (neg_smi - 0.5) >>> 1);
  assertEquals(neg_non_smi, (neg_smi - 0.5) << 1);
  assertEquals(neg_smi / 8, (neg_smi - 0.5) >> 3);
  assertEquals(neg_smi / 8 + 0x100000000 / 8, (neg_smi - 0.5) >>> 3);
  assertEquals(0x46536000, (neg_smi - 0.5) << 4);
  assertEquals(-0x73594000, (neg_smi - 0.5) << 5);
  // End block A

  // Repeat block A with 2^32 added to positive numbers and
  // 2^32 subtracted from negative numbers.
  // Begin block A repeat 1
  var two_32 = 0x100000000;
  var neg_32 = -two_32;
  assertEquals(pos_non_smi, (two_32 + pos_non_smi) >> 0);
  assertEquals(pos_non_smi, (two_32 + pos_non_smi) >>> 0);
  assertEquals(pos_non_smi, (two_32 + pos_non_smi) << 0);
  assertEquals(neg_non_smi, (neg_32 + neg_non_smi) >> 0);
  assertEquals(neg_non_smi + 0x100000000, (neg_32 + neg_non_smi) >>> 0);
  assertEquals(neg_non_smi, (neg_32 + neg_non_smi) << 0);
  assertEquals(pos_smi, (two_32 + pos_smi) >> 0, "2^32+possmi >> 0");
  assertEquals(pos_smi, (two_32 + pos_smi) >>> 0, "2^32+possmi >>> 0");
  assertEquals(pos_smi, (two_32 + pos_smi) << 0, "2^32+possmi << 0");
  assertEquals(neg_smi, (neg_32 + neg_smi) >> 0, "2^32+negsmi >> 0");
  assertEquals(neg_smi + 0x100000000, (neg_32 + neg_smi) >>> 0);
  assertEquals(neg_smi, (neg_32 + neg_smi) << 0, "2^32+negsmi << 0");

  assertEquals(pos_non_smi / 2, (two_32 + pos_non_smi) >> 1);
  assertEquals(pos_non_smi / 2, (two_32 + pos_non_smi) >>> 1);
  assertEquals(-0x1194D800, (two_32 + pos_non_smi) << 1);
  assertEquals(pos_non_smi / 8, (two_32 + pos_non_smi) >> 3);
  assertEquals(pos_non_smi / 8, (two_32 + pos_non_smi) >>> 3);
  assertEquals(-0x46536000, (two_32 + pos_non_smi) << 3);
  assertEquals(0x73594000, (two_32 + pos_non_smi) << 4);
  assertEquals(pos_non_smi, (two_32 + pos_non_smi + 0.5) >> 0);
  assertEquals(pos_non_smi, (two_32 + pos_non_smi + 0.5) >>> 0);
  assertEquals(pos_non_smi, (two_32 + pos_non_smi + 0.5) << 0);
  assertEquals(pos_non_smi / 2, (two_32 + pos_non_smi + 0.5) >> 1);
  assertEquals(pos_non_smi / 2, (two_32 + pos_non_smi + 0.5) >>> 1);
  assertEquals(-0x1194D800, (two_32 + pos_non_smi + 0.5) << 1);
  assertEquals(pos_non_smi / 8, (two_32 + pos_non_smi + 0.5) >> 3);
  assertEquals(pos_non_smi / 8, (two_32 + pos_non_smi + 0.5) >>> 3);
  assertEquals(-0x46536000, (two_32 + pos_non_smi + 0.5) << 3);
  assertEquals(0x73594000, (two_32 + pos_non_smi + 0.5) << 4);

  assertEquals(neg_non_smi / 2, (neg_32 + neg_non_smi) >> 1);
  assertEquals(neg_non_smi / 2 + 0x100000000 / 2, (neg_32 + neg_non_smi) >>> 1);
  assertEquals(0x1194D800, (neg_32 + neg_non_smi) << 1);
  assertEquals(neg_non_smi / 8, (neg_32 + neg_non_smi) >> 3);
  assertEquals(neg_non_smi / 8 + 0x100000000 / 8, (neg_32 + neg_non_smi) >>> 3);
  assertEquals(0x46536000, (neg_32 + neg_non_smi) << 3);
  assertEquals(-0x73594000, (neg_32 + neg_non_smi) << 4);
  assertEquals(neg_non_smi, (neg_32 + neg_non_smi - 0.5) >> 0);
  assertEquals(neg_non_smi + 0x100000000, (neg_32 + neg_non_smi - 0.5) >>> 0);
  assertEquals(neg_non_smi, (neg_32 + neg_non_smi - 0.5) << 0);
  assertEquals(neg_non_smi / 2, (neg_32 + neg_non_smi - 0.5) >> 1);
  assertEquals(neg_non_smi / 2 + 0x100000000 / 2, (neg_32 + neg_non_smi - 0.5)
               >>> 1);
  assertEquals(0x1194D800, (neg_32 + neg_non_smi - 0.5) << 1);
  assertEquals(neg_non_smi / 8, (neg_32 + neg_non_smi - 0.5) >> 3);
  assertEquals(neg_non_smi / 8 + 0x100000000 / 8, (neg_32 + neg_non_smi - 0.5)
               >>> 3);
  assertEquals(0x46536000, (neg_32 + neg_non_smi - 0.5) << 3);
  assertEquals(-0x73594000, (neg_32 + neg_non_smi - 0.5) << 4);

  assertEquals(pos_smi / 2, (two_32 + pos_smi) >> 1);
  assertEquals(pos_smi / 2, (two_32 + pos_smi) >>> 1);
  assertEquals(pos_non_smi, (two_32 + pos_smi) << 1);
  assertEquals(pos_smi / 8, (two_32 + pos_smi) >> 3);
  assertEquals(pos_smi / 8, (two_32 + pos_smi) >>> 3);
  assertEquals(-0x2329b000, (two_32 + pos_smi) << 3);
  assertEquals(0x73594000, (two_32 + pos_smi) << 5);
  assertEquals(pos_smi, (two_32 + pos_smi + 0.5) >> 0);
  assertEquals(pos_smi, (two_32 + pos_smi + 0.5) >>> 0);
  assertEquals(pos_smi, (two_32 + pos_smi + 0.5) << 0);
  assertEquals(pos_smi / 2, (two_32 + pos_smi + 0.5) >> 1);
  assertEquals(pos_smi / 2, (two_32 + pos_smi + 0.5) >>> 1);
  assertEquals(pos_non_smi, (two_32 + pos_smi + 0.5) << 1);
  assertEquals(pos_smi / 8, (two_32 + pos_smi + 0.5) >> 3);
  assertEquals(pos_smi / 8, (two_32 + pos_smi + 0.5) >>> 3);
  assertEquals(-0x2329b000, (two_32 + pos_smi + 0.5) << 3);
  assertEquals(0x73594000, (two_32 + pos_smi + 0.5) << 5);

  assertEquals(neg_smi / 2, (neg_32 + neg_smi) >> 1);
  assertEquals(neg_smi / 2 + 0x100000000 / 2, (neg_32 + neg_smi) >>> 1);
  assertEquals(neg_non_smi, (neg_32 + neg_smi) << 1);
  assertEquals(neg_smi / 8, (neg_32 + neg_smi) >> 3);
  assertEquals((neg_smi + 0x100000000) / 8, (neg_32 + neg_smi) >>> 3);
  assertEquals(0x46536000, (neg_32 + neg_smi) << 4);
  assertEquals(-0x73594000, (neg_32 + neg_smi) << 5);
  assertEquals(neg_smi, (neg_32 + neg_smi - 0.5) >> 0, "-2^32+negsmi.5 >> 0");
  assertEquals(neg_smi + 0x100000000, (neg_32 + neg_smi - 0.5) >>> 0);
  assertEquals(neg_smi, (neg_32 + neg_smi - 0.5) << 0, "-2^32+negsmi.5 << 0");
  assertEquals(neg_smi / 2, (neg_32 + neg_smi - 0.5) >> 1);
  assertEquals(neg_smi / 2 + 0x100000000 / 2, (neg_32 + neg_smi - 0.5) >>> 1);
  assertEquals(neg_non_smi, (neg_32 + neg_smi - 0.5) << 1);
  assertEquals(neg_smi / 8, (neg_32 + neg_smi - 0.5) >> 3);
  assertEquals(neg_smi / 8 + 0x100000000 / 8, (neg_32 + neg_smi - 0.5) >>> 3);
  assertEquals(0x46536000, (neg_32 + neg_smi - 0.5) << 4);
  assertEquals(-0x73594000, (neg_32 + neg_smi - 0.5) << 5);
  // End block A repeat 1
  // Repeat block A with shift amounts in variables intialized with
  // a constant.
  var zero = 0;
  var one = 1;
  var three = 3;
  var four = 4;
  var five = 5;
  // Begin block A repeat 2
  assertEquals(pos_non_smi, (pos_non_smi) >> zero);
  assertEquals(pos_non_smi, (pos_non_smi) >>> zero);
  assertEquals(pos_non_smi, (pos_non_smi) << zero);
  assertEquals(neg_non_smi, (neg_non_smi) >> zero);
  assertEquals(neg_non_smi + 0x100000000, (neg_non_smi) >>> zero);
  assertEquals(neg_non_smi, (neg_non_smi) << zero);
  assertEquals(pos_smi, (pos_smi) >> zero);
  assertEquals(pos_smi, (pos_smi) >>> zero);
  assertEquals(pos_smi, (pos_smi) << zero);
  assertEquals(neg_smi, (neg_smi) >> zero, "negsmi >> zero");
  assertEquals(neg_smi + 0x100000000, (neg_smi) >>> zero);
  assertEquals(neg_smi, (neg_smi) << zero, "negsmi << zero");

  assertEquals(pos_non_smi / 2, (pos_non_smi) >> one);
  assertEquals(pos_non_smi / 2, (pos_non_smi) >>> one);
  assertEquals(-0x1194D800, (pos_non_smi) << one);
  assertEquals(pos_non_smi / 8, (pos_non_smi) >> three);
  assertEquals(pos_non_smi / 8, (pos_non_smi) >>> three);
  assertEquals(-0x46536000, (pos_non_smi) << three);
  assertEquals(0x73594000, (pos_non_smi) << four);
  assertEquals(pos_non_smi, (pos_non_smi + 0.5) >> zero);
  assertEquals(pos_non_smi, (pos_non_smi + 0.5) >>> zero);
  assertEquals(pos_non_smi, (pos_non_smi + 0.5) << zero);
  assertEquals(pos_non_smi / 2, (pos_non_smi + 0.5) >> one);
  assertEquals(pos_non_smi / 2, (pos_non_smi + 0.5) >>> one);
  assertEquals(-0x1194D800, (pos_non_smi + 0.5) << one);
  assertEquals(pos_non_smi / 8, (pos_non_smi + 0.5) >> three);
  assertEquals(pos_non_smi / 8, (pos_non_smi + 0.5) >>> three);
  assertEquals(-0x46536000, (pos_non_smi + 0.5) << three);
  assertEquals(0x73594000, (pos_non_smi + 0.5) << four);

  assertEquals(neg_non_smi / 2, (neg_non_smi) >> one);
  assertEquals(neg_non_smi / 2 + 0x100000000 / 2, (neg_non_smi) >>> one);
  assertEquals(0x1194D800, (neg_non_smi) << one);
  assertEquals(neg_non_smi / 8, (neg_non_smi) >> three);
  assertEquals(neg_non_smi / 8 + 0x100000000 / 8, (neg_non_smi) >>> three);
  assertEquals(0x46536000, (neg_non_smi) << three);
  assertEquals(-0x73594000, (neg_non_smi) << four);
  assertEquals(neg_non_smi, (neg_non_smi - 0.5) >> zero);
  assertEquals(neg_non_smi + 0x100000000, (neg_non_smi - 0.5) >>> zero);
  assertEquals(neg_non_smi, (neg_non_smi - 0.5) << zero);
  assertEquals(neg_non_smi / 2, (neg_non_smi - 0.5) >> one);
  assertEquals(neg_non_smi / 2 + 0x100000000 / 2, (neg_non_smi - 0.5) >>> one);
  assertEquals(0x1194D800, (neg_non_smi - 0.5) << one);
  assertEquals(neg_non_smi / 8, (neg_non_smi - 0.5) >> three);
  assertEquals(neg_non_smi / 8 + 0x100000000 / 8, (neg_non_smi - 0.5)
      >>> three);
  assertEquals(0x46536000, (neg_non_smi - 0.5) << three);
  assertEquals(-0x73594000, (neg_non_smi - 0.5) << four);

  assertEquals(pos_smi / 2, (pos_smi) >> one);
  assertEquals(pos_smi / 2, (pos_smi) >>> one);
  assertEquals(pos_non_smi, (pos_smi) << one);
  assertEquals(pos_smi / 8, (pos_smi) >> three);
  assertEquals(pos_smi / 8, (pos_smi) >>> three);
  assertEquals(-0x2329b000, (pos_smi) << three);
  assertEquals(0x73594000, (pos_smi) << five);
  assertEquals(pos_smi, (pos_smi + 0.5) >> zero);
  assertEquals(pos_smi, (pos_smi + 0.5) >>> zero);
  assertEquals(pos_smi, (pos_smi + 0.5) << zero);
  assertEquals(pos_smi / 2, (pos_smi + 0.5) >> one);
  assertEquals(pos_smi / 2, (pos_smi + 0.5) >>> one);
  assertEquals(pos_non_smi, (pos_smi + 0.5) << one);
  assertEquals(pos_smi / 8, (pos_smi + 0.5) >> three);
  assertEquals(pos_smi / 8, (pos_smi + 0.5) >>> three);
  assertEquals(-0x2329b000, (pos_smi + 0.5) << three);
  assertEquals(0x73594000, (pos_smi + 0.5) << five);

  assertEquals(neg_smi / 2, (neg_smi) >> one);
  assertEquals(neg_smi / 2 + 0x100000000 / 2, (neg_smi) >>> one);
  assertEquals(neg_non_smi, (neg_smi) << one);
  assertEquals(neg_smi / 8, (neg_smi) >> three);
  assertEquals(neg_smi / 8 + 0x100000000 / 8, (neg_smi) >>> three);
  assertEquals(0x46536000, (neg_smi) << four);
  assertEquals(-0x73594000, (neg_smi) << five);
  assertEquals(neg_smi, (neg_smi - 0.5) >> zero);
  assertEquals(neg_smi + 0x100000000, (neg_smi - 0.5) >>> zero);
  assertEquals(neg_smi, (neg_smi - 0.5) << zero);
  assertEquals(neg_smi / 2, (neg_smi - 0.5) >> one);
  assertEquals(neg_smi / 2 + 0x100000000 / 2, (neg_smi - 0.5) >>> one);
  assertEquals(neg_non_smi, (neg_smi - 0.5) << one);
  assertEquals(neg_smi / 8, (neg_smi - 0.5) >> three);
  assertEquals(neg_smi / 8 + 0x100000000 / 8, (neg_smi - 0.5) >>> three);
  assertEquals(0x46536000, (neg_smi - 0.5) << four);
  assertEquals(-0x73594000, (neg_smi - 0.5) << five);
  // End block A repeat 2

  // Repeat previous block, with computed values in the shift variables.
  five = 0;
  while (five < 5 ) ++five;
  four = five - one;
  three = four - one;
  one = four - three;
  zero = one - one;

  // Begin block A repeat 3
  assertEquals(pos_non_smi, (pos_non_smi) >> zero);
  assertEquals(pos_non_smi, (pos_non_smi) >>> zero);
  assertEquals(pos_non_smi, (pos_non_smi) << zero);
  assertEquals(neg_non_smi, (neg_non_smi) >> zero);
  assertEquals(neg_non_smi + 0x100000000, (neg_non_smi) >>> zero);
  assertEquals(neg_non_smi, (neg_non_smi) << zero);
  assertEquals(pos_smi, (pos_smi) >> zero);
  assertEquals(pos_smi, (pos_smi) >>> zero);
  assertEquals(pos_smi, (pos_smi) << zero);
  assertEquals(neg_smi, (neg_smi) >> zero, "negsmi >> zero(2)");
  assertEquals(neg_smi + 0x100000000, (neg_smi) >>> zero);
  assertEquals(neg_smi, (neg_smi) << zero, "negsmi << zero(2)");

  assertEquals(pos_non_smi / 2, (pos_non_smi) >> one);
  assertEquals(pos_non_smi / 2, (pos_non_smi) >>> one);
  assertEquals(-0x1194D800, (pos_non_smi) << one);
  assertEquals(pos_non_smi / 8, (pos_non_smi) >> three);
  assertEquals(pos_non_smi / 8, (pos_non_smi) >>> three);
  assertEquals(-0x46536000, (pos_non_smi) << three);
  assertEquals(0x73594000, (pos_non_smi) << four);
  assertEquals(pos_non_smi, (pos_non_smi + 0.5) >> zero);
  assertEquals(pos_non_smi, (pos_non_smi + 0.5) >>> zero);
  assertEquals(pos_non_smi, (pos_non_smi + 0.5) << zero);
  assertEquals(pos_non_smi / 2, (pos_non_smi + 0.5) >> one);
  assertEquals(pos_non_smi / 2, (pos_non_smi + 0.5) >>> one);
  assertEquals(-0x1194D800, (pos_non_smi + 0.5) << one);
  assertEquals(pos_non_smi / 8, (pos_non_smi + 0.5) >> three);
  assertEquals(pos_non_smi / 8, (pos_non_smi + 0.5) >>> three);
  assertEquals(-0x46536000, (pos_non_smi + 0.5) << three);
  assertEquals(0x73594000, (pos_non_smi + 0.5) << four);

  assertEquals(neg_non_smi / 2, (neg_non_smi) >> one);
  assertEquals(neg_non_smi / 2 + 0x100000000 / 2, (neg_non_smi) >>> one);
  assertEquals(0x1194D800, (neg_non_smi) << one);
  assertEquals(neg_non_smi / 8, (neg_non_smi) >> three);
  assertEquals(neg_non_smi / 8 + 0x100000000 / 8, (neg_non_smi) >>> three);
  assertEquals(0x46536000, (neg_non_smi) << three);
  assertEquals(-0x73594000, (neg_non_smi) << four);
  assertEquals(neg_non_smi, (neg_non_smi - 0.5) >> zero);
  assertEquals(neg_non_smi + 0x100000000, (neg_non_smi - 0.5) >>> zero);
  assertEquals(neg_non_smi, (neg_non_smi - 0.5) << zero);
  assertEquals(neg_non_smi / 2, (neg_non_smi - 0.5) >> one);
  assertEquals(neg_non_smi / 2 + 0x100000000 / 2, (neg_non_smi - 0.5) >>> one);
  assertEquals(0x1194D800, (neg_non_smi - 0.5) << one);
  assertEquals(neg_non_smi / 8, (neg_non_smi - 0.5) >> three);
  assertEquals(neg_non_smi / 8 + 0x100000000 / 8, (neg_non_smi - 0.5)
      >>> three);
  assertEquals(0x46536000, (neg_non_smi - 0.5) << three);
  assertEquals(-0x73594000, (neg_non_smi - 0.5) << four);

  assertEquals(pos_smi / 2, (pos_smi) >> one);
  assertEquals(pos_smi / 2, (pos_smi) >>> one);
  assertEquals(pos_non_smi, (pos_smi) << one);
  assertEquals(pos_smi / 8, (pos_smi) >> three);
  assertEquals(pos_smi / 8, (pos_smi) >>> three);
  assertEquals(-0x2329b000, (pos_smi) << three);
  assertEquals(0x73594000, (pos_smi) << five);
  assertEquals(pos_smi, (pos_smi + 0.5) >> zero);
  assertEquals(pos_smi, (pos_smi + 0.5) >>> zero);
  assertEquals(pos_smi, (pos_smi + 0.5) << zero);
  assertEquals(pos_smi / 2, (pos_smi + 0.5) >> one);
  assertEquals(pos_smi / 2, (pos_smi + 0.5) >>> one);
  assertEquals(pos_non_smi, (pos_smi + 0.5) << one);
  assertEquals(pos_smi / 8, (pos_smi + 0.5) >> three);
  assertEquals(pos_smi / 8, (pos_smi + 0.5) >>> three);
  assertEquals(-0x2329b000, (pos_smi + 0.5) << three);
  assertEquals(0x73594000, (pos_smi + 0.5) << five);

  assertEquals(neg_smi / 2, (neg_smi) >> one);
  assertEquals(neg_smi / 2 + 0x100000000 / 2, (neg_smi) >>> one);
  assertEquals(neg_non_smi, (neg_smi) << one);
  assertEquals(neg_smi / 8, (neg_smi) >> three);
  assertEquals(neg_smi / 8 + 0x100000000 / 8, (neg_smi) >>> three);
  assertEquals(0x46536000, (neg_smi) << four);
  assertEquals(-0x73594000, (neg_smi) << five);
  assertEquals(neg_smi, (neg_smi - 0.5) >> zero, "negsmi.5 >> zero");
  assertEquals(neg_smi + 0x100000000, (neg_smi - 0.5) >>> zero);
  assertEquals(neg_smi, (neg_smi - 0.5) << zero, "negsmi.5 << zero");
  assertEquals(neg_smi / 2, (neg_smi - 0.5) >> one);
  assertEquals(neg_smi / 2 + 0x100000000 / 2, (neg_smi - 0.5) >>> one);
  assertEquals(neg_non_smi, (neg_smi - 0.5) << one);
  assertEquals(neg_smi / 8, (neg_smi - 0.5) >> three);
  assertEquals(neg_smi / 8 + 0x100000000 / 8, (neg_smi - 0.5) >>> three);
  assertEquals(0x46536000, (neg_smi - 0.5) << four);
  assertEquals(-0x73594000, (neg_smi - 0.5) << five);
  // End block A repeat 3

  // Test non-integer shift value
  assertEquals(5, 20.5 >> 2.4);
  assertEquals(5, 20.5 >> 2.7);
  var shift = 2.4;
  assertEquals(5, 20.5 >> shift);
  assertEquals(5, 20.5 >> shift + 0.3);
  shift = shift + zero;
  assertEquals(5, 20.5 >> shift);
  assertEquals(5, 20.5 >> shift + 0.3);
}

testShiftNonSmis();

function intConversion() {
  function foo(x) {
    assertEquals(x, (x * 1.0000000001) | 0, "foo more " + x);
    assertEquals(x, x | 0, "foo " + x);
    if (x > 0) {
      assertEquals(x - 1, (x * 0.9999999999) | 0, "foo less " + x);
    } else {
      assertEquals(x + 1, (x * 0.9999999999) | 0, "foo less " + x);
    }
  }
  for (var i = 1; i < 0x80000000; i *= 2) {
    foo(i);
    foo(-i);
  }
  for (var i = 1; i < 1/0; i *= 2) {
    assertEquals(i | 0, (i * 1.0000000000000001) | 0, "b" + i);
    assertEquals(-i | 0, (i * -1.0000000000000001) | 0, "c" + i);
  }
  for (var i = 0.5; i > 0; i /= 2) {
    assertEquals(0, i | 0, "d" + i);
    assertEquals(0, -i | 0, "e" + i);
  }
}

intConversion();

// Verify that we handle the (optimized) corner case of shifting by
// zero even for non-smis.
function shiftByZero(n) { return n << 0; }

assertEquals(3, shiftByZero(3.1415));
