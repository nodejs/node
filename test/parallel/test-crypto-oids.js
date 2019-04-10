'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const {
  constants,
} = require('crypto');

assert.strictEqual(constants.EVP_PKEY_RSA, '1.2.840.113549.1.1.1');
assert.strictEqual(constants.EVP_PKEY_RSA_PSS, '1.2.840.113549.1.1.10');
assert.strictEqual(constants.EVP_PKEY_DSA, '1.2.840.10040.4.1');
assert.strictEqual(constants.EVP_PKEY_DH, '1.2.840.113549.1.3.1');
assert.strictEqual(constants.EVP_PKEY_EC, '1.2.840.10045.2.1');
assert.strictEqual(constants.EVP_PKEY_ED25519, '1.3.101.112');
assert.strictEqual(constants.EVP_PKEY_ED448, '1.3.101.113');
assert.strictEqual(constants.EVP_PKEY_X25519, '1.3.101.110');
assert.strictEqual(constants.EVP_PKEY_X448, '1.3.101.111');
