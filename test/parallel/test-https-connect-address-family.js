'use strict';
const common = require('../common');
if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}

const assert = require('assert');
const https = require('https');

if (!common.hasIPv6) {
  common.skip('no IPv6 support');
  return;
}

const ciphers = 'AECDH-NULL-SHA';
https.createServer({ ciphers }, function(req, res) {
  this.close();
  res.end();
}).listen(common.PORT, '::1', function() {
  const options = {
    host: 'localhost',
    port: common.PORT,
    family: 6,
    ciphers: ciphers,
    rejectUnauthorized: false,
  };
  // Will fail with ECONNREFUSED if the address family is not honored.
  https.get(options, common.mustCall(function() {
    assert.strictEqual('::1', this.socket.remoteAddress);
    this.destroy();
  }));
});
