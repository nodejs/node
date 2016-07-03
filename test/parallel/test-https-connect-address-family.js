'use strict';
const common = require('../common');
if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}

if (!common.hasIPv6) {
  common.skip('no IPv6 support');
  return;
}

const assert = require('assert');
const https = require('https');

const ciphers = 'AECDH-NULL-SHA';
https.createServer({ ciphers }, common.mustCall(function(req, res) {
  this.close();
  res.end();
})).listen(common.PORT, '::1', common.mustCall(function() {
  const options = {
    host: '::1',
    port: common.PORT,
    family: 6,
    ciphers,
    rejectUnauthorized: false,
  };
  // Will fail with ECONNREFUSED if the address family is not honored.
  https.get(options, common.mustCall(common.mustCall(function() {
    assert.strictEqual('::1', this.socket.remoteAddress);
    this.destroy();
  })));
}));
