'use strict';

// This test verifies that when --use-bundled-ca is used (default to true in default builds),
// the loading of extra and bundled root certificates happens off the main thread.

const common = require('../common');
if (!common.hasCrypto) {
  common.skip('missing crypto');
}
const tmpdir = require('../common/tmpdir');
const { spawnSyncAndAssert } = require('../common/child_process');
const fixtures = require('../common/fixtures');
const assert = require('assert');
const { writeCerts } = require('../common/tls');
const tls = require('tls');

tmpdir.refresh();
writeCerts([
  // Check that the extra cert is loaded.
  fixtures.readKey('fake-startcom-root-cert.pem'),
  // Check that the bundled certs are loaded.
  ...tls.getCACertificates('bundled'),
], tmpdir.resolve('check-cert.pem'));

spawnSyncAndAssert(
  process.execPath,
  [ '--no-use-system-ca', '--use-bundled-ca', fixtures.path('list-certs.js') ],
  {
    env: {
      ...process.env,
      NODE_DEBUG_NATIVE: 'crypto',
      NODE_EXTRA_CA_CERTS: fixtures.path('keys', 'fake-startcom-root-cert.pem'),
      EXPECTED_CERTS_PATH: tmpdir.resolve('check-cert.pem'),
      CERTS_TYPE: 'default',
    }
  },
  {
    stderr(output) {
      assert.match(
        output,
        /Started loading bundled root certificates off-thread/
      );
      assert.match(
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
