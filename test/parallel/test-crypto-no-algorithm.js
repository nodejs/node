'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const { hasOpenSSL3 } = require('../common/crypto');

if (!hasOpenSSL3)
  common.skip('this test requires OpenSSL 3.x');

const assert = require('node:assert/strict');
const crypto = require('node:crypto');
const { isMainThread } = require('worker_threads');

if (isMainThread) {
  // TODO(richardlau): Decide if `crypto.setFips` should error if the
  // provider named "fips" is not available.
  crypto.setFips(1);
  crypto.randomBytes(20, common.mustCall((err) => {
    // crypto.randomBytes should either succeed or fail but not hang.
    if (err) {
      assert.match(err.message, /digital envelope routines::unsupported/);
      const expected = /random number generator::unable to fetch drbg/;
      assert(err.opensslErrorStack.some((msg) => expected.test(msg)),
             `did not find ${expected} in ${err.opensslErrorStack}`);
    }
  }));

  const derivations = [
    ['HKDF', () => crypto.hkdfSync('sha256', Buffer.alloc(32), Buffer.alloc(8),
                                   Buffer.alloc(0), 32)],
    ['PBKDF2', () => crypto.pbkdf2Sync('secret', Buffer.alloc(16), 1000, 32,
                                       'sha256')],
  ];
  for (const { 0: name, 1: derive } of derivations) {
    try {
      derive();
    } catch (err) {
      assert.match(err.message, /derivation failed/);
      assert.strictEqual(err.code, 'ERR_OSSL_EVP_UNSUPPORTED', `${name}: ${err.code}`);
      const expected = /digital envelope routines::unsupported/;
      assert(err.opensslErrorStack.some((msg) => expected.test(msg)),
             `${name}: did not find ${expected} in ${err.opensslErrorStack}`);
    }
  }
}

{
  // Startup test. Should not hang.
  const fixtures = require('../common/fixtures');
  const { spawnSync } = require('node:child_process');
  const baseConf = fixtures.path('openssl3-conf', 'base_only.cnf');
  const cp = spawnSync(process.execPath,
                       [ `--openssl-config=${baseConf}`, '-p', '"hello"' ],
                       { encoding: 'utf8' });
  assert(common.nodeProcessAborted(cp.status, cp.signal),
         `process did not abort, code:${cp.status} signal:${cp.signal}`);
}
