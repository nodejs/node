// Flags: --expose-internals --no-warnings
'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const { hasOpenSSL } = require('../common/crypto');

if (!hasOpenSSL(3, 2))
  common.skip('requires OpenSSL >= 3.2');

// Regression test for https://github.com/nodejs/node/issues/62861.
// `AdditionalConfig` used to invoke the full Argon2 KDF synchronously inside
// `new Argon2Job(...)` for the purpose of getting a parameter validation
// error.
//
// The fix removes the redundant KDF call from the constructor. This test
// asserts that the constructor returns in a small fraction of the time a
// full sync job takes. Pre-fix the constructor ran the KDF, and job.run()
// then ran it a second time in DeriveBits; post-fix the constructor does
// no KDF work at all.

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

// Tuned so a single-threaded Argon2id derivation is expensive enough that
// scheduler/GC noise is negligible compared to it.
const kdfArgs = [
  pass,
  salt,
  1, // lanes
  32, // keylen
  65536, // memcost
  20, // iter
  empty, // secret
  empty, // associatedData
  kTypeArgon2id,
];

// For each mode, measure the constructor and the derivation separately and
// assert that the constructor is a small fraction of the derivation. Pre-fix
// the constructor ran a full KDF, so ctorNs was comparable to runNs. Post-fix
// the constructor does no KDF work and must be orders of magnitude faster.
//
// For async mode the derivation happens on the thread pool, so runNs is
// measured from the start of run() until ondone fires.

// Sync: run() derives on the main thread and returns when done.
{
  const ctorStart = process.hrtime.bigint();
  const job = new Argon2Job(kCryptoJobSync, ...kdfArgs);
  const ctorNs = process.hrtime.bigint() - ctorStart;
  const runStart = process.hrtime.bigint();
  const { 0: err } = job.run();
  const runNs = process.hrtime.bigint() - runStart;
  assert.strictEqual(err, undefined);
  assert.ok(ctorNs * 10n < runNs);
}

// Async: run() dispatches to the thread pool; measure until ondone fires.
{
  const ctorStart = process.hrtime.bigint();
  const job = new Argon2Job(kCryptoJobAsync, ...kdfArgs);
  const ctorNs = process.hrtime.bigint() - ctorStart;
  const runStart = process.hrtime.bigint();
  job.ondone = common.mustSucceed(() => {
    const runNs = process.hrtime.bigint() - runStart;
    assert.ok(ctorNs * 10n < runNs);
  });
  job.run();
}
