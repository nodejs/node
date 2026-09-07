'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const { hasOpenSSL3 } = require('../common/crypto');
const fixtures = require('../common/fixtures');
const tmpdir = require('../common/tmpdir');
const { buildSnapshot, runWithSnapshot } = require('../common/snapshot');

if (!hasOpenSSL3)
  common.skip('this test requires OpenSSL 3.x');

const entry = fixtures.path('snapshot', 'crypto-provider-cipher-cache.js');
const buildEnv = {
  OPENSSL_CONF: fixtures.path(
    'openssl3-conf', 'legacy_provider_enabled.cnf'),
};
const runEnv = {
  OPENSSL_CONF: fixtures.path('openssl3-conf', 'default_only.cnf'),
};

tmpdir.refresh();
buildSnapshot(entry, buildEnv);
const { stdout } = runWithSnapshot(undefined, runEnv);
assert.match(stdout, /provider crypto caches snapshot: ok/);
