'use strict';

// This tests that tls.getCACertificates() returns the system
// certificates correctly when --use-system-ca is disabled.

const common = require('../common');
if (!common.hasCrypto) common.skip('missing crypto');

const assert = require('assert');
const { spawnSyncAndAssert } = require('../common/child_process');
const fixtures = require('../common/fixtures');
const tls = require('tls');

const certs = tls.getCACertificates('system');
if (certs.length === 0) {
  common.skip('No trusted system certificates installed. Skip.');
}

spawnSyncAndAssert(process.execPath, [
  '--no-use-system-ca',
  fixtures.path('tls-get-ca-certificates.js'),
], {
  env: {
    ...process.env,
    CA_TYPE: 'system',
  }
}, {
  stdout(output) {
    const parsed = JSON.parse(output);
    assert.deepStrictEqual(parsed, certs);
  }
});
