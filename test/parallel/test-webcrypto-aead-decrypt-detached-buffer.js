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
  causeCode,
) {
  const key = await subtle.importKey(
    format,
    new Uint8Array(keyLength),
    { name: algorithmName },
    false,
    ['encrypt', 'decrypt'],
  );

  const data = new Uint8Array(32);
  data.buffer.transfer();

  const expected = causeCode === undefined ?
    { name: 'OperationError' } :
    (err) => err.name === 'OperationError' &&
             err.cause?.code === causeCode;
  await assert.rejects(
    subtle.decrypt({ name: algorithmName, iv: new Uint8Array(ivLength) }, key, data),
    expected,
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
    'raw-secret',
    fips3 ? 'ERR_OSSL_EVP_UNSUPPORTED' : undefined));
}

Promise.all(tests).then(common.mustCall());
