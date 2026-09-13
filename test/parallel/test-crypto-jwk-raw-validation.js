'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('node:assert');
const { createPrivateKey } = require('node:crypto');
const fixtures = require('../common/fixtures');
const { hasOpenSSL, isBoringSSL } = require('../common/crypto');

const cases = [
  ['ed25519_private.pem', 'OKP', 'crv', 'x', 'd'],
];
if (hasOpenSSL(3, 5) || isBoringSSL) {
  cases.push(['ml_dsa_44_private_seed_only.pem', 'AKP', 'alg', 'pub', 'priv']);
}

for (const [file, kty, name, pub, priv] of cases) {
  const jwk = createPrivateKey(fixtures.readKey(file)).export({ format: 'jwk' });
  const invalid = { code: 'ERR_CRYPTO_INVALID_JWK', message: `Invalid JWK ${kty} key` };
  const invalidName = kty === 'AKP' ? {
    code: 'ERR_CRYPTO_INVALID_JWK', message: 'Unsupported JWK AKP "alg"',
  } : invalid;
  const importKey = (key) => createPrivateKey({ format: 'jwk', key });

  assert.throws(() => importKey({ ...jwk, [name]: 'unknown' }), invalidName);
  assert.throws(() => importKey({ ...jwk, [name]: jwk[name].toLowerCase() }), invalidName);
  assert.throws(() => importKey({ ...jwk, [pub]: undefined }), invalid);
  assert.throws(() => importKey({ ...jwk, [priv]: 1 }), invalid);
  assert.throws(() => importKey({ ...jwk, [name]: 'unknown', [pub]: undefined }), invalidName);

  // A recognized algorithm from the other schema remains invalid.
  const otherName = kty === 'AKP' ? 'Ed25519' : 'ML-DSA-44';
  assert.throws(() => importKey({ ...jwk, [name]: otherName }), invalidName);

  for (const field of [name, pub, priv]) {
    const error = new Error(`getter for ${field}`);
    const key = { ...jwk };
    Object.defineProperty(key, field, { get() { throw error; } });
    assert.throws(() => importKey(key), (actual) => actual === error);
  }
}
