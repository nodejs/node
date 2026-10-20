'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const {
  createHash,
  getHashes,
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
  assert.throws(() => createHash(algorithm), invalidXofLength);
  assert.throws(() => createHash(algorithm, null), invalidXofLength);
  assert.throws(() => createHash(algorithm, {}), invalidXofLength);
}

const shake128 = createHash('shake128', { outputLength: 5 });
const shake128Copy = shake128.copy({ outputLength: 5 });

assert.throws(() => shake128.copy(), invalidXofLength);
assert.throws(() => shake128.copy(null), invalidXofLength);
assert.throws(() => shake128.copy({}), invalidXofLength);
assert.throws(() => shake128Copy.copy(), invalidXofLength);
