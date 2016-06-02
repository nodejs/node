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

var cert = fs.readFileSync(path.join(common.fixturesDir, 'test_cert.pem'));
var key = fs.readFileSync(path.join(common.fixturesDir, 'test_key.pem'));

var conn = tls.connect({cert: cert, key: key, port: common.PORT}, function() {
  assert.ok(false); // callback should never be executed
});
conn.on('error', function() {
});
assert.doesNotThrow(function() {
  conn.destroy();
});
