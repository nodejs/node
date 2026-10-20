'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const { isBoringSSL } = require('../common/crypto');
if (isBoringSSL)
  common.skip('missing SHA-3');

const assert = require('assert');
const { createPrivateKey, createPublicKey } = require('crypto');
const fixtures = require('../common/fixtures');
const { subtle } = globalThis.crypto;

(async () => {
  const privateKey = createPrivateKey(fixtures.readKey('rsa_private_2048.pem'));
  const privateJwk = privateKey.export({ format: 'jwk' });
  const publicJwk = createPublicKey(privateKey).export({ format: 'jwk' });

  for (const name of ['RSA-PSS', 'RSASSA-PKCS1-v1_5', 'RSA-OAEP']) {
    for (const hash of ['SHA3-256', 'SHA3-384', 'SHA3-512']) {
      for (const jwk of [publicJwk, privateJwk]) {
        const usages = name === 'RSA-OAEP' ? [jwk.d ? 'decrypt' : 'encrypt'] :
          [jwk.d ? 'sign' : 'verify'];
        const algorithm = { name, hash };
        // There is no JWK alg identifier for RSA with SHA-3. Omitting alg is
        // valid, but an identifier for SHA-2 or an unknown identifier is not.
        const key = await subtle.importKey('jwk', jwk, algorithm, true, usages);
        const exported = await subtle.exportKey('jwk', key);
        assert.strictEqual(Object.hasOwn(exported, 'alg'), false);
        const imported = await subtle.importKey('jwk', exported, algorithm, true, usages);
        assert.deepStrictEqual(imported.algorithm, key.algorithm);
        assert.deepStrictEqual(await subtle.exportKey('jwk', imported), exported);
        for (const alg of ['RS256', 'PS256', 'RSA-OAEP-256', 'unknown']) {
          await assert.rejects(subtle.importKey(
            'jwk', { ...jwk, alg }, algorithm, true, usages), { name: 'DataError' });
        }
      }
    }
  }
})().then(common.mustCall());
