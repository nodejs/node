'use strict';
const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const fixtures = require('../common/fixtures');
const tmpdir = require('../common/tmpdir');

const assert = require('assert');
const child_process = require('child_process');
const path = require('path');
const tls = require('tls');

const options = {
  key: fixtures.readKey('agent1-key.pem'),
  cert: fixtures.readKey('agent1-cert.pem'),
  maxVersion: 'TLSv1.3',
  minVersion: 'TLSv1.3',
};

tmpdir.refresh();
const sessionPath = path.join(tmpdir.path, 'sess.pem');

const server = tls
  .createServer(options, (s) => s.end('ok'))
  .listen(0, '127.0.0.1', common.mustCall(go));

function go() {
  const { address, port } = server.address();
  const args = [
    's_client',
    '-tls1_3',
    '-sess_out', sessionPath,
    '-connect', `${address}:${port}`,
  ];
  const options = { stdio: ['inherit', 'pipe', 'inherit'] };
  const proc = child_process.spawn(common.opensslCli, args, options);
  let stdout = '';
  proc.stdout.setEncoding('utf8');
  proc.stdout.on('data', (s) => stdout += s);
  proc.stdout.pipe(process.stdout);  // For debugging.
  proc.on('exit', common.mustCall((exitCode, signalCode) => {
    assert.strictEqual(exitCode, 0);
    assert(stdout.includes('Early data was not sent'));
    assert(stdout.includes('New, TLSv1.3, Cipher is TLS_'));
    next();
  }));
}

function next() {
  const { address, port } = server.address();
  const args = [
    's_client',
    '-tls1_3',
    '-sess_in', sessionPath,
    '-connect', `${address}:${port}`,
    '-early_data', fixtures.path('x.txt'),
  ];
  const options = { stdio: ['inherit', 'pipe', 'inherit'] };
  const proc = child_process.spawn(common.opensslCli, args, options);
  let stdout = '';
  proc.stdout.setEncoding('utf8');
  proc.stdout.on('data', (s) => stdout += s);
  proc.stdout.pipe(process.stdout);  // For debugging.
  proc.on('exit', common.mustCall((exitCode, signalCode) => {
    assert.strictEqual(exitCode, 0);
    assert(stdout.includes('Early data was not sent'));
    assert(stdout.includes('Reused, TLSv1.3, Cipher is TLS_'));
    assert(!stdout.includes('Early data was accepted'));
    assert(!stdout.includes('Early data was rejected'));
    assert(!stdout.includes('New, TLSv1.3, Cipher is TLS_'));
    server.close();
  }));
}
