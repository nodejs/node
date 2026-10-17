'use strict';

// NODE_EXTRA_CA_CERTS is honored when it is set in an env file.
// Refs: https://github.com/nodejs/node/issues/51426

const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const fs = require('fs');
const tls = require('tls');
const { fork, spawnSync } = require('child_process');
const fixtures = require('../common/fixtures');
const tmpdir = require('../common/tmpdir');

if (process.env.CHILD) {
  const client = tls.connect({
    port: process.env.PORT,
    checkServerIdentity: common.mustCall(),
  }, common.mustCall(() => {
    client.end('hi');
  }));
  return;
}

tmpdir.refresh();
const envFile = tmpdir.resolve('extra-ca.env');
fs.writeFileSync(envFile,
                 `NODE_EXTRA_CA_CERTS=${fixtures.path('keys', 'ca1-cert.pem')}\n`);

const env = { ...process.env };
delete env.NODE_EXTRA_CA_CERTS;

function extraCertificates(args, extraEnv) {
  const child = spawnSync(process.execPath, [
    ...args,
    '-p',
    'JSON.stringify(require("tls").getCACertificates("extra"))',
  ], { env: { ...env, ...extraEnv }, encoding: 'utf8' });
  assert.strictEqual(child.status, 0, child.stderr);
  return JSON.parse(child.stdout);
}

const ca1 = [fixtures.readKey('ca1-cert.pem', 'utf8').trim()];
const ca2 = [fixtures.readKey('ca2-cert.pem', 'utf8').trim()];
const trim = (certs) => certs.map((cert) => cert.trim());

assert.deepStrictEqual(extraCertificates([]), []);
assert.deepStrictEqual(trim(extraCertificates([`--env-file=${envFile}`])), ca1);
assert.deepStrictEqual(
  trim(extraCertificates([`--env-file-if-exists=${envFile}`])), ca1);

if (common.isWindows) {
  // Environment variable names are case-insensitive on Windows.
  const lowerCaseFile = tmpdir.resolve('extra-ca-lower-case.env');
  fs.writeFileSync(lowerCaseFile,
                   `node_extra_ca_certs=${fixtures.path('keys', 'ca1-cert.pem')}\n`);
  assert.deepStrictEqual(
    trim(extraCertificates([`--env-file=${lowerCaseFile}`])), ca1);
}

// The environment takes precedence over the env file.
assert.deepStrictEqual(
  trim(extraCertificates([`--env-file=${envFile}`], {
    NODE_EXTRA_CA_CERTS: fixtures.path('keys', 'ca2-cert.pem'),
  })), ca2);

// The certificate is used for TLS peer validation.
const server = tls.createServer({
  key: fixtures.readKey('agent1-key.pem'),
  cert: fixtures.readKey('agent1-cert.pem'),
}, common.mustCall((socket) => {
  socket.end('bye');
  server.close();
})).listen(0, common.mustCall(() => {
  fork(__filename, {
    env: { ...env, CHILD: 'yes', PORT: server.address().port },
    execArgv: [`--env-file=${envFile}`],
  }).on('exit', common.mustCall((status) => {
    assert.strictEqual(status, 0);
  }));
}));
