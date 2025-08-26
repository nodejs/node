'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const { hasOpenSSL } = require('../common/crypto');

const assert = require('assert');
const {
  generateKeyPair,
} = require('crypto');

if (!hasOpenSSL(3, 5)) {
  for (const asymmetricKeyType of [
    'slh-dsa-sha2-128f', 'slh-dsa-sha2-128s', 'slh-dsa-sha2-192f', 'slh-dsa-sha2-192s',
    'slh-dsa-sha2-256f', 'slh-dsa-sha2-256s', 'slh-dsa-shake-128f', 'slh-dsa-shake-128s',
    'slh-dsa-shake-192f', 'slh-dsa-shake-192s', 'slh-dsa-shake-256f', 'slh-dsa-shake-256s',
  ]) {
    assert.throws(() => generateKeyPair(asymmetricKeyType, common.mustNotCall()), {
      code: 'ERR_INVALID_ARG_VALUE',
      message: /The argument 'type' must be a supported key type/
    });
  }
} else {
  for (const asymmetricKeyType of [
    'slh-dsa-sha2-128f', 'slh-dsa-sha2-128s', 'slh-dsa-sha2-192f', 'slh-dsa-sha2-192s',
    'slh-dsa-sha2-256f', 'slh-dsa-sha2-256s', 'slh-dsa-shake-128f', 'slh-dsa-shake-128s',
    'slh-dsa-shake-192f', 'slh-dsa-shake-192s', 'slh-dsa-shake-256f', 'slh-dsa-shake-256s',
  ]) {
    for (const [publicKeyEncoding, validate] of [
      [undefined, (publicKey) => {
        assert.strictEqual(publicKey.type, 'public');
        assert.strictEqual(publicKey.asymmetricKeyType, asymmetricKeyType);
        assert.deepStrictEqual(publicKey.asymmetricKeyDetails, {});
      }],
      [{ format: 'pem', type: 'spki' }, (publicKey) => assert.strictEqual(typeof publicKey, 'string')],
      [{ format: 'der', type: 'spki' }, (publicKey) => assert.strictEqual(Buffer.isBuffer(publicKey), true)],
    ]) {
      generateKeyPair(asymmetricKeyType, { publicKeyEncoding }, common.mustSucceed(validate));
    }
    for (const [privateKeyEncoding, validate] of [
      [undefined, (_, privateKey) => {
        assert.strictEqual(privateKey.type, 'private');
        assert.strictEqual(privateKey.asymmetricKeyType, asymmetricKeyType);
        assert.deepStrictEqual(privateKey.asymmetricKeyDetails, {});
      }],
      [{ format: 'pem', type: 'pkcs8' }, (_, privateKey) => assert.strictEqual(typeof privateKey, 'string')],
      [{ format: 'der', type: 'pkcs8' }, (_, privateKey) => assert.strictEqual(Buffer.isBuffer(privateKey), true)],
    ]) {
      generateKeyPair(asymmetricKeyType, { privateKeyEncoding }, common.mustSucceed(validate));
    }
  }
}
