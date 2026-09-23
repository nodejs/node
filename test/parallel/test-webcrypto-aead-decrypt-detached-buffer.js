'use strict';

const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const { hasOpenSSL, hasFIPS } = require('../common/crypto');
const { subtle } = globalThis.crypto;
const fips3 = hasFIPS(3);

async function test(
  algorithmName,
  keyLength,
  ivLength,
  format = 'raw',
) {
  if (fips3 && algorithmName === 'AES-OCB') {
    await assert.rejects(
      subtle.importKey(format, new Uint8Array(keyLength), algorithmName, false, ['decrypt']),
      { name: 'NotSupportedError', message: 'Unrecognized algorithm name' });
    return;
  }
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

if (fips3) {
  tests.push(assert.rejects(
    subtle.importKey(
      'raw-secret',
      new Uint8Array(32),
      'ChaCha20-Poly1305',
      false,
      ['encrypt', 'decrypt']),
    { name: 'NotSupportedError' }));
} else {
  tests.push(test('ChaCha20-Poly1305', 32, 12, 'raw-secret'));
}

if (hasOpenSSL(3)) {
  tests.push(test(
    'AES-OCB',
    32,
    12,
    'raw-secret'));
}

Promise.all(tests).then(common.mustCall());
