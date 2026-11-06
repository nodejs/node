'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('node:assert');
const {
  Certificate,
  createPrivateKey,
  createPublicKey,
  diffieHellman,
  generateKeyPairSync,
} = require('node:crypto');
const fixtures = require('../common/fixtures');

const privateKeyPem = fixtures.readKey('rsa_private.pem');
const rsaPublicKey = createPublicKey(fixtures.readKey('rsa_public.pem'));
const ec = generateKeyPairSync('ec', { namedCurve: 'P-256' });

function parseValidPrivateKey() {
  // This must not pick up an error from the preceding operation.
  createPrivateKey(privateKeyPem);
}

assert.strictEqual(rsaPublicKey.equals(ec.publicKey), false);
parseValidPrivateKey();

for (const method of ['verifySpkac', 'exportPublicKey', 'exportChallenge']) {
  Certificate[method](Buffer.from('garbage'));
  parseValidPrivateKey();
}

assert.throws(() => diffieHellman({
  privateKey: ec.privateKey,
  publicKey: {
    key: {
      kty: 'EC',
      crv: 'P-256',
      x: 'AAAA',
      y: 'AAAA',
    },
    format: 'jwk',
  },
}), { code: 'ERR_CRYPTO_INVALID_JWK' });
parseValidPrivateKey();
