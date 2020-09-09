'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

if (!common.enoughTestMem)
  common.skip('skip on low memory machines');

const assert = require('assert');

const {
  Buffer
} = require('buffer');

const {
  createHash,
  createHmac,
  createSign,
  createCipheriv,
  randomBytes,
  generateKeyPairSync,
  publicEncrypt,
  publicDecrypt,
} = require('crypto');

let kData;
try {
  kData = Buffer.alloc(2 ** 31 + 1);
} catch {
  // If the Buffer could not be allocated for some reason,
  // just skip the test. There are some systems in CI that
  // simply cannot allocated a large enough buffer.
  common.skip('skip on low memory machines');
}

// https://github.com/nodejs/node/pull/31406 changed the maximum value
// of buffer.kMaxLength but Hash, Hmac, and SignBase, CipherBase update
// expected int for the size, causing both of the following to segfault.
// Both update operations have been updated to expected size size_t.

// Closely related is the fact that publicEncrypt max input size is
// 2**31-1 due to limitations in OpenSSL.

createHash('sha512').update(kData);

createHmac('sha512', 'a secret').update(kData);

createSign('sha512').update(kData);

const { publicKey, privateKey } = generateKeyPairSync('rsa', {
  modulusLength: 1024,
  publicExponent: 3
});

assert.throws(() => publicEncrypt(publicKey, kData), {
  code: 'ERR_OUT_OF_RANGE'
});

assert.throws(() => publicDecrypt(privateKey, kData), {
  code: 'ERR_OUT_OF_RANGE'
});

{
  const cipher = createCipheriv(
    'aes-192-cbc',
    'key'.repeat(8),
    randomBytes(16));

  assert.throws(() => cipher.update(kData), {
    code: 'ERR_OUT_OF_RANGE'
  });
}
