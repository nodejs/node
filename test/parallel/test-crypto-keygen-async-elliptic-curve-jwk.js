'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const {
  generateKeyPair,
} = require('crypto');
const { hasFIPS, isBoringSSL } = require('../common/crypto');
const rejectsXCurves = hasFIPS(3, 5);

// Test async elliptic curve key generation with 'jwk' encoding.
{
  for (const type of ['ed25519', 'ed448', 'x25519', 'x448']) {
    if (isBoringSSL && type.endsWith('448')) {
      common.printSkipMessage(`Skipping unsupported ${type} test case`);
      continue;
    }
    const options = {
      publicKeyEncoding: {
        format: 'jwk'
      },
      privateKeyEncoding: {
        format: 'jwk'
      }
    };
    if (rejectsXCurves && type.startsWith('x')) {
      assert.throws(() => generateKeyPair(type, options, common.mustNotCall()), {
        code: 'ERR_INVALID_ARG_VALUE',
      });
      continue;
    }
    generateKeyPair(type, options, common.mustSucceed((publicKey, privateKey) => {
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
