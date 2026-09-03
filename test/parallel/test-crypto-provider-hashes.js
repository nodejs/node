// Flags: --expose-internals
'use strict';

const common = require('../common');

if (!common.hasCrypto) {
  common.skip('missing crypto');
}

const assert = require('node:assert');
const {
  createHash,
  createHmac,
  createSign,
  createVerify,
  generateKeyPair,
  generateKeyPairSync,
  getHashes,
  hash,
  hkdf,
  hkdfSync,
  pbkdf2,
  pbkdf2Sync,
  privateDecrypt,
  publicEncrypt,
  sign,
  verify,
} = require('node:crypto');
const { hasOpenSSL, isBoringSSL } = require('../common/crypto');

if (!hasOpenSSL(3) || isBoringSSL) {
  common.skip('OpenSSL 3 provider support is required');
}

const { internalBinding } = require('internal/test/binding');
const {
  HashJob,
  kCryptoJobSync,
  kCryptoJobWebCrypto,
} = internalBinding('crypto');

const hashes = getHashes();
const lowercaseHashes = hashes.map((name) => name.toLowerCase());
const modifiedHashes = getHashes();
modifiedHashes.length = 0;

assert.deepStrictEqual(hashes, [...hashes].sort());
assert.deepStrictEqual(getHashes(), hashes);
assert.strictEqual(new Set(lowercaseHashes).size, hashes.length);
if (lowercaseHashes.includes('sha1')) {
  assert(hashes.includes('RSA-SHA1'));
}
assert(!lowercaseHashes.includes('null'));
assert(!lowercaseHashes.includes('ml-dsa-mu'));
assert(!hashes.some((name) => /^\d+(?:\.\d+)+$/.test(name)));

for (const name of hashes) {
  try {
    createHash(name);
  } catch (err) {
    assert.strictEqual(err.code, 'ERR_OSSL_EVP_NOT_XOF_OR_INVALID_LENGTH');
    createHash(name, { outputLength: 32 });
  }
}

const input = Buffer.alloc(0);
assert.throws(
  () => createHash('ml-dsa-mu'),
  /Digest method not supported/,
);
assert.throws(
  () => hash('ml-dsa-mu', input),
  { message: 'Digest method ml-dsa-mu is not supported' },
);

const providerVectors = {
  'keccak-kmac-128': {
    aliases: ['keccak-kmac-128', 'keccak-kmac128'],
    expected: '83aa04c211dc19d16912571ed0a75130' +
              'd36aebd58562dd080c1ea84a8c7d73f7',
    options: { outputLength: 32 },
  },
  'keccak-256': {
    aliases: ['keccak-256'],
    expected: 'c5d2460186f7233c927e7db2dcc703c0' +
              'e500b653ca82273b7bfad8045d85a470',
  },
  'sha256-192': {
    aliases: ['sha2-256/192', 'sha-256/192', 'sha256-192'],
    expected: 'e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934c',
  },
};

function testHashVector({ aliases, expected, options }) {
  for (const alias of aliases) {
    if (!hashes.includes(alias)) continue;
    assert(!hashes.includes(alias.toUpperCase()));

    for (const name of [alias, alias.toUpperCase()]) {
      const streaming = createHash(name, options).update(input).digest('hex');
      assert.strictEqual(streaming, expected);
      assert.strictEqual(hash(name, input, options), expected);
    }
  }
}

// These digests are tested when the active provider advertises them.
for (const name of ['keccak-kmac-128', 'keccak-256', 'sha256-192']) {
  const vector = providerVectors[name];
  if (vector.aliases.some((alias) => hashes.includes(alias))) {
    testHashVector(vector);
  } else {
    common.printSkipMessage(`${name} is not available from the active provider`);
  }
}

const keccakKmacName = providerVectors['keccak-kmac-128'].aliases
  .find((alias) => hashes.includes(alias));
if (keccakKmacName !== undefined) {
  (async () => {
    const { expected } = providerVectors['keccak-kmac-128'];
    const { 0: err, 1: syncResult } = new HashJob(
      kCryptoJobSync,
      keccakKmacName,
      input,
      256,
    ).run();
    assert.strictEqual(err, undefined);
    assert.strictEqual(Buffer.from(syncResult).toString('hex'), expected);

    const result = await new HashJob(
      kCryptoJobWebCrypto,
      keccakKmacName,
      input,
      256,
    ).run();
    assert.strictEqual(Buffer.from(result).toString('hex'), expected);
  })().then(common.mustCall());
}

