'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const { hasOpenSSL3 } = require('../common/crypto');
if (!hasOpenSSL3 || process.features.openssl_is_boringssl)
  common.skip('this test requires OpenSSL 3 EVP_MAC support');

const assert = require('node:assert');
const { getMacs } = require('node:crypto');
const fixtures = require('../common/fixtures');
const tmpdir = require('../common/tmpdir');
const { buildSnapshot, runWithSnapshot } = require('../common/snapshot');

if (!getMacs().includes('poly1305'))
  common.skip('Poly1305 is not supported');

const entry = fixtures.path('snapshot', 'crypto-provider-mac-cache.js');
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
assert.match(stdout, /provider MAC cache snapshot: ok/);
