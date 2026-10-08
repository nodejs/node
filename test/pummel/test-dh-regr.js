// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

'use strict';
const common = require('../common');

if (!common.hasCrypto) {
  common.skip('missing crypto');
}

if (common.isPi()) {
  common.skip('Too slow for Raspberry Pi devices');
}

const assert = require('assert');
const crypto = require('crypto');
const { hasOpenSSL, hasFIPS } = require('../common/crypto');

let iterations = 2000;
if (hasFIPS(3)) {
  assert.throws(() => crypto.createDiffieHellman(1024), {
    code: 'ERR_INVALID_ARG_VALUE',
    name: 'TypeError',
  });

  // Keep a lower iteration count for FIPS jobs.
  iterations = 100;
}

let createDH;
if (hasOpenSSL(3)) {
  // OpenSSL 3 recognizes named groups without validating their primes.
  createDH = () => crypto.getDiffieHellman('modp14');
} else {
  // Other backends validate each peer's parameters, so keep them small.
  const length = crypto.getFips() === 1 ? 1024 : 256;
  const prime = crypto.createDiffieHellman(length).getPrime();
  createDH = () => crypto.createDiffieHellman(prime);
}

for (let i = 0; i < iterations; i++) {
  const a = createDH();
  const b = createDH();

  a.generateKeys();
  b.generateKeys();

  const aSecret = a.computeSecret(b.getPublicKey());
  const bSecret = b.computeSecret(a.getPublicKey());

  assert.deepStrictEqual(
    aSecret,
    bSecret,
    'Secrets should be equal.\n' +
    `aSecret: ${aSecret.toString('base64')}\n` +
    `bSecret: ${bSecret.toString('base64')}`,
  );
}
