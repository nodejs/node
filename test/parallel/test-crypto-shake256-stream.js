'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const { createHash } = require('crypto');
const { isBoringSSL } = require('../common/crypto');

const expected =
  '483366601360a8771c6863080cc4114d8db44530f8f1e1ee4f94ea37e78b5739' +
  'd5a15bef186a5386c75744c0527e1faa9f8726e462a12a4feb06bd8801e751e4';

for (const outputLength of [32, 64]) {
  const stream = createHash('shake256', { outputLength });
  stream.end('abc');
  const digest = expected.slice(0, outputLength * 2);
  assert.strictEqual(stream.read().toString('hex'), digest);

  // A stream flush caches the digest without setting Hash's JavaScript
  // finalized state. Reusing BoringSSL's cleared context used to hang or crash.
  if (isBoringSSL) {
    assert.throws(() => stream.update('def'), {
      code: 'ERR_CRYPTO_HASH_UPDATE_FAILED',
    });
    assert.throws(() => stream.copy({ outputLength }), {
      name: 'Error',
      message: 'Digest method not supported',
    });
  }

  assert.strictEqual(stream.digest('hex'), digest);
}
