// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Distilled tight loop from the JetStream/Octane "crypto" benchmark: the
// BigInteger.am3 multiply-accumulate that dominates RSA multiplyTo / squareTo /
// montReduce. The loop reuses loop-invariant `this.array` / `w.array` element
// pointers and lengths; non-eager loop peeling used to recompute those every
// iteration. Inputs are RSA-scale operands (arrays of 28-bit "digits").

function BigIntArr() {
  this.array = [];
}

// Verbatim from Octane crypto.js (the 28-bit `am3` path).
BigIntArr.prototype.am3 = function(i, x, w, j, c, n) {
  var this_array = this.array;
  var w_array = w.array;
  var xl = x & 0x3fff, xh = x >> 14;
  while (--n >= 0) {
    var l = this_array[i] & 0x3fff;
    var h = this_array[i++] >> 14;
    var m = xh * l + h * xl;
    l = xl * l + ((m & 0x3fff) << 14) + w_array[j] + c;
    c = (l >> 28) + (m >> 14) + xh * h;
    w_array[j++] = l & 0xfffffff;
  }
  return c;
};

const DIGITS = 64;  // ~1792-bit operands at 28 bits/digit (RSA scale).

function makeBigIntArr(seed) {
  const b = new BigIntArr();
  let s = seed >>> 0;
  for (let k = 0; k < DIGITS; k++) {
    // Deterministic LCG so the benchmark is reproducible.
    s = (Math.imul(s, 1103515245) + 12345) >>> 0;
    b.array[k] = s & 0xfffffff;  // a 28-bit digit, like crypto's BigInteger.
  }
  return b;
}

const A = makeBigIntArr(0x12345);
const B = makeBigIntArr(0x6789a);
const W = new BigIntArr();
for (let k = 0; k <= 2 * DIGITS; k++) W.array[k] = 0;

// Schoolbook multiply A*B into W, exactly as BigInteger.multiplyTo drives am3.
function Am3Multiply() {
  const a = A, b = B, w = W;
  for (let k = 0; k <= 2 * DIGITS; k++) w.array[k] = 0;
  for (let i = 0; i < DIGITS; i++) {
    w.array[i + DIGITS] = a.am3(0, b.array[i], w, i, 0, DIGITS);
  }
  return w.array[DIGITS];
}

new BenchmarkSuite('Crypto-Am3-Multiply', [1000], [
  new Benchmark('Crypto-Am3-Multiply', false, false, 0, Am3Multiply),
]);
