'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const { subtle } = globalThis.crypto;

(async () => {
  const algorithm = { name: 'HMAC', hash: 'SHA-256' };
  const jwk = { kty: 'oct', k: 'AAAAAAAAAAAAAAAAAAAAAA' };
  await subtle.importKey('jwk', { ...jwk, key_ops: ['sign', 'future'] },
                         algorithm, false, ['sign']);
  for (const key_ops of [['sign', 'sign'], ['sign', 'future', 'future']]) {
    await assert.rejects(subtle.importKey('jwk', { ...jwk, key_ops },
                                          algorithm, false, ['sign']),
                         { name: 'DataError' });
  }
})().then(common.mustCall());
