'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const { createPublicKey, subtle } = require('crypto');

// This is the valid P-256 point (0, sqrt(b)). A failed conversion of an
// oversized x coordinate must not silently replace it with zero.
const jwk = {
  kty: 'EC',
  crv: 'P-256',
  x: Buffer.alloc(32).toString('base64url'),
  y: 'ZkhceA4vg9ckM71dhKBrtlQcKvMdrocXKL-FahdPk_Q',
};

(async () => {
  for (const field of ['x', 'y']) {
    const invalid = { ...jwk, [field]: Buffer.alloc(33, 1).toString('base64url') };
    assert.throws(() => createPublicKey({ key: invalid, format: 'jwk' }), {
      code: 'ERR_CRYPTO_INVALID_JWK',
    });
    for (const name of ['ECDSA', 'ECDH']) {
      await assert.rejects(subtle.importKey(
        'jwk', invalid, { name, namedCurve: 'P-256' }, true,
        name === 'ECDSA' ? ['verify'] : []), { name: 'DataError' });
    }
  }

  // Equivalent integer encodings remain accepted by the existing decoder.
  for (const encoded of [
    jwk,
    { ...jwk, x: Buffer.alloc(1).toString('base64url') },
    {
      ...jwk,
      x: Buffer.alloc(33).toString('base64url'),
      y: Buffer.concat([Buffer.alloc(1), Buffer.from(jwk.y, 'base64url')]).toString('base64url'),
    },
  ]) {
    const publicKey = createPublicKey({ key: encoded, format: 'jwk' });
    assert.deepStrictEqual(publicKey.export({ format: 'jwk' }), jwk);
    for (const name of ['ECDSA', 'ECDH']) {
      const key = await subtle.importKey(
        'jwk', encoded, { name, namedCurve: 'P-256' }, true,
        name === 'ECDSA' ? ['verify'] : []);
      const exported = await subtle.exportKey('jwk', key);
      assert.strictEqual(exported.x, jwk.x);
      assert.strictEqual(exported.y, jwk.y);
    }
  }
})().then(common.mustCall());
