'use strict';
// Flags: --expose-internals

const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const tls = require('tls');
const fixtures = require('../common/fixtures');
const { internalBinding } = require('internal/test/binding');
const binding = internalBinding('crypto');

const { fork } = require('child_process');

// This test ensures that extra certificates are loaded at startup.
if (process.argv[2] !== 'child') {
  // Parent
  const NODE_EXTRA_CA_CERTS = fixtures.path('keys', 'ca1-cert.pem');
  const extendsEnv = (obj) => ({ ...process.env, ...obj });

  [
    extendsEnv({ CHILD_USE_EXTRA_CA_CERTS: 'yes', NODE_EXTRA_CA_CERTS }),
    extendsEnv({ CHILD_USE_EXTRA_CA_CERTS: 'no' }),
  ].forEach((processEnv) => {
    fork(__filename, ['child'], { env: processEnv })
    .on('exit', common.mustCall((status) => {
      // Client did not succeed in connecting
      assert.strictEqual(status, 0);
    }));
  });
} else if (process.env.CHILD_USE_EXTRA_CA_CERTS === 'yes') {
  // Child with extra certificates loaded at startup.
  assert.strictEqual(binding.isExtraRootCertsFileLoaded(), true);
} else {
  // Child without extra certificates.
  assert.strictEqual(binding.isExtraRootCertsFileLoaded(), false);
  tls.createServer({});
  assert.strictEqual(binding.isExtraRootCertsFileLoaded(), false);
}
