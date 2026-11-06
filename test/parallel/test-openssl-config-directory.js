'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
if (process.features.openssl_is_boringssl)
  common.skip('BoringSSL does not use OpenSSL configuration files');
if (!require('../common/crypto').hasOpenSSL3)
  common.skip('requires OpenSSL 3');

const assert = require('node:assert');
const { spawnSync } = require('node:child_process');
const { writeFileSync } = require('node:fs');
const { join } = require('node:path');
const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

function run(conf, args = []) {
  return spawnSync(process.execPath, [...args, '-p', '42'], {
    encoding: 'utf8',
    env: { ...process.env, OPENSSL_CONF: conf },
  });
}

const directory = run(tmpdir.path);
assert.ifError(directory.error);
assert.strictEqual(directory.status, 0, directory.stderr);
assert.strictEqual(directory.stdout.trim(), '42');
assert.match(directory.stderr, /OPENSSL_CONF path is a directory; ignoring:/);

const validConfig = join(tmpdir.path, 'valid.cnf');
writeFileSync(validConfig, '');
const overridden = run(tmpdir.path, [`--openssl-config=${validConfig}`]);
assert.ifError(overridden.error);
assert.strictEqual(overridden.status, 0, overridden.stderr);
assert.strictEqual(overridden.stdout.trim(), '42');
assert.strictEqual(overridden.stderr, '');

// Ignoring a directory must not turn other configuration errors into warnings.
const invalidConfig = join(tmpdir.path, 'invalid.cnf');
writeFileSync(invalidConfig, '[unterminated\n');
for (const result of [
  run(invalidConfig),
  run(tmpdir.path, [`--openssl-config=${invalidConfig}`]),
]) {
  assert.ifError(result.error);
  assert.strictEqual(result.signal, null);
  assert.notStrictEqual(result.status, 0);
  assert.strictEqual(result.stdout, '');
  assert.match(result.stderr, /OpenSSL configuration error/);
}
