'use strict';

const assert = require('node:assert');
const {
  createHash,
  createCipheriv,
  createMac,
  getCipherInfo,
  getCiphers,
  getCurves,
  getHashes,
  getMacs,
  setFips,
} = require('node:crypto');
const { setDeserializeMainFunction } = require('node:v8').startupSnapshot;

const cipher = 'aes-128-cbc-cts';
const key = Buffer.alloc(16);
const iv = Buffer.alloc(16);
const legacyCipher = 'blowfish';
const legacyHash = 'md4';
const mac = 'poly1305';
const macKey = Buffer.from(
  '85d6be7857556d337f4452fe42d506a8' +
  '0103808afb0db2fd4abff6af4149f51b',
  'hex',
);
const macData = Buffer.from('Cryptographic Forum Research Group');
const macExpected = 'a8061dc1305136c6c22b8baf0c0127a9';

setFips(0);
const hasMac = getMacs().includes(mac);
const cases = [
  { name: 'getCiphers', get: getCiphers, algorithm: cipher, legacy: legacyCipher },
  { name: 'getHashes', get: getHashes, algorithm: 'md5', legacy: legacyHash },
  { name: 'getMacs', get: getMacs, algorithm: hasMac ? mac : undefined },
  { name: 'getCurves', get: getCurves, algorithm: 'secp256k1' },
];

function assertMac() {
  if (!hasMac) return;
  assert.strictEqual(
    createMac(mac, macKey).update(macData).final('hex'),
    macExpected,
  );
}

function assertHash() {
  assert.strictEqual(
    createHash('md5').digest('hex'),
    'd41d8cd98f00b204e9800998ecf8427e',
  );
}

for (const { name, get, algorithm, legacy } of cases) {
  const list = get();
  if (algorithm !== undefined)
    assert(list.includes(algorithm), `${name}: ${algorithm}`);
  if (legacy !== undefined)
    assert(list.includes(legacy), `${name}: ${legacy}`);
}
assert(getCipherInfo(cipher));
createCipheriv(cipher, key, iv);
createHash(legacyHash).digest();
assertHash();
assertMac();

setDeserializeMainFunction(() => {
  // Resolve native handles and JavaScript alias IDs before refreshing any
  // algorithm list. Build-time caches must not survive snapshot serialization.
  assertMac();
  assertHash();
  assert(getCipherInfo(cipher));
  createCipheriv(cipher, key, iv);

  const restoredLists = cases.map(({ name, get, algorithm, legacy }) => {
    const list = get();
    if (algorithm !== undefined)
      assert(list.includes(algorithm), `${name}: ${algorithm}`);
    if (legacy !== undefined)
      assert(!list.includes(legacy), `${name}: ${legacy}`);

    const disposable = get();
    assert.notStrictEqual(disposable, list, name);
    disposable.length = 0;
    disposable.push('not-a-real-algorithm');
    assert.deepStrictEqual(get(), list, name);
    return list;
  });
  assertMac();
  assert.throws(
    () => createCipheriv(legacyCipher, key, Buffer.alloc(8)),
    { code: 'ERR_OSSL_EVP_UNSUPPORTED' },
  );
  assert.throws(
    () => createHash(legacyHash),
    { code: 'ERR_OSSL_EVP_UNSUPPORTED' },
  );

  setFips(1);
  for (const { name, get, algorithm } of cases) {
    const list = get();
    if (algorithm !== undefined)
      assert(!list.includes(algorithm), `${name}: ${algorithm}`);
  }
  assert.strictEqual(getCipherInfo(cipher), undefined);
  assert.throws(() => createCipheriv(cipher, key, iv), {
    code: 'ERR_CRYPTO_UNKNOWN_CIPHER',
  });
  if (hasMac) {
    assert.throws(() => createMac(mac, macKey), {
      code: 'ERR_CRYPTO_INVALID_MAC',
    });
  }

  setFips(0);
  for (const [index, { name, get }] of cases.entries()) {
    assert.deepStrictEqual(get(), restoredLists[index], name);
  }
  assert(getCipherInfo(cipher));
  createCipheriv(cipher, key, iv);
  assertHash();
  assertMac();
  console.log('provider crypto caches snapshot: ok');
});
