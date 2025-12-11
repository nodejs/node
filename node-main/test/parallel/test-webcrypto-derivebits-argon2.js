'use strict';

const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const { hasOpenSSL } = require('../common/crypto');

if (!hasOpenSSL(3, 2))
  common.skip('requires OpenSSL >= 3.2');

const assert = require('assert');
const { createSecretKey } = require('crypto');
const { subtle } = globalThis.crypto;

const vectors = [
  // https://www.rfc-editor.org/rfc/rfc9106#name-argon2d-test-vectors
  {
    algorithm: 'Argon2d',
    length: 256,
    password: Buffer.alloc(32, 0x01),
    params: {
      memory: 32,
      passes: 3,
      parallelism: 4,
      nonce: Buffer.alloc(16, 0x02),
      secretValue: Buffer.alloc(8, 0x03),
      associatedData: Buffer.alloc(12, 0x04),
    },
    tag: Buffer.alloc(32,
                      '512b391b6f1162975371d30919734294f868e3be3984f3c1a13a4db9fabe4acb',
                      'hex').buffer,
  },
  // https://www.rfc-editor.org/rfc/rfc9106#name-argon2i-test-vectors
  {
    algorithm: 'Argon2i',
    length: 256,
    password: Buffer.alloc(32, 0x01),
    params: {
      memory: 32,
      passes: 3,
      parallelism: 4,
      nonce: Buffer.alloc(16, 0x02),
      secretValue: Buffer.alloc(8, 0x03),
      associatedData: Buffer.alloc(12, 0x04),
    },
    tag: Buffer.alloc(32,
                      'c814d9d1dc7f37aa13f0d77f2494bda1c8de6b016dd388d29952a4c4672b6ce8',
                      'hex').buffer,
  },
  // https://www.rfc-editor.org/rfc/rfc9106#name-argon2id-test-vectors
  {
    algorithm: 'Argon2id',
    length: 256,
    password: Buffer.alloc(32, 0x01),
    params: {
      memory: 32,
      passes: 3,
      parallelism: 4,
      nonce: Buffer.alloc(16, 0x02),
      secretValue: Buffer.alloc(8, 0x03),
      associatedData: Buffer.alloc(12, 0x04),
    },
    tag: Buffer.alloc(32,
                      '0d640df58d78766c08c037a34a8b53c9d01ef0452d75b65eb52520e96b01e659',
                      'hex').buffer,
  },
];

for (const { algorithm, length, password, params, tag } of vectors) {
  (async () => {
    const parameters = { name: algorithm, ...params };
    const usages = ['deriveBits', 'deriveKey'];
    {
      const key = await subtle.importKey('raw-secret', password, algorithm, false, usages);

      const result = await subtle.deriveBits(parameters, key, length);
      assert.deepStrictEqual(result, tag);
    }
    {
      const derivedKeyType = { name: 'HMAC', length, hash: 'SHA-256' };

      const key = createSecretKey(password)
        .toCryptoKey(algorithm, false, usages);
      const hmac = await subtle.deriveKey(parameters, key, derivedKeyType, true, ['sign', 'verify']);

      const result = await subtle.exportKey('raw', hmac);
      assert.deepStrictEqual(result, tag);
    }
  })().then(common.mustCall());
}
