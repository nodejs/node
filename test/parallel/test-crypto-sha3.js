'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const {
  constants,
  createHash,
  createHmac,
  createPrivateKey,
  createPublicKey,
  createSign,
  createVerify,
  getHashes,
  hash,
  hkdf,
  hkdfSync,
  pbkdf2,
  pbkdf2Sync,
  sign,
  verify,
} = require('crypto');
const rsaPss = require('../fixtures/crypto/rsa_pss');

const vectors = [
  {
    name: 'sha3-256',
    digest: '3a985da74fe225b2045c172d6bd390bd855f086e3e9d525b46bfe24511431532',
    hmac: '961fcf59ed455732e405e74f5dc78beb7aa41ad315af5e2b2a0dcf8cef9887e0',
    hkdf: '38c2d1e7b3933f7df51e35f3eeaa0a0138b1b812e2bf57810ad4e2300e2c4a81',
    pbkdf2: 'db7d83f5e7ebc6b0ea83e9b6c57a27e254e96cfef315278e9ed96b10483a7ad0',
  },
  {
    name: 'sha3-384',
    digest: 'ec01498288516fc926459f58e2c6ad8df9b473cb0fc08c2596da7cf0e49be4b29' +
            '8d88cea927ac7f539f1edf228376d25',
    hmac: 'f1d3898bb4cd7b7904fe4e7cecb84d3225b3add0265842ec7386867d662ee3364' +
          '8a90e98a5c673b2266eb1077fb91ed6',
    hkdf: '3ca21ccc4e742ec6a365ee5857db1ab5a0a8843196467d7a0b26c724e542f6f5',
    pbkdf2: '7f049e44bfddb1b591d1ae2beaa1f3be54d683063bd25759b412a86012186c16',
  },
  {
    name: 'sha3-512',
    digest: 'b751850b1a57168a5693cd924b6b096e08f621827444f70d884f5d0240d2712e1' +
            '0e116e9192af3c91a7ec57647e3934057340b4cf408d5a56592f8274eec53f0',
    hmac: '5576c01b021fc928ae1bc83a6ddb8db5f9a201681e940ecf1f7d5c8a6ec80ac34' +
          '614d4a68c9581957f69dc8a8c0fbf413984b0f938db3e8212c0744a1a9c5915',
    hkdf: 'cd54f848a5f6ffb9e730d4bfbdff30d69a4b86316afdf5ba61b4fe95bd022037',
    pbkdf2: '2b5150c50e0bf48dcb5b32205ad81e8c42d97f8f8dff57dcc365a38e857a05c7',
  },
];

const hashes = getHashes();
for (const vector of vectors) {
  assert(hashes.includes(vector.name));
  for (const name of [vector.name, vector.name.toUpperCase(),
                      vector.name.replace('sha', 'sHa')]) {
    assert.strictEqual(hash(name, 'abc'), vector.digest);

    const original = createHash(name).update('a');
    const copy = original.copy();
    assert.strictEqual(original.update('bc').digest('hex'), vector.digest);
    assert.strictEqual(copy.update('b').copy().update('c').digest('hex'), vector.digest);

    // Exceed every SHA-3 block size so HMAC also hashes the key.
    assert.strictEqual(createHmac(name, Buffer.alloc(200, 0xaa))
      .update('abc').digest('hex'), vector.hmac);

    assert.strictEqual(Buffer.from(hkdfSync(name, 'passwordpassword', 'saltsaltsaltsalt', 'info', 32))
      .toString('hex'), vector.hkdf);
    hkdf(name, 'passwordpassword', 'saltsaltsaltsalt', 'info', 32, common.mustSucceed((result) => {
      assert.strictEqual(Buffer.from(result).toString('hex'), vector.hkdf);
    }));

    assert.strictEqual(pbkdf2Sync('passwordpassword', 'saltsaltsaltsalt', 1000, 32, name)
      .toString('hex'), vector.pbkdf2);
    pbkdf2('passwordpassword', 'saltsaltsaltsalt', 1000, 32, name, common.mustSucceed((result) => {
      assert.strictEqual(result.toString('hex'), vector.pbkdf2);
    }));
  }
}

// Reuse independent RSA-PSS signature vectors to check SHA-3 in both the
// one-shot and streaming APIs, including zero-length and digest-length salts.
for (const vector of rsaPss()) {
  if (!vector.hash.startsWith('SHA3-')) continue;
  const privateKey = createPrivateKey({
    key: vector.privateKeyBuffer, format: 'der', type: 'pkcs8',
  });
  const publicKey = createPublicKey({
    key: vector.publicKeyBuffer, format: 'der', type: 'spki',
  });
  const options = {
    padding: constants.RSA_PKCS1_PSS_PADDING,
    saltLength: vector.algorithm.saltLength,
  };
  const signingKey = { key: privateKey, ...options };
  const verificationKey = { key: publicKey, ...options };

  assert(verify(vector.hash, vector.plaintext, verificationKey,
                vector.signature));
  assert(createVerify(vector.hash).update(vector.plaintext)
    .verify(verificationKey, vector.signature));

  for (const signature of [
    sign(vector.hash, vector.plaintext, signingKey),
    createSign(vector.hash).update(vector.plaintext).sign(signingKey),
  ]) {
    assert(verify(vector.hash, vector.plaintext, verificationKey, signature));
    assert(createVerify(vector.hash).update(vector.plaintext)
      .verify(verificationKey, signature));
    if (options.saltLength === 0)
      assert.deepStrictEqual(signature, vector.signature);
  }
}
