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
  { lanes: 0, keylen: 32, memcost: 16, iter: 1,
    reason: /invalid thread pool size/ },
  { lanes: 1, keylen: 32, memcost: 0, iter: 1,
    reason: /invalid memory size/ },
  { lanes: 1, keylen: 32, memcost: 16, iter: 0,
    reason: /invalid iteration count/ },
];

function assertError(err, { reason }) {
  assert.ok(err);
  const details = [err.message, ...(err.opensslErrorStack ?? [])];
  assert.ok(details.some((msg) => reason.test(msg)),
            `did not find ${reason} in ${details}`);
}

for (const params of badParams) {
  const { lanes, keylen, memcost, iter } = params;

  {
    const job = new Argon2Job(
      kCryptoJobSync, pass, salt, lanes, keylen, memcost, iter,
      empty, empty, kTypeArgon2id);
    const { 0: err, 1: result } = job.run();
    assertError(err, params);
    assert.strictEqual(result, undefined);
  }

  {
    const job = new Argon2Job(
      kCryptoJobAsync, pass, salt, lanes, keylen, memcost, iter,
      empty, empty, kTypeArgon2id);
    job.ondone = common.mustCall((err, result) => {
      assertError(err, params);
      assert.strictEqual(result, undefined);
    });
    job.run();
  }
}
