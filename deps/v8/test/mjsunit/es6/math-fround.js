// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// Monkey-patch Float32Array.
Float32Array = function(x) { this[0] = 0; };

assertTrue(isNaN(Math.fround(NaN)));
assertTrue(isNaN(Math.fround(function() {})));
assertTrue(isNaN(Math.fround({ toString: function() { return NaN; } })));
assertTrue(isNaN(Math.fround({ valueOf: function() { return "abc"; } })));
assertTrue(isNaN(Math.fround(NaN)));
assertTrue(isNaN(Math.fround(function() {})));
assertTrue(isNaN(Math.fround({ toString: function() { return NaN; } })));
assertTrue(isNaN(Math.fround({ valueOf: function() { return "abc"; } })));

function unopt(x) { return Math.fround(x); }
function opt(y) { return Math.fround(y); }

opt(0.1);
opt(0.1);
unopt(0.1);
%NeverOptimizeFunction(unopt);
%OptimizeFunctionOnNextCall(opt);

function test(f) {
  assertEquals("Infinity", String(1/f(0)));
  assertEquals("-Infinity", String(1/f(-0)));
  assertEquals("Infinity", String(f(Infinity)));
  assertEquals("-Infinity", String(f(-Infinity)));
  assertEquals("Infinity", String(f(1E200)));
  assertEquals("-Infinity", String(f(-1E200)));
  assertEquals("Infinity", String(1/f(1E-300)));
  assertEquals("-Infinity", String(1/f(-1E-300)));
}

test(opt);
test(unopt);

mantissa_23_shift = Math.pow(2, -23);
mantissa_29_shift = Math.pow(2, -23-29);

// Javascript implementation of IEEE 754 to test double to single conversion.
function ieee754float(sign_bit,
                      exponent_bits,
                      mantissa_23_bits,
                      mantissa_29_bits) {
  this.sign_bit = sign_bit & 1;
  this.exponent_bits = exponent_bits & ((1 << 11) - 1);
  this.mantissa_23_bits = mantissa_23_bits & ((1 << 23) - 1);
  this.mantissa_29_bits = mantissa_29_bits & ((1 << 29) - 1);
}

ieee754float.prototype.returnSpecial = function() {
  if (mantissa_23_bits == 0 && mantissa_29_bits == 0) return sign * Infinity;
  return NaN;
}

ieee754float.prototype.toDouble = function() {
  var sign = this.sign_bit ? -1 : 1;
  var exponent = this.exponent_bits - 1023;
  if (exponent == -1023) returnSpecial();
  var mantissa = 1 + this.mantissa_23_bits * mantissa_23_shift +
                     this.mantissa_29_bits * mantissa_29_shift;
  return sign * Math.pow(2, exponent) * mantissa;
}

ieee754float.prototype.toSingle = function() {
  var sign = this.sign_bit ? -1 : 1;
  var exponent = this.exponent_bits - 1023;
  if (exponent == -1023) returnSpecial();
  if (exponent > 127) return sign * Infinity;
  if (exponent < -126) return this.toSingleSubnormal(sign, exponent);
  var round = this.mantissa_29_bits >> 28;
  var mantissa = 1 + (this.mantissa_23_bits + round) * mantissa_23_shift;
  return sign * Math.pow(2, exponent) * mantissa;
}

ieee754float.prototype.toSingleSubnormal = function(sign, exponent) {
  var shift = -126 - exponent;
  if (shift > 24) return sign * 0;
  var round_mask = 1 << (shift - 1);
  var mantissa_23_bits = this.mantissa_23_bits + (1 << 23);
  var round = ((mantissa_23_bits & round_mask) != 0) | 0;
  if (round) {  // Round to even if tied.
    var tied_mask = round_mask - 1;
    var result_last_bit_mask = 1 << shift;
    var tied = this.mantissa_29_bits == 0 &&
               (mantissa_23_bits & tied_mask ) == 0;
    var result_already_even = (mantissa_23_bits & result_last_bit_mask) == 0;
    if (tied && result_already_even) round = 0;
  }
  mantissa_23_bits >>= shift;
  var mantissa = (mantissa_23_bits + round) * mantissa_23_shift;
  return sign * Math.pow(2, -126) * mantissa;
}


var pi = new ieee754float(0, 0x400, 0x490fda, 0x14442d18);
assertEquals(pi.toSingle(), opt(pi.toDouble()));
assertEquals(pi.toSingle(), unopt(pi.toDouble()));


function fuzz_mantissa(sign, exp, m1inc, m2inc) {
  for (var m1 = 0; m1 < (1 << 23); m1 += m1inc) {
    for (var m2 = 0; m2 < (1 << 29); m2 += m2inc) {
      var float = new ieee754float(sign, exp, m1, m2);
      assertEquals(float.toSingle(), unopt(float.toDouble()));
      assertEquals(float.toSingle(), opt(float.toDouble()));
    }
  }
}

for (var sign = 0; sign < 2; sign++) {
  for (var exp = 1024 - 170; exp < 1024 + 170; exp++) {
    fuzz_mantissa(sign, exp, 1337 * exp - sign, 127913 * exp - sign);
  }
}
