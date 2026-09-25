'use strict';

const common = require('../common');
const { isBoringSSL } = require('../common/crypto');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const crypto = require('crypto');

if (isBoringSSL) {
  common.skip('Skipping unsupported Diffie-Hellman tests');
}

// Unlike DiffieHellman, DiffieHellmanGroup does not have any setters.
const dhg = crypto.getDiffieHellman('modp1');
assert.strictEqual(dhg.constructor, crypto.DiffieHellmanGroup);
assert.strictEqual(dhg.setPrivateKey, undefined);
assert.strictEqual(dhg.setPublicKey, undefined);
