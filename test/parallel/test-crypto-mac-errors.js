'use strict';

const common = require('../common');

if (!common.hasCrypto) {
  common.skip('missing crypto');
}

const { hasOpenSSL } = require('../common/crypto');

if (!hasOpenSSL(3) || process.features.openssl_is_boringssl) {
  common.skip('OpenSSL 3 EVP_MAC support is required');
}

const assert = require('node:assert');
const fixtures = require('../common/fixtures');
const {
  createMac,
  createPublicKey,
  createSecretKey,
  getMacs,
} = require('node:crypto');

const key = Buffer.alloc(32, 0x42);
const data = Buffer.from('data');
const availableMacs = new Set(getMacs());

function invalidType(fn) {
  assert.throws(fn, { code: 'ERR_INVALID_ARG_TYPE' });
}

function invalidValue(fn) {
  assert.throws(fn, { code: 'ERR_INVALID_ARG_VALUE' });
}

for (const algorithm of [undefined, null, 1, true, [], {}]) {
  invalidType(() => createMac(algorithm, key));
}

for (const algorithm of ['', 'hmac\0sha256']) {
  invalidValue(() => createMac(algorithm, key));
}

for (const [algorithm, options] of [
  ['hmac', { digest: 1 }],
  ['cmac', { cipher: 1 }],
  ['gmac', { iv: 'not a BufferSource' }],
  ['kmac128', { customization: 'not a BufferSource' }],
  ['blake2bmac', { salt: 'not a BufferSource' }],
  ['kmac128', { outputLength: '32' }],
]) {
  invalidType(() => createMac(algorithm, key, options));
}

for (const [algorithm, options] of [
  ['hmac', { digest: 'sha256\0sha512' }],
  ['cmac', { cipher: 'aes-128-cbc\0aes-256-cbc' }],
]) {
  invalidValue(() => createMac(algorithm, key, options));
}

for (const outputLength of [-1, 0.5, 2 ** 32, Infinity, NaN]) {
  assert.throws(
    () => createMac('kmac128', key, { outputLength }),
    { code: 'ERR_OUT_OF_RANGE' },
  );
}

assert.throws(
  () => createMac('definitely-not-a-mac', key),
  { code: 'ERR_CRYPTO_INVALID_MAC' },
);

if (availableMacs.has('siphash')) {
  assert.throws(() => createMac('siphash', Buffer.alloc(15)), (error) => {
    assert.strictEqual(error.name, 'Error');
    assert.strictEqual(error.message, 'Failed to initialize MAC');
    assert.strictEqual(error.code, 'ERR_CRYPTO_OPERATION_FAILED');
    for (const property of [
      'function',
      'library',
      'reason',
      'opensslErrorStack',
    ]) {
      assert.ok(!(property in error));
    }
    return true;
  });
}

if (availableMacs.has('hmac')) {
  const algorithm = 'hmac';
  const options = { digest: 'sha256' };
  assert.throws(
    () => createMac(algorithm, key, { digest: 'definitely-not-a-digest' }),
    {
      code: 'ERR_OSSL_EVP_UNSUPPORTED',
      library: 'digital envelope routines',
      reason: 'unsupported',
    },
  );
  const publicKey = createPublicKey(fixtures.readKey('rsa_public.pem'));
  const cryptoKey = createSecretKey(key).toCryptoKey(
    { name: 'HMAC', hash: 'SHA-256' }, false, ['sign']);
  for (const invalidKey of [
    undefined,
    null,
    'key',
    {},
    publicKey,
    cryptoKey,
  ]) {
    invalidType(() => createMac(algorithm, invalidKey, options));
  }

  for (const invalidData of [
    undefined,
    null,
    1,
    true,
    {},
    new ArrayBuffer(4),
  ]) {
    invalidType(() => createMac(algorithm, key, options).update(invalidData));
  }

  invalidType(() => createMac(algorithm, key, options).update('data', 1));
  invalidType(() => createMac(algorithm, key, options).final(1));
  invalidType(() => createMac(algorithm, key, 'hex'));

  invalidValue(() => createMac(algorithm, key, options)
    .update('data', 'not-an-encoding'));
  const invalidFinalEncoding = createMac(algorithm, key, options);
  invalidValue(() => invalidFinalEncoding.final('not-an-encoding'));
  assert.deepStrictEqual(
    invalidFinalEncoding.update(data).final(),
    createMac(algorithm, key, options).update(data).final(),
  );
  invalidValue(() => createMac(algorithm, key, options).update('0', 'hex'));

  invalidValue(() => createMac('hmac', key));
  for (const extra of [
    { cipher: 'aes-128-cbc' },
    { iv: Buffer.alloc(12) },
    { customization: Buffer.alloc(0) },
    { salt: Buffer.alloc(16) },
    { outputLength: 16 },
  ]) {
    invalidValue(() => createMac('hmac', key, {
      ...options,
      ...extra,
    }));
  }
}

if (availableMacs.has('cmac')) {
  invalidValue(() => createMac('cmac', key));
  invalidValue(() => createMac('cmac', key, { digest: 'sha256' }));
  invalidValue(() => createMac('cmac', key, {
    cipher: 'aes-256-cbc',
    iv: Buffer.alloc(16),
  }));
}

if (availableMacs.has('gmac')) {
  invalidValue(() => createMac('gmac', key));
  invalidValue(() => createMac('gmac', key, { cipher: 'aes-256-gcm' }));
  invalidValue(() => createMac('gmac', key, { iv: Buffer.alloc(12) }));
  invalidValue(() => createMac('gmac', key, {
    cipher: 'aes-256-gcm',
    iv: Buffer.alloc(12),
    digest: 'sha256',
  }));
}

if (availableMacs.has('poly1305')) {
  invalidValue(() => createMac('poly1305', key, {
    customization: Buffer.alloc(0),
  }));
}
