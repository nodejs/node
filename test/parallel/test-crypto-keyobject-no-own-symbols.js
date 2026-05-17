'use strict';

// KeyObject instances must not expose own string or Symbol properties, even
// after the native slot tuple and lazy metadata cache have been populated.

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('node:assert');
const {
  createSecretKey,
  generateKeyPairSync,
} = require('node:crypto');

function assertNoOwnKeys(key) {
  assert.deepStrictEqual(Object.getOwnPropertySymbols(key), []);
  assert.deepStrictEqual(Object.getOwnPropertyNames(key), []);
  assert.deepStrictEqual(Reflect.ownKeys(key), []);
}

{
  const secret = createSecretKey(Buffer.alloc(16));
  const { publicKey, privateKey } = generateKeyPairSync('rsa', {
    modulusLength: 1024,
  });

  for (const key of [secret, publicKey, privateKey]) {
    const type = key.type;
    assert.notStrictEqual(type, undefined);
    if (type === 'secret') {
      assert.strictEqual(key.symmetricKeySize, 16);
      key.export();
    } else {
      assert.notStrictEqual(key.asymmetricKeyType, undefined);
      assert.notStrictEqual(key.asymmetricKeyDetails, undefined);
      key.export({ format: 'pem', type: 'pkcs1' });
    }
    key.equals(key);
    assertNoOwnKeys(key);
  }
}
