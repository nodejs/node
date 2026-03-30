'use strict';
// This tests that when --use-openssl-ca is specified, no off-thread cert loading happens.

const common = require('../common');
if (!common.hasCrypto) {
  common.skip('missing crypto');
}
const { spawnSyncAndAssert } = require('../common/child_process');
const fixtures = require('../common/fixtures');
const assert = require('assert');

spawnSyncAndAssert(
  process.execPath,
  [ '--use-openssl-ca', fixtures.path('list-certs.js') ],
  {
    env: {
      ...process.env,
      NODE_DEBUG_NATIVE: 'crypto',
      NODE_EXTRA_CA_CERTS: fixtures.path('keys', 'fake-startcom-root-cert.pem'),
      CERTS_TYPE: 'default',
    }
  },
  {
    stderr(output) {
      assert.doesNotMatch(
        output,
        /Started loading bundled root certificates off-thread/
      );
      assert.doesNotMatch(
        output,
        /Started loading extra root certificates off-thread/
      );
      assert.doesNotMatch(
        output,
        /Started loading system root certificates off-thread/
      );
      return true;
    }
  }
);
