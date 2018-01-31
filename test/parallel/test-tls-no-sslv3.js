'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

if (common.opensslCli === false)
  common.skip('node compiled without OpenSSL CLI.');

const assert = require('assert');
const tls = require('tls');
const spawn = require('child_process').spawn;
const fixtures = require('../common/fixtures');

const cert = fixtures.readSync('test_cert.pem');
const key = fixtures.readSync('test_key.pem');
const server = tls.createServer({ cert, key }, common.mustNotCall());
const errors = [];
let stderr = '';

server.listen(0, '127.0.0.1', function() {
  const address = `${this.address().address}:${this.address().port}`;
  const args = ['s_client',
                '-ssl3',
                '-connect', address];

  // for the performance and stability issue in s_client on Windows
  if (common.isWindows)
    args.push('-no_rand_screen');

  const client = spawn(common.opensslCli, args, { stdio: 'pipe' });
  client.stdout.pipe(process.stdout);
  client.stderr.pipe(process.stderr);
  client.stderr.setEncoding('utf8');
  client.stderr.on('data', (data) => stderr += data);

  client.once('exit', common.mustCall(function(exitCode) {
    assert.strictEqual(exitCode, 1);
    server.close();
  }));
});

server.on('tlsClientError', (err) => errors.push(err));

process.on('exit', function() {
  if (/unknown option -ssl3/.test(stderr)) {
    common.printSkipMessage('`openssl s_client -ssl3` not supported.');
  } else {
    assert.strictEqual(errors.length, 1);
    // OpenSSL 1.0.x and 1.1.x report invalid client versions differently.
    assert(/:wrong version number/.test(errors[0].message) ||
           /:version too low/.test(errors[0].message));
  }
});
