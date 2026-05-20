'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const crypto = require('crypto');

common.expectWarning({
  DeprecationWarning: {
    DEP0203: 'Passing a CryptoKey to node:crypto functions is deprecated.',
  },
});

(async () => {
  const key = await globalThis.crypto.subtle.generateKey(
    { name: 'AES-CBC', length: 128 },
    true,
    ['encrypt'],
  );

  crypto.createCipheriv('aes-128-cbc', key, Buffer.alloc(16));
})().then(common.mustCall());
