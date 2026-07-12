'use strict';

const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const { SubtleCrypto } = globalThis;
const { subtle } = globalThis.crypto;

const algorithm = {
  name: 'HKDF',
  hash: 'SHA-256',
  info: new Uint8Array(),
  salt: new Uint8Array(32),
};
const invalidLength = 2 ** 32;
const expectedError = {
  code: 'ERR_OUT_OF_RANGE',
  name: 'TypeError',
};

assert.throws(
  () => SubtleCrypto.supports('deriveBits', algorithm, invalidLength),
  expectedError);

(async () => {
  const key = await subtle.importKey(
    'raw', new Uint8Array(32), 'HKDF', false, ['deriveBits']);

  await assert.rejects(
    subtle.deriveBits(algorithm, key, invalidLength),
    expectedError);
})().then(common.mustCall());
