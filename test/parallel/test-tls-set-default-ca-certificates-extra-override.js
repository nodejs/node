'use strict';

// This tests that tls.setDefaultCACertificates() properly overrides certificates
// added through NODE_EXTRA_CA_CERTS environment variable.

const common = require('../common');
if (!common.hasCrypto) common.skip('missing crypto');

const fixtures = require('../common/fixtures');
const { spawnSyncAndExitWithoutError } = require('../common/child_process');

spawnSyncAndExitWithoutError(process.execPath, [
  fixtures.path('tls-extra-ca-override.js'),
], {
  env: {
    ...process.env,
    NODE_EXTRA_CA_CERTS: fixtures.path('keys', 'fake-startcom-root-cert.pem')
  }
});
