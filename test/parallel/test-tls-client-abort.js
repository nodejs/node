'use strict';
const common = require('../common');
const assert = require('assert');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}
const tls = require('tls');

const fs = require('fs');
const path = require('path');

const cert = fs.readFileSync(path.join(common.fixturesDir, 'test_cert.pem'));
const key = fs.readFileSync(path.join(common.fixturesDir, 'test_key.pem'));

const conn = tls.connect({cert: cert, key: key, port: common.PORT}, function() {
  assert.ok(false); // callback should never be executed
});
conn.on('error', function() {
});
assert.doesNotThrow(function() {
  conn.destroy();
});
