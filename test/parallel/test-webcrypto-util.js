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
  // Check that normalizeAlgorithm does not add an undefined hash property.
  assert.strictEqual('hash' in normalizeAlgorithm({ name: 'ECDH' }), false);
  assert.strictEqual('hash' in normalizeAlgorithm('ECDH'), false);
}

{
  // Check that normalizeAlgorithm does not mutate object inputs.
  const algorithm = { name: 'ECDH', hash: 'SHA-256' };
  assert.strictEqual(normalizeAlgorithm(algorithm) !== algorithm, true);
  assert.deepStrictEqual(algorithm, { name: 'ECDH', hash: 'SHA-256' });
}
