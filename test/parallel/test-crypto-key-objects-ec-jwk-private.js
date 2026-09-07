'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const {
  createECDH,
  createPrivateKey,
  createPublicKey,
  getCurves,
  getFips,
  sign,
  verify,
} = require('crypto');

const curves = [
  ['prime256v1', 'P-256', 32,
   'ffffffff00000000ffffffffffffffffbce6faada7179e84f3b9cac2fc632551'],
  ['secp384r1', 'P-384', 48,
   'ffffffffffffffffffffffffffffffffffffffffffffffffc7634d81f4372ddf' +
   '581a0db248b0a77aecec196accc52973'],
  ['secp521r1', 'P-521', 66,
   '01ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff' +
   'fa51868783bf2f966b7fcc0148f709a5d03bb5c9b8899c47aebb6fb71e91386409'],
];
if (!getFips() && getCurves().includes('secp256k1')) {
  curves.push(['secp256k1', 'secp256k1', 32,
               'fffffffffffffffffffffffffffffffebaaedce6af48a03bbfd25e8cd0364141']);
}

for (const [namedCurve, crv, width, orderHex] of curves) {
  const order = BigInt(`0x${orderHex}`);
  const encode = (scalar) => Buffer.from(
    scalar.toString(16).padStart(width * 2, '0'), 'hex');
  const makeJwk = (scalar) => {
    const ecdh = createECDH(namedCurve);
    ecdh.setPrivateKey(encode(scalar));
    const point = ecdh.getPublicKey();
    return {
      kty: 'EC',
      crv,
      x: point.subarray(1, 1 + width).toString('base64url'),
      y: point.subarray(1 + width).toString('base64url'),
      d: encode(scalar).toString('base64url'),
    };
  };
  const generator = makeJwk(1n);
  const other = makeJwk(2n);
  const message = Buffer.from('EC JWK private key consistency');

  for (const jwk of [generator, other, makeJwk(order - 1n)]) {
    const key = createPrivateKey({ format: 'jwk', key: jwk });
    assert.deepStrictEqual(key.export({ format: 'jwk' }), jwk);
    const publicJwk = { kty: jwk.kty, crv, x: jwk.x, y: jwk.y };
    const publicKey = createPublicKey({ format: 'jwk', key: publicJwk });
    assert(verify('sha256', message, publicKey, sign('sha256', message, key)));
  }

  const invalid = [
    { ...generator, d: other.d },
    { ...generator, x: other.x, y: other.y },
    ...[0n, order, order + 1n].map((scalar) => ({
      ...generator, d: encode(scalar).toString('base64url'),
    })),
    { ...generator, d: '' },
    {
      ...generator,
      x: Buffer.alloc(width).toString('base64url'),
      y: Buffer.alloc(width).toString('base64url'),
    },
  ];
  for (const jwk of invalid) {
    assert.throws(() => createPrivateKey({ format: 'jwk', key: jwk }), {
      code: 'ERR_CRYPTO_INVALID_JWK',
    });
  }
}
