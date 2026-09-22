'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const { subtle } = globalThis.crypto;

(async () => {
  const data = new Uint8Array(0);
  assert.deepStrictEqual(await subtle.digest('sha-256', data),
                         await subtle.digest('SHA-256', data));
  for (const name of ['\u017fha-256', 'SHA-256\u0131']) {
    await assert.rejects(subtle.digest(name, data), { name: 'NotSupportedError' });
    assert.strictEqual(SubtleCrypto.supports('digest', name), false);
  }
  await assert.rejects(subtle.generateKey({ name: 'AE\u017f-GCM', length: 128 },
                                          false, ['encrypt']),
                       { name: 'NotSupportedError' });
  await assert.rejects(subtle.importKey('raw-secret', data, 'Argon2\u0131d',
                                        false, ['deriveBits']),
                       { name: 'NotSupportedError' });
})().then(common.mustCall());
