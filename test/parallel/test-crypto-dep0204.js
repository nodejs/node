'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const { KeyObject } = require('crypto');

common.expectWarning({
  DeprecationWarning: {
    DEP0204: 'Passing a non-extractable CryptoKey to KeyObject.from() is deprecated.',
  },
});

(async () => {
  const key = await globalThis.crypto.subtle.generateKey(
    { name: 'AES-CBC', length: 128 },
    false,  // non-extractable
    ['encrypt'],
  );

  KeyObject.from(key);
})().then(common.mustCall());
