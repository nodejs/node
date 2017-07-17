'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const tls = require('tls');
const fixtures = require('../common/fixtures');

const cert = fixtures.readSync('test_cert.pem');
const key = fixtures.readSync('test_key.pem');

const conn = tls.connect({cert, key, port: 0}, common.mustNotCall());
conn.on('error', function() {
});
assert.doesNotThrow(function() {
  conn.destroy();
});
