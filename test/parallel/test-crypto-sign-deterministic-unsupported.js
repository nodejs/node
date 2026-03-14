'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const { hasOpenSSL } = require('../common/crypto');

if (hasOpenSSL(3, 2))
  common.skip('requires OpenSSL < 3.2');

const assert = require('assert');
const crypto = require('crypto');
const fixtures = require('../common/fixtures');

const data = Buffer.from('Hello world');
const ecPrivKey = fixtures.readKey('ec_p256_private.pem');
const dsaPrivKey = fixtures.readKey('dsa_private.pem');

const expectedError = {
  code: 'ERR_CRYPTO_OPERATION_FAILED',
  message: /dsaNonceType/,
};

// Streaming API: sign with deterministic nonce must throw for EC.
{
  assert.throws(() => {
    crypto.createSign('sha256').update(data).sign({
      key: ecPrivKey,
      dsaNonceType: 'deterministic',
    });
  }, expectedError);
}

// Streaming API: sign with deterministic nonce must throw for DSA.
{
  assert.throws(() => {
    crypto.createSign('sha256').update(data).sign({
      key: dsaPrivKey,
      dsaNonceType: 'deterministic',
    });
  }, expectedError);
}

// One-shot sync API: sign with deterministic nonce must throw for EC.
{
  assert.throws(() => {
    crypto.sign('sha256', data, {
      key: ecPrivKey,
      dsaNonceType: 'deterministic',
    });
  }, expectedError);
}

// One-shot sync API: sign with deterministic nonce must throw for DSA.
{
  assert.throws(() => {
    crypto.sign('sha256', data, {
      key: dsaPrivKey,
      dsaNonceType: 'deterministic',
    });
  }, expectedError);
}

// One-shot async API: sign with deterministic nonce must error for EC.
{
  crypto.sign('sha256', data, {
    key: ecPrivKey,
    dsaNonceType: 'deterministic',
  }, common.mustCall((err) => {
    assert(err);
  }));
}

// One-shot async API: sign with deterministic nonce must error for DSA.
{
  crypto.sign('sha256', data, {
    key: dsaPrivKey,
    dsaNonceType: 'deterministic',
  }, common.mustCall((err) => {
    assert(err);
  }));
}

// dsaNonceType: 'random' should still work (explicit default).
{
  const sig = crypto.sign('sha256', data, {
    key: ecPrivKey,
    dsaNonceType: 'random',
  });
  assert.strictEqual(
    crypto.verify('sha256', data, ecPrivKey, sig),
    true,
  );
}

// Invalid dsaNonceType values should still throw validation errors.
{
  for (const dsaNonceType of ['foo', null, {}, 5, true, NaN]) {
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
