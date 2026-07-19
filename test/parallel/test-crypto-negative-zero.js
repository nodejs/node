'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const crypto = require('crypto');
const { hasOpenSSL } = require('../common/crypto');

function getOutcome(fn) {
  try {
    return { result: fn() };
  } catch (err) {
    return { err };
  }
}

function assertSameOutcome(actual, expected) {
  if (expected.err !== undefined) {
    assert(actual.err instanceof Error);
    assert.strictEqual(actual.err.name, expected.err.name);
    assert.strictEqual(actual.err.code, expected.err.code);
    assert.strictEqual(actual.err.message, expected.err.message);
  } else {
    assert.deepStrictEqual(actual.result, expected.result);
  }
}

function assertSameErrorOrSuccess(actual, expected) {
  if (expected.err !== undefined) {
    assert(actual.err instanceof Error);
    assert.strictEqual(actual.err.name, expected.err.name);
    assert.strictEqual(actual.err.code, expected.err.code);
    assert.strictEqual(actual.err.message, expected.err.message);
  } else {
    assert.strictEqual(actual.err, undefined);
  }
}

{
  const expected = getOutcome(() =>
    crypto.hkdfSync('sha256', 'key', 'salt', 'info', 0),
  );
  assertSameOutcome(
    getOutcome(() => crypto.hkdfSync('sha256', 'key', 'salt', 'info', -0)),
    expected,
  );
  crypto.hkdf('sha256', 'key', 'salt', 'info', -0,
              common.mustCall((err, result) => {
                assertSameOutcome({ err, result }, expected);
              }));
}

{
  assert.strictEqual(
    crypto.checkPrimeSync(Buffer.from([3]), { checks: -0 }),
    true,
  );
  crypto.checkPrime(Buffer.from([3]), { checks: -0 },
                    common.mustSucceed((result) => {
                      assert.strictEqual(result, true);
                    }));
}

{
  assert.throws(() => crypto.createDiffieHellman(-0, 2), {
    name: 'Error',
  });
}

{
  for (const [type, getOptions] of [
    ['rsa', (zero) => ({ modulusLength: zero })],
    ['rsa', (zero) => ({ modulusLength: 512, publicExponent: zero })],
    ['rsa-pss', (zero) => ({
      modulusLength: 512,
      publicExponent: 65537,
      saltLength: zero,
    })],
    ['dsa', (zero) => ({ modulusLength: zero })],
    ['dh', (zero) => ({ primeLength: zero })],
    ['dh', (zero) => ({ primeLength: 2, generator: zero })],
  ]) {
    assertSameErrorOrSuccess(
      getOutcome(() => crypto.generateKeyPairSync(type, getOptions(-0))),
      getOutcome(() => crypto.generateKeyPairSync(type, getOptions(0))),
    );
  }

  if (!hasOpenSSL(3)) {
    common.printSkipMessage(
      'Skipping DSA divisorLength 0 key generation on OpenSSL 1.1.1');
  } else {
    assertSameErrorOrSuccess(
      getOutcome(() => crypto.generateKeyPairSync('dsa', {
        modulusLength: 512,
        divisorLength: -0,
      })),
      getOutcome(() => crypto.generateKeyPairSync('dsa', {
        modulusLength: 512,
        divisorLength: 0,
      })),
    );
  }

  crypto.generateKeyPair('rsa', { modulusLength: -0 },
                         common.mustCall((err) => {
                           assert(err instanceof Error);
                         }));
}

if (!process.features.openssl_is_boringssl) {
  assert.strictEqual(
    crypto.createHash('shake128', { outputLength: -0 }).digest('hex'),
    '',
  );
  assert.strictEqual(
    crypto.createHash('shake128', { outputLength: 5 })
          .copy({ outputLength: -0 })
          .digest('hex'),
    '',
  );
  assert.strictEqual(
    crypto.hash('shake128', 'data', { outputLength: -0 }),
    '',
  );
}

{
  const key = Buffer.alloc(16);
  const iv = Buffer.alloc(12);

  assertSameErrorOrSuccess(
    getOutcome(() => crypto.createCipheriv(
      'aes-128-gcm', key, iv, { authTagLength: -0 })),
    getOutcome(() => crypto.createCipheriv(
      'aes-128-gcm', key, iv, { authTagLength: 0 })),
  );
  assertSameErrorOrSuccess(
    getOutcome(() => crypto.createCipheriv(
      'aes-128-gcm', key, iv).setAAD(
      Buffer.alloc(0),
      { plaintextLength: -0 },
    )),
    getOutcome(() => crypto.createCipheriv(
      'aes-128-gcm', key, iv).setAAD(
      Buffer.alloc(0),
      { plaintextLength: 0 },
    )),
  );
  assert.strictEqual(
    crypto.getCipherInfo('aes-128-cbc', { keyLength: -0 }),
    undefined,
  );
  assert.strictEqual(
    crypto.getCipherInfo('aes-128-cbc', { ivLength: -0 }),
    undefined,
  );
}
