'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const { hasOpenSSL } = require('../common/crypto');

if (!hasOpenSSL(3, 2))
  common.skip('deterministic nonce requires OpenSSL >= 3.2');

const assert = require('assert');
const crypto = require('crypto');
const fixtures = require('../common/fixtures');

const data = Buffer.from('Hello world');

// Test deterministic ECDSA signatures (RFC 6979) with streaming API.
{
  const ecPrivKey = fixtures.readKey('ec_p256_private.pem');

  const sig1 = crypto.createSign('sha256').update(data).sign({
    key: ecPrivKey,
    dsaNonceType: 'deterministic',
  });
  const sig2 = crypto.createSign('sha256').update(data).sign({
    key: ecPrivKey,
    dsaNonceType: 'deterministic',
  });
  // RFC 6979: same key + same message + same hash => same signature.
  assert.deepStrictEqual(sig1, sig2);

  // Verify the deterministic signature.
  assert.strictEqual(
    crypto.createVerify('sha256').update(data).verify(ecPrivKey, sig1),
    true,
  );
}

// Test deterministic ECDSA signatures with one-shot crypto.sign()/verify().
{
  const ecPrivKey = fixtures.readKey('ec_p256_private.pem');

  const sig1 = crypto.sign('sha256', data, {
    key: ecPrivKey,
    dsaNonceType: 'deterministic',
  });
  const sig2 = crypto.sign('sha256', data, {
    key: ecPrivKey,
    dsaNonceType: 'deterministic',
  });
  assert.deepStrictEqual(sig1, sig2);

  assert.strictEqual(
    crypto.verify('sha256', data, ecPrivKey, sig1),
    true,
  );
}

// Test deterministic DSA signatures with streaming API.
{
  const dsaPrivKey = fixtures.readKey('dsa_private.pem');

  const sig1 = crypto.createSign('sha256').update(data).sign({
    key: dsaPrivKey,
    dsaNonceType: 'deterministic',
  });
  const sig2 = crypto.createSign('sha256').update(data).sign({
    key: dsaPrivKey,
    dsaNonceType: 'deterministic',
  });
  assert.deepStrictEqual(sig1, sig2);

  assert.strictEqual(
    crypto.createVerify('sha256').update(data).verify(dsaPrivKey, sig1),
    true,
  );
}

// Test deterministic DSA signatures with one-shot API.
{
  const dsaPrivKey = fixtures.readKey('dsa_private.pem');

  const sig1 = crypto.sign('sha256', data, {
    key: dsaPrivKey,
    dsaNonceType: 'deterministic',
  });
  const sig2 = crypto.sign('sha256', data, {
    key: dsaPrivKey,
    dsaNonceType: 'deterministic',
  });
  assert.deepStrictEqual(sig1, sig2);

  assert.strictEqual(
    crypto.verify('sha256', data, dsaPrivKey, sig1),
    true,
  );
}

// Test dsaNonceType: 'hedged' produces valid signatures (explicit default).
{
  const ecPrivKey = fixtures.readKey('ec_p256_private.pem');

  const sig = crypto.sign('sha256', data, {
    key: ecPrivKey,
    dsaNonceType: 'hedged',
  });
  assert.strictEqual(
    crypto.verify('sha256', data, ecPrivKey, sig),
    true,
  );
}

// Test dsaNonceType combined with dsaEncoding: 'ieee-p1363'.
{
  const ecPrivKey = fixtures.readKey('ec_p256_private.pem');

  const sig1 = crypto.sign('sha256', data, {
    key: ecPrivKey,
    dsaNonceType: 'deterministic',
    dsaEncoding: 'ieee-p1363',
  });
  const sig2 = crypto.sign('sha256', data, {
    key: ecPrivKey,
    dsaNonceType: 'deterministic',
    dsaEncoding: 'ieee-p1363',
  });
  assert.deepStrictEqual(sig1, sig2);
  // P-256 IEEE P1363 signature is exactly 64 bytes (32 + 32).
  assert.strictEqual(sig1.length, 64);

  assert.strictEqual(
    crypto.verify('sha256', data, {
      key: ecPrivKey,
      dsaEncoding: 'ieee-p1363',
    }, sig1),
    true,
  );
}

// Test async one-shot sign with deterministic nonce.
{
  const ecPrivKey = fixtures.readKey('ec_p256_private.pem');

  crypto.sign('sha256', data, {
    key: ecPrivKey,
    dsaNonceType: 'deterministic',
  }, common.mustSucceed((sig) => {
    assert.strictEqual(
      crypto.verify('sha256', data, ecPrivKey, sig),
      true,
    );
  }));
}

// Test that dsaNonceType is ignored for non-EC/DSA keys (RSA).
{
  const rsaPrivKey = fixtures.readKey('rsa_private.pem');

  assert.throws(() => {
    crypto.sign('sha256', data, {
      key: rsaPrivKey,
      dsaNonceType: 'deterministic',
    });
  }, {
    code: 'ERR_CRYPTO_OPERATION_FAILED',
  });
}

// Test invalid dsaNonceType values.
{
  const ecPrivKey = fixtures.readKey('ec_p256_private.pem');

  for (const dsaNonceType of ['foo', 'random', null, {}, 5, true, NaN]) {
    assert.throws(() => {
      crypto.sign('sha256', data, {
        key: ecPrivKey,
        dsaNonceType,
      });
    }, {
      code: 'ERR_INVALID_ARG_VALUE',
    });
  }
}

// Test invalid dsaNonceType values with streaming API.
{
  const ecPrivKey = fixtures.readKey('ec_p256_private.pem');

  for (const dsaNonceType of ['foo', 'random', null, {}, 5, true, NaN]) {
    assert.throws(() => {
      crypto.createSign('sha256').update(data).sign({
        key: ecPrivKey,
        dsaNonceType,
      });
    }, {
      code: 'ERR_INVALID_ARG_VALUE',
    });
  }
}
