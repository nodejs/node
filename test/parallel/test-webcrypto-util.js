// Flags: --expose-internals
'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');

const {
  normalizeAlgorithm,
  validateKeyOps,
} = require('internal/crypto/util');

{
  // Check that normalizeAlgorithm does not mutate object inputs.
  const algorithm = { name: 'ECDSA', hash: 'SHA-256' };
  assert.strictEqual(normalizeAlgorithm(algorithm, 'sign') !== algorithm, true);
  assert.deepStrictEqual(algorithm, { name: 'ECDSA', hash: 'SHA-256' });
}

// The algorithm name getter should only be invoked once during
// normalizeAlgorithm, including for algorithms with a non-null desiredType
// where step 6 runs the specialized dictionary converter.
// Refs: https://github.com/web-platform-tests/wpt/pull/57614#pullrequestreview-3808145365
{
  let nameReadCount = 0;
  const algorithm = {
    get name() {
      nameReadCount++;
      return 'AES-GCM';
    },
    iv: new Uint8Array(12),
  };
  const normalized = normalizeAlgorithm(algorithm, 'encrypt');
  assert.strictEqual(normalized.name, 'AES-GCM');
  assert.strictEqual(nameReadCount, 1);
}

{
  let nameReadCount = 0;
  const algorithm = {
    get name() {
      nameReadCount++;
      return 'ECDSA';
    },
    hash: 'SHA-256',
  };
  const normalized = normalizeAlgorithm(algorithm, 'sign');
  assert.strictEqual(normalized.name, 'ECDSA');
  assert.strictEqual(nameReadCount, 1);
}

{
  for (const ops of [
    ['sign', 'toString', 'constructor'],
    ['sign', '__proto__', 'constructor'],
  ]) {
    validateKeyOps(ops);
  }
}
