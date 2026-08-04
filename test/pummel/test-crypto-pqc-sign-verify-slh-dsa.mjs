import * as common from '../common/index.mjs';

if (!common.hasCrypto)
  common.skip('missing crypto');

import { hasOpenSSL } from '../common/crypto.js';

if (!hasOpenSSL(3, 5))
  common.skip('requires OpenSSL >= 3.5');

import * as assert from 'node:assert';
import { promisify } from 'node:util';
import { randomBytes, sign, verify } from 'node:crypto';
import fixtures from '../common/fixtures.js';

function getKeyFileName(type, suffix) {
  return `${type.replaceAll('-', '_')}_${suffix}.pem`;
}

for (const [asymmetricKeyType, sigLen] of [
  ['slh-dsa-sha2-128f', 17088],
  ['slh-dsa-sha2-128s', 7856],
  ['slh-dsa-sha2-192f', 35664],
  ['slh-dsa-sha2-192s', 16224],
  ['slh-dsa-sha2-256f', 49856],
  ['slh-dsa-sha2-256s', 29792],
  ['slh-dsa-shake-128f', 17088],
  ['slh-dsa-shake-128s', 7856],
  ['slh-dsa-shake-192f', 35664],
  ['slh-dsa-shake-192s', 16224],
  ['slh-dsa-shake-256f', 49856],
  ['slh-dsa-shake-256s', 29792],
]) {
  const keys = {
    public: fixtures.readKey(getKeyFileName(asymmetricKeyType, 'public'), 'ascii'),
    private: fixtures.readKey(getKeyFileName(asymmetricKeyType, 'private'), 'ascii'),
  };

  const data = randomBytes(32);

  // sync
  {
    const signature = sign(undefined, data, keys.private);
    assert.strictEqual(signature.byteLength, sigLen);
    assert.strictEqual(verify(undefined, randomBytes(32), keys.public, signature), false);
    assert.strictEqual(verify(undefined, data, keys.public, signature), true);
  }

  // async
  {
    const pSign = promisify(sign);
    const pVerify = promisify(verify);
    const signature = await pSign(undefined, data, keys.private);
    assert.strictEqual(signature.byteLength, sigLen);
    assert.strictEqual(await pVerify(undefined, randomBytes(32), keys.public, signature), false);
    assert.strictEqual(await pVerify(undefined, data, keys.public, signature), true);
  }

  assert.throws(() => sign('sha256', data, keys.private), { code: 'ERR_OSSL_INVALID_DIGEST' });
  assert.throws(
    () => verify('sha256', data, keys.public, Buffer.alloc(sigLen)),
    { code: 'ERR_OSSL_INVALID_DIGEST' });
}
