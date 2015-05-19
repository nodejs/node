'use strict';
var common = require('../common');
var assert = require('assert');

if (!common.hasCrypto) {
  console.log('1..0 # Skipped: missing crypto');
  process.exit();
}
var tls = require('tls');

var fs = require('fs');
var spawn = require('child_process').spawn;

if (common.opensslCli === false) {
  console.error('Skipping because openssl command cannot be executed');
  process.exit(0);
}

var cert = fs.readFileSync(common.fixturesDir + '/test_cert.pem');
var key = fs.readFileSync(common.fixturesDir + '/test_key.pem');
var server = tls.createServer({ cert: cert, key: key }, assert.fail);

server.listen(common.PORT, '127.0.0.1', function() {
  var address = this.address().address + ':' + this.address().port;
  var args = ['s_client',
              '-no_ssl2',
              '-ssl3',
              '-no_tls1',
              '-no_tls1_1',
              '-no_tls1_2',
              '-connect', address];
  var client = spawn(common.opensslCli, args, { stdio: 'inherit' });
  client.once('exit', common.mustCall(function(exitCode) {
    assert.equal(exitCode, 1);
    server.close();
  }));
});

server.once('clientError', common.mustCall(function(err, conn) {
  assert(/:wrong version number/.test(err.message));
}));
