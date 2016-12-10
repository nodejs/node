'use strict';
var common = require('../common');
var assert = require('assert');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}
var tls = require('tls');

var fs = require('fs');
var path = require('path');

// https://github.com/joyent/node/issues/1218
// uncatchable exception on TLS connection error
{
  const cert = fs.readFileSync(path.join(common.fixturesDir, 'test_cert.pem'));
  const key = fs.readFileSync(path.join(common.fixturesDir, 'test_key.pem'));

  const options = {cert: cert, key: key, port: common.PORT};
  const conn = tls.connect(options, common.fail);

  conn.on('error', common.mustCall(function() {}));
}

// SSL_accept/SSL_connect error handling
{
  const cert = fs.readFileSync(path.join(common.fixturesDir, 'test_cert.pem'));
  const key = fs.readFileSync(path.join(common.fixturesDir, 'test_key.pem'));

  const conn = tls.connect({
    cert: cert,
    key: key,
    port: common.PORT,
    ciphers: 'rick-128-roll'
  }, function() {
    assert.ok(false); // callback should never be executed
  });

  conn.on('error', common.mustCall(function() {}));
}
