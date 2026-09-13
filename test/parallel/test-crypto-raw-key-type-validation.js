'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('node:assert');
const { createPrivateKey, createPublicKey } = require('node:crypto');
const { hasOpenSSL, hasFIPS, isBoringSSL } = require('../common/crypto');

const key = Buffer.alloc(32);
const formats = ['raw-public', 'raw-private', 'raw-seed'];

// Recognized classical types reject raw encodings with a format error.
for (const asymmetricKeyType of ['rsa', 'rsa-pss', 'dsa', 'dh']) {
  for (const format of formats) {
    assert.throws(() => createPublicKey({ key, format, asymmetricKeyType }), {
      code: 'ERR_CRYPTO_INCOMPATIBLE_KEY_OPTIONS',
    });
  }
}

// Only exact public names are accepted, excluding aliases and unexposed types.
for (const asymmetricKeyType of [
  'RSA', 'RSA-PSS', 'DSA', 'DH', 'EC', 'sm2', 'SM2', 'unknown',
  'Ed25519', 'ED25519', 'Ed448', 'ED448', 'X25519', 'X448',
  'ML-DSA-44', 'Ml-Dsa-65', 'ML-DSA-87',
  'ML-KEM-512', 'Ml-Kem-768', 'ML-KEM-1024', 'SLH-DSA-SHA2-128s',
  'rsaEncryption', 'id-ecPublicKey', '1.3.101.112', '1.3.101.110',
  'ed25519 ', ' ml-dsa-44',
  'rsa\0suffix', 'ec\0suffix', 'ed25519\0suffix', 'ml-dsa-44\0suffix',
]) {
  for (const format of formats) {
    assert.throws(() => createPublicKey({
      key, format, asymmetricKeyType, namedCurve: 'P-256',
    }), {
      code: 'ERR_INVALID_ARG_VALUE',
      message: `Invalid asymmetricKeyType: ${asymmetricKeyType.split('\0')[0]}`,
    });
  }
}

assert.throws(() => createPublicKey({
  key, format: 'raw-seed', asymmetricKeyType: 'ec', namedCurve: 'P-256',
}), { code: 'ERR_CRYPTO_INCOMPATIBLE_KEY_OPTIONS' });

// Recognized OKP and PQC names retain their raw-format restrictions.
for (const asymmetricKeyType of ['ed25519', 'x25519']) {
  const options = { key, format: 'raw-private', asymmetricKeyType };
  if (asymmetricKeyType === 'x25519' && hasFIPS(3, 5)) {
    assert.throws(() => createPrivateKey(options), {
      code: 'ERR_INVALID_ARG_VALUE', message: 'Invalid key data',
    });
  } else {
    const imported = createPrivateKey(options);
    assert.strictEqual(imported.asymmetricKeyType, asymmetricKeyType);
  }
  assert.throws(() => createPrivateKey({
    key, format: 'raw-seed', asymmetricKeyType,
  }), { code: 'ERR_CRYPTO_INCOMPATIBLE_KEY_OPTIONS' });
}

{
  const asymmetricKeyType = 'ml-dsa-44';
  if (hasOpenSSL(3, 5) || isBoringSSL) {
    const imported = createPrivateKey({
      key, format: 'raw-seed', asymmetricKeyType,
    });
    assert.strictEqual(imported.asymmetricKeyType, 'ml-dsa-44');
    assert.throws(() => createPrivateKey({
      key, format: 'raw-private', asymmetricKeyType,
    }), { code: 'ERR_CRYPTO_INCOMPATIBLE_KEY_OPTIONS' });
  } else {
    for (const format of formats) {
      assert.throws(() => createPublicKey({ key, format, asymmetricKeyType }), {
        code: 'ERR_INVALID_ARG_VALUE',
        message: 'Unsupported key type',
      });
    }
  }
}
