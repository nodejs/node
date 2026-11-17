'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const { isBoringSSL } = require('../common/crypto');
if (isBoringSSL)
  common.skip('this test requires OpenSSL provider support');

const { isMainThread } = require('worker_threads');
if (!isMainThread)
  common.skip('crypto.setFips() is not supported in workers');

const assert = require('assert');
const crypto = require('crypto');

// createHmac(), createSign(), createVerify() and the synchronous one-shot
// functions cache digests and HMAC contexts per Environment. Changing the FIPS
// state must invalidate those caches without breaking objects that already
// hold a context.

const key = Buffer.from('Jefe');
const data = 'what do ya want for nothing?';
// RFC 4231, test case 2.
const expectedSha256 =
  '5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843';

const { privateKey, publicKey } =
  crypto.generateKeyPairSync('rsa', { modulusLength: 2048 });

const operations = {
  createHmac: (hash) => crypto.createHmac(hash, key).update(data).digest('hex'),
  createSign: (hash) =>
    crypto.createSign(hash).update(data).sign(privateKey, 'hex'),
  createVerify: (hash) => {
    const signature = crypto.sign(hash, Buffer.from(data), privateKey);
    return crypto.createVerify(hash).update(data).verify(publicKey, signature);
  },
  sign: (hash) => crypto.sign(hash, Buffer.from(data), privateKey).toString('hex'),
  verify: (hash) => {
    const signature = crypto.sign(hash, Buffer.from(data), privateKey);
    return crypto.verify(hash, Buffer.from(data), publicKey, signature);
  },
  hkdfSync: (hash) =>
    Buffer.from(crypto.hkdfSync(hash, key, 'salt', 'info', 32)).toString('hex'),
  pbkdf2Sync: (hash) =>
    crypto.pbkdf2Sync(key, 'salt', 1, 32, hash).toString('hex'),
};

function runAll(hash) {
  const results = {};
  for (const [name, operation] of Object.entries(operations))
    results[name] = operation(hash);
  return results;
}

const originalFips = crypto.getFips();
try {
  crypto.setFips(false);
} catch (err) {
  if (err.code !== 'ERR_CRYPTO_FIPS_FORCED') throw err;
  common.skip('FIPS mode cannot be disabled');
}

try {
  // Warm the caches with a digest that FIPS mode never allows.
  const md5 = runAll('md5');
  assert.strictEqual(md5.createVerify, true);
  assert.strictEqual(md5.verify, true);
  assert.strictEqual(runAll('sha256').createHmac, expectedSha256);

  const live = crypto.createHmac('sha256', key).update(data.slice(0, 10));

  crypto.setFips(true);

  // New operations follow the current policy instead of reusing cached
  // implementations.
  for (const [name, operation] of Object.entries(operations))
    assert.throws(() => operation('md5'), Error, name);

  let fipsSha256;
  try {
    fipsSha256 = crypto.createHash('sha256').update(data).digest('hex');
  } catch {
    // No FIPS provider is available, so nothing can use SHA-256 either.
  }
  if (fipsSha256 === undefined) {
    assert.throws(() => operations.createHmac('sha256'), Error);
  } else {
    assert.strictEqual(operations.createHmac('sha256'), expectedSha256);
  }

  // An HMAC that already has its context is not affected.
  assert.strictEqual(live.update(data.slice(10)).digest('hex'),
                     expectedSha256);

  crypto.setFips(false);
  assert.deepStrictEqual(runAll('md5'), md5);
  assert.strictEqual(operations.createHmac('sha256'), expectedSha256);
} finally {
  crypto.setFips(originalFips);
}
