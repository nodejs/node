'use strict';
var common = require('../common');
var assert = require('assert');

if (!common.hasCrypto) {
  console.log('1..0 # Skipped: missing crypto');
  return;
}
var tls = require('tls');

var fs = require('fs');
var path = require('path');

// https://github.com/joyent/node/issues/1218
// uncatchable exception on TLS connection error
(function() {
  var cert = fs.readFileSync(path.join(common.fixturesDir, 'test_cert.pem'));
  var key = fs.readFileSync(path.join(common.fixturesDir, 'test_key.pem'));

  var errorEmitted = false;

  process.on('exit', function() {
    assert.ok(errorEmitted);
  });

  var conn = tls.connect({cert: cert, key: key, port: common.PORT}, function() {
    assert.ok(false); // callback should never be executed
  });

  conn.on('error', function() {
    errorEmitted = true;
  });
})();

// SSL_accept/SSL_connect error handling
(function() {
  var cert = fs.readFileSync(path.join(common.fixturesDir, 'test_cert.pem'));
  var key = fs.readFileSync(path.join(common.fixturesDir, 'test_key.pem'));

  var errorEmitted = false;

  process.on('exit', function() {
    assert.ok(errorEmitted);
  });

  var conn = tls.connect({
    cert: cert,
    key: key,
    port: common.PORT,
    ciphers: 'rick-128-roll'
  }, function() {
    assert.ok(false); // callback should never be executed
  });

  conn.on('error', function() {
    errorEmitted = true;
  });
})();
