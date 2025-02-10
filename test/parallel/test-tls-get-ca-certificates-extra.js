'use strict';
// This tests that tls.getCACertificates('extra') returns the extra
// certificates from NODE_EXTRA_CA_CERTS correctly.

const common = require('../common');
if (!common.hasCrypto) common.skip('missing crypto');

const assert = require('assert');
const { spawnSyncAndAssert } = require('../common/child_process');
const fixtures = require('../common/fixtures');

// If NODE_EXTRA_CA_CERTS is set, it should contain a list of certificates.
spawnSyncAndAssert(process.execPath, [fixtures.path('tls-get-ca-certificates.js')], {
  env: {
    ...process.env,
    NODE_EXTRA_CA_CERTS: fixtures.path('keys', 'ca1-cert.pem'),
    CA_TYPE: 'extra',
  }
}, {
  stdout(output) {
    const parsed = JSON.parse(output);
    assert.deepStrictEqual(parsed, [fixtures.readKey('ca1-cert.pem', 'utf8')]);
  }
});
