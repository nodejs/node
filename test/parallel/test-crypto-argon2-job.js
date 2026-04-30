// Flags: --expose-internals --no-warnings
'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const { hasOpenSSL } = require('../common/crypto');

if (!hasOpenSSL(3, 2))
  common.skip('requires OpenSSL >= 3.2');

// Exercises the native Argon2 job directly via internalBinding, bypassing
// the JS validators, to ensure that if invalid parameters ever reach the
// native layer they produce a clean error from the KDF rather than crashing,
// in both sync and async modes.

const assert = require('node:assert');
const { internalBinding } = require('internal/test/binding');
const {
  Argon2Job,
  kCryptoJobAsync,
  kCryptoJobSync,
  kTypeArgon2id,
} = internalBinding('crypto');

const pass = Buffer.from('password');
const salt = Buffer.alloc(16, 0x02);
const empty = Buffer.alloc(0);

// Parameters that OpenSSL's Argon2 KDF rejects.
const badParams = [
  { lanes: 0, keylen: 32, memcost: 16, iter: 1 },  // lanes < 1
  { lanes: 1, keylen: 32, memcost: 0, iter: 1 },   // memcost == 0
  { lanes: 1, keylen: 32, memcost: 16, iter: 0 },  // iter == 0
];

for (const { lanes, keylen, memcost, iter } of badParams) {
  {
    const job = new Argon2Job(
      kCryptoJobSync, pass, salt, lanes, keylen, memcost, iter,
      empty, empty, kTypeArgon2id);
    const { 0: err, 1: result } = job.run();
    assert.ok(err);
    assert.match(err.message, /Argon2 derivation failed/);
    assert.strictEqual(result, undefined);
  }

  {
    const job = new Argon2Job(
      kCryptoJobAsync, pass, salt, lanes, keylen, memcost, iter,
      empty, empty, kTypeArgon2id);
    job.ondone = common.mustCall((err, result) => {
      assert.ok(err);
      assert.match(err.message, /Argon2 derivation failed/);
      assert.strictEqual(result, undefined);
    });
    job.run();
  }
}
