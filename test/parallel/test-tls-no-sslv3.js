'use strict';
const common = require('../common');
const assert = require('assert');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}
const tls = require('tls');

const fs = require('fs');
const spawn = require('child_process').spawn;

if (common.opensslCli === false) {
  common.skip('node compiled without OpenSSL CLI.');
  return;
}

const cert = fs.readFileSync(common.fixturesDir + '/test_cert.pem');
const key = fs.readFileSync(common.fixturesDir + '/test_key.pem');
const server = tls.createServer({ cert: cert, key: key }, common.mustNotCall());
const errors = [];
let stderr = '';

server.listen(0, '127.0.0.1', function() {
  const address = this.address().address + ':' + this.address().port;
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
    common.skip('`openssl s_client -ssl3` not supported.');
  } else {
    assert.strictEqual(errors.length, 1);
    assert(/:wrong version number/.test(errors[0].message));
  }
});
