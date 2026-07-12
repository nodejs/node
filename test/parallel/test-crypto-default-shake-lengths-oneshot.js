'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const {
  getHashes,
  hash,
} = require('crypto');

if (!getHashes().includes('shake128'))
  common.skip('unsupported shake128 test');

const assert = require('assert');

const invalidXofLength = {
  code: 'ERR_OSSL_EVP_NOT_XOF_OR_INVALID_LENGTH',
  name: 'Error',
  message: /not XOF or invalid length/,
};

const shakeAlgorithms = [
  'shake128',
  'SHAKE128',
  'shake256',
  'SHAKE256',
];

for (const algorithm of shakeAlgorithms) {
  assert.throws(() => hash(algorithm, 'test'), invalidXofLength);
  assert.throws(() => hash(algorithm, 'test', 'hex'), invalidXofLength);
  assert.throws(
    () => hash(algorithm, 'test', { outputEncoding: 'buffer' }),
    invalidXofLength);
  assert.throws(() => hash(algorithm, 'test', {}), invalidXofLength);
}
