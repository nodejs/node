'use strict';

const assert = require('assert');
const {
  createHash,
  createCipheriv,
  getCipherInfo,
  getCiphers,
  getHashes,
  setFips,
} = require('crypto');
const { setDeserializeMainFunction } = require('v8').startupSnapshot;

const algorithm = 'aes-128-cbc-cts';
const key = Buffer.alloc(16);
const iv = Buffer.alloc(16);
const legacyCipher = 'blowfish';
const legacyHash = 'md4';

setFips(0);
assert(getCiphers().includes(algorithm));
assert(getCiphers().includes(legacyCipher));
assert(getHashes().includes(legacyHash));
assert(getCipherInfo(algorithm));
createCipheriv(algorithm, key, iv);
createHash(legacyHash).digest();

setDeserializeMainFunction(() => {
  assert(getCiphers().includes(algorithm));
  assert(!getCiphers().includes(legacyCipher));
  assert(!getHashes().includes(legacyHash));
  assert(getCipherInfo(algorithm));
  createCipheriv(algorithm, key, iv);
  assert.throws(
    () => createCipheriv(legacyCipher, key, Buffer.alloc(8)),
    { code: 'ERR_OSSL_EVP_UNSUPPORTED' },
  );
  assert.throws(
    () => createHash(legacyHash),
    { code: 'ERR_OSSL_EVP_UNSUPPORTED' },
  );

  setFips(1);
  assert(!getCiphers().includes(algorithm));
  assert.strictEqual(getCipherInfo(algorithm), undefined);
  assert.throws(() => createCipheriv(algorithm, key, iv), {
    code: 'ERR_CRYPTO_UNKNOWN_CIPHER',
  });

  setFips(0);
  assert(getCiphers().includes(algorithm));
  assert(getCipherInfo(algorithm));
  createCipheriv(algorithm, key, iv);
  console.log('provider crypto caches snapshot: ok');
});
