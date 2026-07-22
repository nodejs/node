'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const crypto = require('crypto');

(async () => {
  const secretKey = await globalThis.crypto.subtle.generateKey(
    { name: 'AES-CBC', length: 128 },
    true,
    ['encrypt'],
  );

  assert.throws(() => {
    crypto.createCipheriv('aes-128-cbc', secretKey, Buffer.alloc(16));
  }, {
    code: 'ERR_INVALID_ARG_TYPE',
  });

  const { publicKey } = await globalThis.crypto.subtle.generateKey(
    { name: 'ECDSA', namedCurve: 'P-256' },
    true,
    ['sign', 'verify'],
  );

  assert.throws(() => {
    crypto.createPublicKey(publicKey);
  }, {
    code: 'ERR_INVALID_ARG_TYPE',
  });

  assert.throws(() => {
    crypto.publicEncrypt({ key: publicKey }, Buffer.alloc(0));
  }, {
    code: 'ERR_INVALID_ARG_TYPE',
  });

  const ecdh = await globalThis.crypto.subtle.generateKey(
    { name: 'ECDH', namedCurve: 'P-256' },
    true,
    ['deriveBits'],
  );

  assert.throws(() => {
    crypto.diffieHellman({
      privateKey: ecdh.privateKey,
      publicKey: ecdh.publicKey,
    });
  }, {
    code: 'ERR_INVALID_ARG_TYPE',
  });
})().then(common.mustCall());
