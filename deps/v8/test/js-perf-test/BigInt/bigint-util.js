// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

"use strict";

// Test configuration.
const TEST_ITERATIONS = 1000;
const SLOW_TEST_ITERATIONS = 50;
const SMALL_BITS_CASES = [32, 64, 128, 256];
const MEDIUM_BITS_CASES = [512, 1024];
const BIG_BITS_CASES = [2048, 4096, 8192];
const BITS_CASES = [32, 64, 128, 256, 512, 1024, 2048, 4096, 8192];
const RANDOM_BIGINTS_MAX_BITS = 64 * 100;
const BIGINT_MAX_BITS = %BigIntMaxLengthBits();


function RandomHexDigit(allow_zero) {
  const chars = allow_zero ? '0123456789ABCDEF' : '123456789ABCDEF';
  return chars.charAt(Math.floor(Math.random() * chars.length));
}


// Some benchmarks shall compute sums but the result must not grow in terms
// of digits. These can use "small" BigInts, which are BigInts where the most
// significant bits of a digit are 0, so it does not overflow.
function SmallRandomBigIntWithBits(bits) {
  console.assert(bits % 4 === 0);
  if (bits <= 0) {
    return 0n;
  }

  // Make sure it does not start with four 0-bits.
  let s = "0x" + RandomHexDigit(false);
  bits -= 4;
  // Digits are at least 32 bits long, so we round down to the next smaller
  // multiple of 32 to keep the most significant digit small.
  bits = Math.floor(bits / 32) * 32;
  for (; bits > 0; bits -= 4) {
    s += RandomHexDigit(true);
  }
  return BigInt(s);
}


function MaxBigIntWithBits(bits) {
  console.assert(bits % 4 === 0);
  if (bits <= 0) {
    return 0n;
  }

  let s = "0x";
  s += "F".repeat(bits / 4);
  return BigInt(s);
}


// Generates a random BigInt between 2^(bits-4) and 2^bits-1 (for bits > 0).
function RandomBigIntWithBits(bits) {
  console.assert(bits % 4 === 0);
  if (bits <= 0) {
    return 0n;
  }

  // Make sure it does not start with four 0-bits.
  let s = "0x" + RandomHexDigit(false);
  bits -= 4;
  // Randomly generate remaining bits.
  for (; bits > 0; bits -= 4) {
    s += RandomHexDigit(true);
  }
  return BigInt(s);
}
