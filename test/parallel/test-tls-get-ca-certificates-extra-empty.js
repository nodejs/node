'use strict';
// This tests that tls.getCACertificates('extra') returns an empty
// array if NODE_EXTRA_CA_CERTS is empty.

const common = require('../common');
if (!common.hasCrypto) common.skip('missing crypto');

const assert = require('assert');
const { spawnSyncAndAssert } = require('../common/child_process');
const fixtures = require('../common/fixtures');

// If NODE_EXTRA_CA_CERTS is not set, it should be an empty array.
spawnSyncAndAssert(process.execPath, [fixtures.path('tls-get-ca-certificates.js')], {
  env: {
    ...process.env,
    NODE_EXTRA_CA_CERTS: undefined,
    CA_TYPE: 'extra',
  }
}, {
  stdout(output) {
    const parsed = JSON.parse(output);
    assert.deepStrictEqual(parsed, []);
  }
});
