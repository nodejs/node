'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const { hasOpenSSL, isBoringSSL } = require('../common/crypto');
if (isBoringSSL || !hasOpenSSL(3, 5))
  common.skip('requires OpenSSL 3.5 or later');

const assert = require('assert');
const { createPrivateKey } = require('crypto');
const fixtures = require('../common/fixtures');
const { subtle } = globalThis.crypto;

(async () => {
  const algorithm = { name: 'AES-GCM', iv: new Uint8Array(12) };
  const wrappingKey = await subtle.generateKey({ name: 'AES-GCM', length: 128 },
                                               false, ['wrapKey']);
  for (const [name, usage] of [
    ['ML-KEM-512', 'decapsulateBits'],
    ['ML-KEM-768', 'decapsulateBits'],
    ['ML-KEM-1024', 'decapsulateBits'],
    ['ML-DSA-44', 'sign'],
    ['ML-DSA-65', 'sign'],
    ['ML-DSA-87', 'sign'],
  ]) {
    const file = `${name.toLowerCase().replaceAll('-', '_')}_private_priv_only.pem`;
    const key = createPrivateKey(fixtures.readKey(file)).toCryptoKey(name, true, [usage]);
    await assert.rejects(subtle.exportKey('jwk', key), {
      name: 'OperationError', constructor: DOMException,
    });
    await assert.rejects(subtle.wrapKey('jwk', key, wrappingKey, algorithm), {
      name: 'OperationError', constructor: DOMException,
    });
  }
})().then(common.mustCall());
