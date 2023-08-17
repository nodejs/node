'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const {
  generateKeyPair,
} = require('crypto');

// Test async elliptic curve key generation with 'jwk' encoding
{
  [
    ['ec', ['P-384', 'P-256', 'P-521', 'secp256k1']],
    ['rsa'],
    ['ed25519'],
    ['ed448'],
    ['x25519'],
    ['x448'],
  ].forEach((types) => {
    const [type, options] = types;
    switch (type) {
      case 'ec': {
        return options.forEach((curve) => {
          generateKeyPair(type, {
            namedCurve: curve,
            publicKeyEncoding: {
              format: 'jwk'
            },
            privateKeyEncoding: {
              format: 'jwk'
            }
          }, common.mustSucceed((publicKey, privateKey) => {
            assert.strictEqual(typeof publicKey, 'object');
            assert.strictEqual(typeof privateKey, 'object');
            assert.strictEqual(publicKey.x, privateKey.x);
            assert.strictEqual(publicKey.y, privateKey.y);
            assert(!publicKey.d);
            assert(privateKey.d);
            assert.strictEqual(publicKey.kty, 'EC');
            assert.strictEqual(publicKey.kty, privateKey.kty);
            assert.strictEqual(publicKey.crv, curve);
            assert.strictEqual(publicKey.crv, privateKey.crv);
          }));
        });
      }
      case 'rsa': {
        return generateKeyPair(type, {
          modulusLength: 4096,
          publicKeyEncoding: {
            format: 'jwk'
          },
          privateKeyEncoding: {
            format: 'jwk'
          }
        }, common.mustSucceed((publicKey, privateKey) => {
          assert.strictEqual(typeof publicKey, 'object');
          assert.strictEqual(typeof privateKey, 'object');
          assert.strictEqual(publicKey.kty, 'RSA');
          assert.strictEqual(publicKey.kty, privateKey.kty);
          assert.strictEqual(typeof publicKey.n, 'string');
          assert.strictEqual(publicKey.n, privateKey.n);
          assert.strictEqual(typeof publicKey.e, 'string');
          assert.strictEqual(publicKey.e, privateKey.e);
          assert.strictEqual(typeof privateKey.d, 'string');
          assert.strictEqual(typeof privateKey.p, 'string');
          assert.strictEqual(typeof privateKey.q, 'string');
          assert.strictEqual(typeof privateKey.dp, 'string');
          assert.strictEqual(typeof privateKey.dq, 'string');
          assert.strictEqual(typeof privateKey.qi, 'string');
        }));
      }
      case 'ed25519':
      case 'ed448':
      case 'x25519':
      case 'x448': {
        generateKeyPair(type, {
          publicKeyEncoding: {
            format: 'jwk'
          },
          privateKeyEncoding: {
            format: 'jwk'
          }
        }, common.mustSucceed((publicKey, privateKey) => {
          assert.strictEqual(typeof publicKey, 'object');
          assert.strictEqual(typeof privateKey, 'object');
          assert.strictEqual(publicKey.x, privateKey.x);
          assert(!publicKey.d);
          assert(privateKey.d);
          assert.strictEqual(publicKey.kty, 'OKP');
          assert.strictEqual(publicKey.kty, privateKey.kty);
          const expectedCrv = `${type.charAt(0).toUpperCase()}${type.slice(1)}`;
          assert.strictEqual(publicKey.crv, expectedCrv);
          assert.strictEqual(publicKey.crv, privateKey.crv);
        }));
      }
    }
  });
}
