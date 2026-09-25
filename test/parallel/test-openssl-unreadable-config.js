'use strict';

// A default OpenSSL configuration file that cannot be read is fatal, and an
// empty OPENSSL_CONF is the documented way past it.
// Refs: https://github.com/nodejs/node/issues/62230

const common = require('../common');
const assert = require('node:assert');
const { spawnSync } = require('node:child_process');

if (!common.hasCrypto)
  common.skip('missing crypto');
if (!common.isLinux)
  common.skip('linux only');
if (process.config.variables.node_shared_openssl)
  common.skip('shared openssl may read a different configuration file');

// Replace /etc/ssl with an empty tmpfs in a private mount namespace, where
// openssl.cnf is a symlink loop: opening it then fails with ELOOP instead of
// ENOENT, which OpenSSL ignores on its own. The namespace goes away with the
// process, so the host /etc/ssl is left alone.
const setup = 'mount -t tmpfs tmpfs /etc/ssl && ln -s openssl.cnf /etc/ssl/openssl.cnf';

if (spawnSync('unshare', ['-Urm', 'sh', '-c', setup]).status !== 0)
  common.skip('cannot set up an unprivileged user and mount namespace');

function run(env) {
  return spawnSync(
    'unshare',
    ['-Urm', 'sh', '-c', `${setup} && exec "$0" -p 42`, process.execPath],
    { encoding: 'utf8', env: { ...process.env, ...env } });
}

const failed = run({});
assert.notStrictEqual(failed.status, 0);
assert.match(failed.stderr, /OpenSSL configuration error/);

const skipped = run({ OPENSSL_CONF: '' });
assert.strictEqual(skipped.status, 0);
assert.strictEqual(skipped.stdout.trim(), '42');
