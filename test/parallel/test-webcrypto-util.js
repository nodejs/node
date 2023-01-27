// Flags: --expose-internals
'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');

const {
  normalizeAlgorithm,
} = require('internal/crypto/util');

{
  // Check that normalizeAlgorithm does not mutate object inputs.
  const algorithm = { name: 'ECDSA', hash: 'SHA-256' };
  assert.strictEqual(normalizeAlgorithm(algorithm, 'sign') !== algorithm, true);
  assert.deepStrictEqual(algorithm, { name: 'ECDSA', hash: 'SHA-256' });
}
