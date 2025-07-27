'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const { hasOpenSSL } = require('../common/crypto');

if (!hasOpenSSL(3, 5))
  common.skip('requires OpenSSL >= 3.5');

const assert = require('assert');
const {
  randomBytes,
  sign,
  verify,
} = require('crypto');

const fixtures = require('../common/fixtures');

function getKeyFileName(type, suffix) {
  return `${type.replaceAll('-', '_')}_${suffix}.pem`;
}

for (const [asymmetricKeyType, sigLen] of [
  ['ml-dsa-44', 2420], ['ml-dsa-65', 3309], ['ml-dsa-87', 4627],
]) {
  const keys = {
    public: fixtures.readKey(getKeyFileName(asymmetricKeyType, 'public'), 'ascii'),
    private: fixtures.readKey(getKeyFileName(asymmetricKeyType, 'private'), 'ascii'),
    private_seed_only: fixtures.readKey(getKeyFileName(asymmetricKeyType, 'private_seed_only'), 'ascii'),
    private_priv_only: fixtures.readKey(getKeyFileName(asymmetricKeyType, 'private_priv_only'), 'ascii'),
  };

  for (const privateKey of [keys.private, keys.private_seed_only, keys.private_priv_only]) {
    for (const data of [randomBytes(0), randomBytes(1), randomBytes(32), randomBytes(128), randomBytes(1024)]) {
      // sync
      {
        const signature = sign(undefined, data, privateKey);
        assert.strictEqual(signature.byteLength, sigLen);
        assert.strictEqual(verify(undefined, randomBytes(32), keys.public, signature), false);
        assert.strictEqual(verify(undefined, data, keys.public, Buffer.alloc(sigLen)), false);
        assert.strictEqual(verify(undefined, data, keys.public, signature), true);
        assert.strictEqual(verify(undefined, data, privateKey, signature), true);
        assert.throws(() => sign('sha256', data, privateKey), { code: 'ERR_OSSL_INVALID_DIGEST' });
        assert.throws(
          () => verify('sha256', data, keys.public, Buffer.alloc(sigLen)),
          { code: 'ERR_OSSL_INVALID_DIGEST' });
      }

      // async
      {
        sign(undefined, data, privateKey, common.mustSucceed((signature) => {
          assert.strictEqual(signature.byteLength, sigLen);
          verify(undefined, data, privateKey, signature, common.mustSucceed((valid) => {
            assert.strictEqual(valid, true);
          }));
          verify(undefined, data, privateKey, Buffer.alloc(sigLen), common.mustSucceed((valid) => {
            assert.strictEqual(valid, false);
          }));
        }));

        sign('sha256', data, privateKey, common.expectsError(/invalid digest/));
        verify('sha256', data, keys.public, Buffer.alloc(sigLen), common.expectsError(/invalid digest/));
      }
    }
  }
}
