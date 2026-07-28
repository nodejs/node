'use strict';
// This tests that NODE_USE_SYSTEM_CA environment variable works the same
// as --use-system-ca flag by comparing certificate counts.

const common = require('../common');
if (!common.hasCrypto) common.skip('missing crypto');

const tls = require('tls');
const { spawnSyncAndExitWithoutError } = require('../common/child_process');

const systemCerts = tls.getCACertificates('system');
if (systemCerts.length === 0) {
  common.skip('no system certificates available');
}

const { child: { stdout: expectedLength } } = spawnSyncAndExitWithoutError(process.execPath, [
  '--use-system-ca',
  '-p',
  `tls.getCACertificates('default').length`,
], {
  env: { ...process.env, NODE_USE_SYSTEM_CA: '0' },
});

spawnSyncAndExitWithoutError(process.execPath, [
  '-p',
  `assert.strictEqual(tls.getCACertificates('default').length, ${expectedLength.toString()})`,
], {
  env: { ...process.env, NODE_USE_SYSTEM_CA: '1' },
});
