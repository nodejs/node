'use strict';
var common = require('../common');
var assert = require('assert');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}
var tls = require('tls');

var fs = require('fs');
var spawn = require('child_process').spawn;

if (common.opensslCli === false) {
  common.skip('node compiled without OpenSSL CLI.');
  return;
}

var cert = fs.readFileSync(common.fixturesDir + '/test_cert.pem');
var key = fs.readFileSync(common.fixturesDir + '/test_key.pem');
var server = tls.createServer({ cert: cert, key: key }, common.fail);
var errors = [];
var stderr = '';

server.listen(0, '127.0.0.1', function() {
  var address = this.address().address + ':' + this.address().port;
  var args = ['s_client',
              '-no_ssl2',
              '-ssl3',
              '-no_tls1',
              '-no_tls1_1',
              '-no_tls1_2',
              '-connect', address];

  // for the performance and stability issue in s_client on Windows
  if (common.isWindows)
    args.push('-no_rand_screen');

  var client = spawn(common.opensslCli, args, { stdio: 'pipe' });
  client.stdout.pipe(process.stdout);
  client.stderr.pipe(process.stderr);
  client.stderr.setEncoding('utf8');
  client.stderr.on('data', (data) => stderr += data);

  client.once('exit', common.mustCall(function(exitCode) {
    assert.equal(exitCode, 1);
    server.close();
  }));
});

server.on('tlsClientError', (err) => errors.push(err));

process.on('exit', function() {
  if (/unknown option -ssl3/.test(stderr)) {
    common.skip('`openssl s_client -ssl3` not supported.');
  } else {
    assert.equal(errors.length, 1);
    assert(/:wrong version number/.test(errors[0].message));
  }
});
