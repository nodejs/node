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

let p;
let iterations = 2000;
if (hasFIPS(3)) {
  assert.throws(() => crypto.createDiffieHellman(1024), {
    code: 'ERR_INVALID_ARG_VALUE',
    name: 'TypeError',
  });

  // Use a precomputed approved group instead of generating a 2048-bit prime
  // for every test run. Its larger keys also make each pummel iteration more
  // expensive, so use enough iterations to exercise the regression without
  // making the FIPS job excessively slow.
  p = crypto.getDiffieHellman('modp14').getPrime();
  iterations = 100;
} else {
  // FIPS requires length >= 1024, but small parameters keep this pummel test
  // from timing out in ordinary CI.
  const length = crypto.getFips() === 1 ? 1024 : (hasOpenSSL(3) ? 512 : 256);
  p = crypto.createDiffieHellman(length).getPrime();
}

for (let i = 0; i < iterations; i++) {
  const a = crypto.createDiffieHellman(p);
  const b = crypto.createDiffieHellman(p);

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
