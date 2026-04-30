'use strict';

const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const { hasOpenSSL } = require('../common/crypto');
const { subtle } = globalThis.crypto;

async function test(algorithmName, keyLength, ivLength, format = 'raw') {
  const key = await subtle.importKey(
    format,
    new Uint8Array(keyLength),
    { name: algorithmName },
    false,
    ['encrypt', 'decrypt'],
  );

  const data = new Uint8Array(32);
  data.buffer.transfer();

  await assert.rejects(
    subtle.decrypt({ name: algorithmName, iv: new Uint8Array(ivLength) }, key, data),
    { name: 'OperationError' },
  );
}

const tests = [
  test('AES-GCM', 32, 12),
];

if (hasOpenSSL(3)) {
  tests.push(test('AES-OCB', 32, 12, 'raw-secret'));
}

if (!process.features.openssl_is_boringssl) {
  tests.push(test('ChaCha20-Poly1305', 32, 12, 'raw-secret'));
}

Promise.all(tests).then(common.mustCall());
