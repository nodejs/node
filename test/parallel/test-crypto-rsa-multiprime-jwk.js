'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const fixtures = require('../common/fixtures');
const { hasFIPS } = require('../common/crypto');
const {
  createPrivateKey,
} = require('crypto');
const { subtle } = globalThis.crypto;

if (process.features.openssl_is_boringssl)
  common.skip('multi-prime RSA is not available with BoringSSL');
if (hasFIPS())
  common.skip('multi-prime RSA is not available in FIPS mode');

const privateKey = createPrivateKey(
  fixtures.readKey('rsa_private_2048_3_primes.pem'));
const pkcs8 = privateKey.export({ format: 'der', type: 'pkcs8' });
const jwk = privateKey.export({ format: 'jwk' });

assert.strictEqual(jwk.oth.length, 1);
assert.deepStrictEqual(Object.keys(jwk.oth[0]), ['r', 'd', 't']);

const importedKey = createPrivateKey({ key: jwk, format: 'jwk' });
assert.deepStrictEqual(importedKey.export({ format: 'jwk' }), jwk);
assert.deepStrictEqual(
  importedKey.export({ format: 'der', type: 'pkcs8' }),
  pkcs8);

for (const field of ['r', 'd', 't']) {
  const invalidJwk = {
    ...jwk,
    oth: [{ ...jwk.oth[0] }],
  };
  delete invalidJwk.oth[0][field];
  assert.throws(
    () => createPrivateKey({ key: invalidJwk, format: 'jwk' }),
    { code: 'ERR_CRYPTO_INVALID_JWK' });
}

(async () => {
  const algorithm = { name: 'RSA-PSS', hash: 'SHA-256' };
  const cryptoKey = await subtle.importKey(
    'pkcs8', pkcs8, algorithm, true, ['sign']);
  const exportedJwk = await subtle.exportKey('jwk', cryptoKey);

  const exportedKeyMaterial = { ...exportedJwk };
  delete exportedKeyMaterial.key_ops;
  delete exportedKeyMaterial.ext;
  delete exportedKeyMaterial.alg;
  assert.deepStrictEqual(exportedKeyMaterial, jwk);

  const importedCryptoKey = await subtle.importKey(
    'jwk', exportedJwk, algorithm, true, ['sign']);
  assert.deepStrictEqual(
    Buffer.from(await subtle.exportKey('pkcs8', importedCryptoKey)),
    pkcs8);
})().then(common.mustCall());
