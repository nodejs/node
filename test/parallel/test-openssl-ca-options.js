'use strict';
// This test checks the usage of --use-bundled-ca and --use-openssl-ca arguments
// to verify that both are not used at the same time.
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const os = require('os');
const childProcess = require('child_process');
const result = childProcess.spawnSync(process.execPath, [
  '--use-bundled-ca',
  '--use-openssl-ca',
  '-p', 'process.version'],
                                      {encoding: 'utf8'});

assert.strictEqual(result.stderr, `${process.execPath
}: either --use-openssl-ca or --use-bundled-ca can be used, not both${os.EOL}`
);
assert.strictEqual(result.status, 9);

const useBundledCA = childProcess.spawnSync(process.execPath, [
  '--use-bundled-ca',
  '-p', 'process.version']);
assert.strictEqual(useBundledCA.status, 0);

const useOpenSSLCA = childProcess.spawnSync(process.execPath, [
  '--use-openssl-ca',
  '-p', 'process.version']);
assert.strictEqual(useOpenSSLCA.status, 0);