if (hashes.includes('sha256-192')) {
  const operationInput = Buffer.from('abc');

  assert.strictEqual(
    createHmac('sha256-192', 'key').update(operationInput).digest('hex'),
    'd7774e586190fa2d2f4d4be4bc86ccd459a9170d52c38809',
  );

  const hkdfExpected = 'ef23757b94b5e1e46c3f981d87828d7aeb0207733ab5c78' +
                       'c60df321c9e8c88e0ad54b4eecfef8c258ccd';
  assert.strictEqual(
    Buffer.from(hkdfSync('sha256-192', 'key', 'salt', 'info', 42))
      .toString('hex'),
    hkdfExpected,
  );
  hkdf(
    'sha256-192',
    'key',
    'salt',
    'info',
    42,
    common.mustSucceed((result) => {
      assert.strictEqual(Buffer.from(result).toString('hex'), hkdfExpected);
    }),
  );

  const pbkdf2Expected = '1fee3dd5ea13d5b563d3cc88fbc6dcf7' +
                         '3497aeffc3b3e6358ab3d3d1aa2aa0ee';
  assert.strictEqual(
    pbkdf2Sync('password', 'salt', 2, 32, 'sha256-192').toString('hex'),
    pbkdf2Expected,
  );
  pbkdf2(
    'password',
    'salt',
    2,
    32,
    'sha256-192',
    common.mustSucceed((result) => {
      assert.strictEqual(result.toString('hex'), pbkdf2Expected);
    }),
  );

  const { privateKey: ecPrivateKey, publicKey: ecPublicKey } =
    generateKeyPairSync('ec', { namedCurve: 'prime256v1' });
  const signature = sign('sha256-192', operationInput, ecPrivateKey);
  assert(verify('sha256-192', operationInput, ecPublicKey, signature));

  const streamingSignature = createSign('sha256-192')
    .update(operationInput)
    .sign(ecPrivateKey);
  const verifier = createVerify('sha256-192');
  verifier.update(operationInput);
  assert(verifier.verify(ecPublicKey, streamingSignature));

  sign(
    'sha256-192',
    operationInput,
    ecPrivateKey,
    common.mustSucceed((asyncSignature) => {
      verify(
        'sha256-192',
        operationInput,
        ecPublicKey,
        asyncSignature,
        common.mustSucceed((result) => assert(result)),
      );
    }),
  );

  const { privateKey: rsaPrivateKey, publicKey: rsaPublicKey } =
    generateKeyPairSync('rsa', { modulusLength: 2048 });
  const plaintext = Buffer.from('provider digest');

  assert.throws(
    () => sign('sha256-192', plaintext, rsaPrivateKey),
    { code: 'ERR_OSSL_DIGEST_NOT_ALLOWED' },
  );
  assert.deepStrictEqual(
    privateDecrypt(
      { key: rsaPrivateKey, oaepHash: 'sha256-192' },
      publicEncrypt(
        { key: rsaPublicKey, oaepHash: 'sha256-192' },
        plaintext,
      ),
    ),
    plaintext,
  );

  const pssOptions = [
    {
      hashAlgorithm: 'sha256-192',
      modulusLength: 2048,
    },
    {
      hashAlgorithm: 'sha256',
      mgf1HashAlgorithm: 'sha256-192',
      modulusLength: 2048,
    },
  ];
  const keyGenerationFailed = { message: 'Key generation job failed' };

  for (const options of pssOptions) {
    assert.throws(
      () => generateKeyPairSync('rsa-pss', options),
      keyGenerationFailed,
    );
    generateKeyPair(
      'rsa-pss',
      options,
      common.mustCall((err, publicKey, privateKey) => {
        assert.strictEqual(err?.message, keyGenerationFailed.message);
        assert.strictEqual(publicKey, undefined);
        assert.strictEqual(privateKey, undefined);
      }),
    );
  }
}
