'use strict';

// Flags: --expose-internals

const common = require('../common');
if (!common.hasCrypto) common.skip('missing crypto');

const assert = require('assert');
const crypto = require('crypto');
const { kHandle } = require('internal/crypto/util');

/**
 * This test verifies that the Hash stream properly surfaces an error
 * when the underlying native update fails.
 */

const h = crypto.createHash('sha256');

// Simulate native update failure by replacing the internal handle
const fakeHandle = {
  update: () => false,
  digest: () => Buffer.from('')
};

h[kHandle] = fakeHandle;

h.on('error', common.mustCall((err) => {
  assert.strictEqual(err.code, 'ERR_CRYPTO_HASH_UPDATE_FAILED');
}));

h.write('test data');
h.end();
